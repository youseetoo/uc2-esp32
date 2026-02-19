#include "CanIsoTp.hpp"
#include "../can_controller.h"
#include <esp_system.h>      // For esp_get_free_heap_size()
#include <esp_heap_caps.h>   // For heap_caps_get_largest_free_block()

// use can_controller namespace for debugState
using namespace can_controller;

static SemaphoreHandle_t canIsoTpSemaphore = nullptr;

// Already declared in ESP32-TWAI-CAN.hpp
/*
typedef struct {
    uint32_t txId;      // Transmit CAN ID
    uint32_t rxId;      // Receive CAN ID
    uint8_t *data;      // Pointer to data buffer
    uint16_t len;       // Data length
    uint8_t seqId;      // Sequence ID for consecutive frames, meaning the number of the frame in the sequence
    uint8_t fcStatus;   // Flow control status
    uint8_t blockSize;  // Block size for flow control
    uint8_t separationTimeMin; // Minimum separation time
    uint8_t cantpState; // Current ISO-TP state
} pdu_t;
*/

// constructor
CanIsoTp::CanIsoTp()
{
    if (!canIsoTpSemaphore)
    {
        canIsoTpSemaphore = xSemaphoreCreateMutex();
    }
}
// destructor
CanIsoTp::~CanIsoTp()
{
}

bool CanIsoTp::begin(long baudRate, int8_t txPin, int8_t rxPin)
{
    ESP32CanTwai.setSpeed(ESP32Can.convertSpeed(baudRate));
    ESP32CanTwai.setPins(txPin, rxPin);
    return ESP32CanTwai.begin();
}

void CanIsoTp::end()
{
    ESP32CanTwai.end();
}

int CanIsoTp::send(pdu_t *pdu)
{
    uint8_t ret = 0;
    uint8_t bs = false;
    uint32_t _timerFCWait = 0;
    uint16_t _bsCounter = 0;
    // can_controller::debugState = true;
    if (can_controller::debugState) log_i("Acquiring ISO-TP semaphore for send");
    if (xSemaphoreTake(canIsoTpSemaphore, portMAX_DELAY) == pdTRUE)
    {
        if (can_controller::debugState) log_i("Starting ISO-TP send, len: %d to ID: %d, with rxId %d", pdu->len, pdu->txId, pdu->rxId);
        pdu->cantpState = CANTP_SEND;
        while (pdu->cantpState != CANTP_IDLE && pdu->cantpState != CANTP_ERROR)
        {
            if (can_controller::debugState) log_i("State: %d", pdu->cantpState);
            bs = false;
            switch (pdu->cantpState)
            {
            case CANTP_SEND: // 1
                // If data fits in a single frame (<= 7 bytes)
                if (pdu->len <= 7)
                {
                    ret = send_SingleFrame(pdu);
                    pdu->cantpState = CANTP_IDLE;
                    if (can_controller::debugState) log_i("Single Frame sent: %d", ret);
                }
                else
                {
                    ret = send_FirstFrame(pdu);
                    pdu->cantpState = CANTP_WAIT_FIRST_FC;
                    _timerFCWait = millis();
                    if (can_controller::debugState) log_i("First Frame sent: %d", ret);
                }
                break;

            case CANTP_WAIT_FIRST_FC: // 2
                if (can_controller::debugState) log_i("Waiting for first FC");
            case CANTP_WAIT_FC:       // 3
                // Check for timeout
                if ((millis() - _timerFCWait) >= TIMEOUT_FC)
                {
                    pdu->cantpState = CANTP_IDLE;
                    ret = 1; // Timeout
                    if (can_controller::debugState) log_i("Timeout waiting for FC");
                }
                CanFrame frame;

                // Try reading a frame from TWAI
                if (ESP32CanTwai.readFrame(&frame, TIMEOUT_READ))
                {
                    if (can_controller::debugState) log_i("Frame received: %d, rxId %d, len %d", frame.identifier, pdu->rxId, frame.data_length_code);
                    if (frame.identifier == pdu->txId && frame.data_length_code > 0)
                    {
                        if (can_controller::debugState) log_i("Data length: %d", frame.data_length_code);
                        // Check if it's a Flow Control frame
                        if ((frame.data[0] & 0xF0) == N_PCItypeFC)
                        {
                            if (can_controller::debugState) log_i("FC frame received");
                            if (receive_FlowControlFrame(pdu, &frame) == 0)
                            {
                                if (can_controller::debugState) log_i("FC received successfully");
                                // Received FC successfully, now we can continue sending CF
                                pdu->cantpState = CANTP_SEND_CF;
                            }
                            else
                            {
                                if (can_controller::debugState) log_i("Error in FC frame");
                                // Error in FC frame
                                pdu->cantpState = CANTP_IDLE;
                                ret = 1;
                            }
                        }
                    }
                }
                break;

            case CANTP_SEND_CF: // 4 Send consecutive frames
                // BS = 0 send everything.
                if (can_controller::debugState) log_i("Sending Consecutive Frames");
                if (pdu->blockSize == 0)
                {
                    _bsCounter = 4095;

                    bs = false;
                    while (pdu->len > 7 && _bsCounter > 0)
                    {
                        // Use adaptive separation time from can_controller namespace
                        // In FreeRTOS tasks, use vTaskDelay to yield to other tasks
                        vTaskDelay(pdMS_TO_TICKS(can_controller::separationTimeMin));

                        if (!(ret = send_ConsecutiveFrame(pdu)))
                        {
                            if (pdu->len == 0)
                                pdu->cantpState = CANTP_IDLE;
                        } // End if
                    } // End while
                    if (pdu->len <= 7 && _bsCounter > 0 && pdu->cantpState == CANTP_SEND_CF) // Last block. /!\ bsCounter can be 0.
                    {
                        if (can_controller::debugState) log_i("Sending Last block");
                        vTaskDelay(pdMS_TO_TICKS(can_controller::separationTimeMin));
                        ret = send_ConsecutiveFrame(pdu);
                        pdu->cantpState = CANTP_IDLE;
                    } // End if
                }

                // BS != 0, send by blocks.
                else
                {
                    if (can_controller::debugState) log_i("Block size: %d", pdu->blockSize);
                    bs = false;
                    _bsCounter = pdu->blockSize;
                    while (pdu->len > 7 && !bs)
                    {
                        vTaskDelay(pdMS_TO_TICKS(can_controller::separationTimeMin));
                        if (!(ret = send_ConsecutiveFrame(pdu)))
                        {
                            if (_bsCounter == 0 && pdu->len > 0)
                            {
                                pdu->cantpState = CANTP_WAIT_FC;
                                bs = true;
                                _timerFCWait = millis();
                            }
                            else if (pdu->len == 0)
                            {
                                pdu->cantpState = CANTP_IDLE;
                                bs = true;
                            }
                        } // End if
                    } // End while
                    if (pdu->len <= 7 && !bs) // Last block.
                    {
                        if (can_controller::debugState) log_i("Last block");
                        vTaskDelay(pdMS_TO_TICKS(can_controller::separationTimeMin));
                        ret = send_ConsecutiveFrame(pdu);
                        pdu->cantpState = CANTP_IDLE;
                    } // End if
                }
                break;

            case CANTP_IDLE:
                // Should not be here, but just in case
                if (can_controller::debugState) log_i("Already idle");
                break;
            default:
                // Do nothing, just exit
                if (can_controller::debugState) log_i("Idle");
                break;
            }
        }

        pdu->cantpState = CANTP_IDLE;
        if (can_controller::debugState) log_i("Return: %d", ret);
        xSemaphoreGive(canIsoTpSemaphore);
    }
    else{
        if (can_controller::debugState) log_i("Could not take semaphore");
    }
    return ret;
}

int CanIsoTp::receive(pdu_t *rxpdu, uint32_t timeout)
{
    uint8_t ret = 1;
    uint8_t N_PCItype = 0;
    uint32_t _timerSession = millis();
    if (xSemaphoreTake(canIsoTpSemaphore, portMAX_DELAY) == pdTRUE)
    {
        rxpdu->cantpState = CANTP_IDLE;

        while (rxpdu->cantpState != CANTP_END && rxpdu->cantpState != CANTP_ERROR)
        {
            // if (can_controller::debugState) log_i("State in receive: %d", rxpdu->cantpState);
            if (millis() - _timerSession >= TIMEOUT_SESSION)
            {
                if (can_controller::debugState) log_i("Session timeout");
                ret = 1; // Session timeout
                break;
            }
            CanFrame frame;

            if (ESP32CanTwai.readFrame(&frame, timeout)) // TODO: As far as I understand, this pulls messages from the internal queue, so we should not need to wait for the timeout here too long
            {
                //if (can_controller::debugState) log_i("Frame.identifier: %d, rxId: %d, txId: %d", frame.identifier, rxpdu->rxId, rxpdu->txId);
                // if 0 we accept all frames (i.e. broadcasting) - this we do by overwriting the rxId
                // frame.identifier is the ID to which the message was sent
                if (rxpdu->rxId == 0) // Broadcast: accept all frames // TODO: not implemented yet
                {
                    if (can_controller::debugState) log_i("Broadcasting");
                    rxpdu->rxId = frame.identifier; // frame.identifier is the ID to which the message was sent
                }
                if (frame.identifier == rxpdu->rxId) // we are listening to frame ids with the device's current ID
                {
                    // Extract N_PCItype
                    if (frame.data_length_code > 0)
                    {
                        N_PCItype = (frame.data[0] & 0xF0);
                        switch (N_PCItype)
                        {
                        case N_PCItypeSF: // 0x00
                            if (can_controller::debugState) log_i("SF received");
                            ret = receive_SingleFrame(rxpdu, &frame);
                            _timerSession = millis(); // Reset session timer
                            break;
                        case N_PCItypeFF: // 0x10
                            ret = receive_FirstFrame(rxpdu, &frame);
                            _timerSession = millis(); // Reset session timer
                            break;
                        case N_PCItypeFC: // 0x30
                            if (can_controller::debugState) log_i("FC received - but it doesn'T make sense!");
                            // TODO: This does not make any sense as the receiver won'T receive a flow control frame
                            // in case this case is activated, maybe the last frame from the receiver is still in the buffer, we can ignore it anyway
                            // ret = receive_FlowControlFrame(rxpdu, &frame);
                            break;
                        case N_PCItypeCF: // 0x20
                            //if (can_controller::debugState) log_i("CF received");
                            ret = receive_ConsecutiveFrame(rxpdu, &frame);
                            _timerSession = millis(); // Reset session timer // TODO: Check if this is correct
                            break;
                        default:
                            if (can_controller::debugState) log_i("Unrecognized PCI");
                            // Unrecognized PCI, do nothing or set error
                            break;
                        }
                    }
                }
                else
                {
                    if (can_controller::debugState) log_i("Frame ID mismatch");
                }
            }
            else
            {
                // FIXME: there are still some packages not read at the end... not sure why. Timeout issues?
                if (rxpdu->cantpState == CANTP_IDLE)
                    break;
                else
                    if (can_controller::debugState) log_i("No frame received and not idling");
            }
        }
        // if (can_controller::debugState) log_i("Return: %d", ret);
        xSemaphoreGive(canIsoTpSemaphore);
    }
    return ret;
}

int CanIsoTp::send_SingleFrame(pdu_t *pdu)
{
    if (can_controller::debugState) log_i("Sending SF");
    CanFrame frame = {0};
    frame.identifier = pdu->txId; //
    frame.extd = 0;
    frame.data_length_code = 1 + pdu->len;

    frame.data[0] = N_PCItypeSF | pdu->len; // PCI: Single Frame + Data Length
    memcpy(&frame.data[1], pdu->data, pdu->len);

    bool mWriteFrameResult = ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
    pdu->cantpState = CANTP_END;
    return mWriteFrameResult;
}

int CanIsoTp::send_FirstFrame(pdu_t *pdu)
{
    if (can_controller::debugState) log_i("Sending FF");
    CanFrame frame = {0};
    frame.identifier = pdu->txId;
    frame.extd = 0;
    frame.data_length_code = 8;

    frame.data[0] = N_PCItypeFF | ((pdu->len >> 8) & 0x0F); // PCI: First Frame (upper nibble is DL)
    frame.data[1] = pdu->len & 0xFF;                        // Lower byte of data length
    memcpy(&frame.data[2], pdu->data, 6);                   // Copy first 6 bytes of data

    pdu->data += 6; // Advance data pointer
    pdu->len -= 6;  // Remaining data size
    pdu->seqId = 1; // Sequence ID for consecutive frames

    return ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
}

int CanIsoTp::send_ConsecutiveFrame(pdu_t *pdu)
{
    CanFrame frame = {0};
    frame.identifier = pdu->txId;
    frame.extd = 0;
    frame.data_length_code = 8;

    //if (can_controller::debugState) log_i("Sending CF: (seqId): %d", pdu->seqId);
    frame.data[0] = N_PCItypeCF | (pdu->seqId & 0x0F); // PCI: Consecutive Frame with sequence number
    uint8_t sizeToSend = (pdu->len > 7) ? 7 : pdu->len;

    memcpy(&frame.data[1], pdu->data, sizeToSend);
    pdu->data += sizeToSend;
    pdu->len -= sizeToSend;
    pdu->seqId = (pdu->seqId + 1) % 16; // Sequence number wraps after 15
    return ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
}

int CanIsoTp::send_FlowControlFrame(pdu_t *pdu)
{
    if (can_controller::debugState) log_i("Sending FC: frame.identifier = pdu->txId (%u); we are listening on pdu->rxId (%u), STmin=%dms", pdu->txId, pdu->rxId, can_controller::separationTimeMin);
    CanFrame frame = {0};
    frame.identifier = pdu->rxId;
    frame.extd = 0;
    frame.data_length_code = 3;

    frame.data[0] = N_PCItypeFC | CANTP_FLOWSTATUS_CTS; // PCI: Flow Control + CTS
    frame.data[1] = pdu->blockSize;                     // Block Size
    frame.data[2] = can_controller::separationTimeMin;  // Use global adaptive separation time

    return ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
}

int CanIsoTp::receive_SingleFrame(pdu_t *pdu, CanFrame *frame)
{
    if (can_controller::debugState) log_i("Single Frame received");
    uint8_t dataLength = frame->data[0] & 0x0F;
    // if data is empty, allocate memory
    if (pdu->data == nullptr)
    {
        pdu->data = (uint8_t *)malloc(frame->data_length_code - 1);
        if (!pdu->data)
        {
            // Could not allocate; set an error
            pdu->cantpState = CANTP_ERROR;
            return 1;
        }
    }
    pdu->len = frame->data[0] & 0x0F; // Extract data length

    if (pdu->len > (frame->data_length_code - 1))
    {
        if (can_controller::debugState) log_e("Error: Data length exceeds frame length");
        pdu->cantpState = CANTP_ERROR;
        return 1;
    }

    // we have to perform a malloc here, as we don't know the datatype to cast on default
    // Allocate enough space for the entire payload and cast it later
    if (can_controller::debugState) {
        log_i("Allocating memory for pdu->data (single frame), size=%u, heap free=%lu, largest block=%lu", 
              pdu->len, (unsigned long)esp_get_free_heap_size(), (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    }
    pdu->data = (uint8_t *)malloc(pdu->len);
    if (!pdu->data)
    {
        // Could not allocate; set an error - log heap state for debugging
        log_e("Could not allocate memory for data: requested=%u, heap free=%lu, largest block=%lu", 
              pdu->len, (unsigned long)esp_get_free_heap_size(), (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        pdu->cantpState = CANTP_ERROR;
        return 1;
    }
    memcpy(pdu->data, &frame->data[1], pdu->len);
    pdu->cantpState = IsoTpState::CANTP_END; // Transmission complete
    return 0;
}

int CanIsoTp::receive_FirstFrame(pdu_t *pdu, CanFrame *frame)
{
    
    // check for invalid data length code - has to be at least 8?
    if (frame->data_length_code < 8) {
        if (can_controller::debugState) log_e("Invalid First Frame length code: %d", frame->data_length_code);
        pdu->cantpState = CANTP_ERROR;
        return 1;
    }
    // Determine total length from FF
    uint16_t totalLen = ((frame->data[0] & 0x0F) << 8) | frame->data[1]; // len is 16bits: 0000 XXXX. 0000 0000.
    pdu->len = totalLen;
    _rxRestBytes = pdu->len;

    if (can_controller::debugState) log_i("First Frame received, txID %d, rxID %d, size: %i, totalLen: %i", pdu->txId, pdu->rxId, frame->data_length_code, totalLen);

    // If totalLen < 6, we cannot safely copy 6 bytes.
    if (totalLen < 6) {
        if (can_controller::debugState) log_e("Invalid totalLen < 6 in First Frame: %d", totalLen);
        pdu->cantpState = CANTP_ERROR;
        return 1;
    }

        // introduce a special case where we don't know the datatype to cast on default, so we create the array and cast it later
        // Free existing buffer if needed
        /*
        if (pdu->data) 
        {
            free(pdu->data); // => assert failed: heap_caps_free heap_caps.c:381 (heap != NULL && "free() target pointer is outside heap areas")
            pdu->data = nullptr;
            if (can_controller::debugState) log_i("Freeing existing buffer");
        }
        */
       
        // Allocate enough space for the entire payload and cast it later
        if (can_controller::debugState) {
            log_i("Allocating memory for pdu->data, size=%u, heap free=%lu, largest block=%lu", 
                  totalLen, (unsigned long)esp_get_free_heap_size(), (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        }
        pdu->data = (uint8_t *)malloc(totalLen);
        if (!pdu->data)
        {
            // Could not allocate; set an error - log heap state for debugging
            log_e("Could not allocate memory for data: requested=%u, heap free=%lu, largest block=%lu", 
                  totalLen, (unsigned long)esp_get_free_heap_size(), (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            pdu->cantpState = CANTP_ERROR;
            return 1;
        }
    // Copy the first 6 bytes
    memcpy(pdu->data, &frame->data[2], 6); // Copy first 6 bytes
    _rxRestBytes -= 6;
    pdu->seqId = 1;                                // Start sequence ID
    pdu->cantpState = IsoTpState::CANTP_WAIT_DATA; // Awaiting consecutive frames
    if (can_controller::debugState) log_i("Sending Flow Control FC");

    return send_FlowControlFrame(pdu);
}

int CanIsoTp::receive_ConsecutiveFrame(pdu_t *pdu, CanFrame *frame)
{
    // Update the time difference and reset _timerCFWait (as in original)
    uint32_t timeDiff = millis() - _timerCFWait;
    _timerCFWait = millis();

    // The original code checks if we are waiting for data
    if (pdu->cantpState != CANTP_WAIT_DATA)
    {
        if (can_controller::debugState) log_i("receive_ConsecutiveFrame: Invalid state: %d", pdu->cantpState);
        return 0;
    }

    // Check sequence number to ensure correct order
    // ISO-TP sequence ID is 4 bits (0-15), wraps around after 15
    uint8_t frameSeqId = frame->data[0] & 0x0F;
    uint8_t expectedSeqId = pdu->seqId & 0x0F;  // Modulo 16 for comparison
    if (frameSeqId != expectedSeqId)
    {
        if (can_controller::debugState) log_i("Sequence mismatch: expected %d (frame %d), got %d", expectedSeqId, pdu->seqId, frameSeqId);
        return 1; // Sequence mismatch
    }

    // Determine how many bytes to copy
    // Copy up to 7 bytes (or fewer if _rxRestBytes < 7)
    uint8_t sizeToCopy = (_rxRestBytes > 7) ? 7 : _rxRestBytes;
    // Calculate offset based on frame number (not seqId which wraps)
    // First CF (seqId=1) goes at offset 6, second CF (seqId=2) at offset 13, etc.
    uint16_t offset = 6 + 7 * (pdu->seqId - 1);
    memcpy(pdu->data + offset, &frame->data[1], sizeToCopy);

    // Decrease the remaining bytes count
    _rxRestBytes -= sizeToCopy;
    if (can_controller::debugState) log_i("Consecutive Frame received, state: %d, frameNum: %d, seqID incoming: %d with size %d, Rest bytes: %d", pdu->cantpState, pdu->seqId, frameSeqId, frame->data_length_code - 1, _rxRestBytes);

    // If we've received all data, update state to CANTP_END
    if (_rxRestBytes <= 0)
    {
        if (can_controller::debugState) log_i("All data received");
        pdu->cantpState = CANTP_END;
    }

    // Increment frame counter (not modulo - we need to track total frames for offset calculation)
    pdu->seqId++;
    return 0;
}

int CanIsoTp::receive_FlowControlFrame(pdu_t *pdu, CanFrame *frame)
{
    if (can_controller::debugState) log_i("Flow Control Frame received");
    uint8_t flowStatus = frame->data[0] & 0x0F;
    int ret = 0;

    if (pdu->cantpState != CANTP_WAIT_DATA && pdu->cantpState != CANTP_WAIT_FIRST_FC && pdu->cantpState != CANTP_WAIT_FC)
    {
        if (can_controller::debugState) log_e("receive_FlowControlFrame: Invalid state: %d", pdu->cantpState);
        return 0;
    }

    // Update blockSize and separationTimeMin only when waiting for the first FC
    if (pdu->cantpState == CANTP_WAIT_FIRST_FC) // 2
    {
        pdu->blockSize = frame->data[1];
        pdu->separationTimeMin = frame->data[2]; // 0x7F, by defaul 127ms
        if (can_controller::debugState) log_i("Updating BS and STmin to: %d, %d", pdu->blockSize, pdu->separationTimeMin);
        // Ensure STmin is within allowed range
        if ((pdu->separationTimeMin > 127 && pdu->separationTimeMin < 241) || (pdu->separationTimeMin > 249))
        {
            if (can_controller::debugState) log_i("STmin out of range, setting to 30ms");
            pdu->separationTimeMin = 30; // Default to 30ms if out-of-range
        }
    }

    switch (flowStatus)
    {
    case CANTP_FLOWSTATUS_CTS: // Continue to send
        if (can_controller::debugState) log_i("CTS (Continue to send) frame received");
        pdu->cantpState = CANTP_SEND_CF;
        break;

    case CANTP_FLOWSTATUS_WT: // Wait
        _receivedFCWaits++;
        if (can_controller::debugState) log_i("WT (Wait) frame received, count: %d", _receivedFCWaits);
        if (_receivedFCWaits >= WFTmax)
        {
            // Too many waits, abort transmission
            _receivedFCWaits = 0;
            pdu->cantpState = CANTP_IDLE;
            ret = 1;
            if (can_controller::debugState) log_i("WFTmax exceeded, aborting transmission.");
        }
        // If WFTmax not reached, remain in current waiting state
        break;

    case CANTP_FLOWSTATUS_OVFL: // Overflow
    default:
        // Any unrecognized flow status leads to abort
        pdu->cantpState = CANTP_IDLE;
        ret = 1;
        if (can_controller::debugState) log_i("Overflow or unknown flow status, aborting.");
        break;
    }

    return ret;
}

void CanIsoTp::setSeparationTimeMin(uint8_t stMin){
    // Update the global separation time in can_controller namespace
    // This affects all ISO-TP transmissions
    can_controller::separationTimeMin = stMin;
    if (can_controller::debugState) log_i("Set separationTimeMin to %dms", stMin);
}
// Multi-address receive function
int CanIsoTp::receive(pdu_t *rxpdu, uint8_t *rxIDs, uint8_t numIDs, uint32_t timeout)
{
    uint8_t ret = 1;
    uint8_t N_PCItype = 0;
    uint32_t _timerSession = millis();
    if (xSemaphoreTake(canIsoTpSemaphore, portMAX_DELAY) == pdTRUE)
    {
        rxpdu->cantpState = CANTP_IDLE;

        while (rxpdu->cantpState != CANTP_END && rxpdu->cantpState != CANTP_ERROR)
        {
            if (millis() - _timerSession >= TIMEOUT_SESSION)
            {
                if (can_controller::debugState) log_i("Session timeout");
                ret = 1; // Session timeout
                break;
            }
            CanFrame frame;

            if (ESP32CanTwai.readFrame(&frame, timeout))
            {
                if (can_controller::debugState) log_i("Frame.identifier: %d, checking against %d addresses", frame.identifier, numIDs);
                
                // Check if frame matches any of the target IDs
                bool frameMatches = false;
                for (uint8_t i = 0; i < numIDs; i++)
                {
                    if (frame.identifier == rxIDs[i])
                    {
                        frameMatches = true;
                        rxpdu->rxId = frame.identifier; // Set the actual received ID
                        if (can_controller::debugState) log_i("Frame matches ID %d", rxIDs[i]);
                        break;
                    }
                }
                
                if (frameMatches)
                {
                    
                    // Extract N_PCItype
                    if (frame.data_length_code > 0)
                    {
                        N_PCItype = (frame.data[0] & 0xF0);
                        switch (N_PCItype)
                        {
                            case N_PCItypeSF: // 0x00
                            if (can_controller::debugState) log_i("SF received");
                            ret = receive_SingleFrame(rxpdu, &frame);
                            break;
                            case N_PCItypeFF: // 0x10
                            ret = receive_FirstFrame(rxpdu, &frame);
                            // resetting session timer
                            _timerSession = millis(); // TODO: check if this is needed here, as we are not sending anything yet
                            break;
                        case N_PCItypeFC: // 0x30
                            if (can_controller::debugState) log_i("FC received - but it doesn't make sense!");
                            break;
                        case N_PCItypeCF: // 0x20
                            if (can_controller::debugState) log_i("CF received");
                            ret = receive_ConsecutiveFrame(rxpdu, &frame);
                            _timerSession = millis(); // TODO: check if needed as we continously receive frames and should reset the timer anyway
                            break;
                        default:
                            if (can_controller::debugState) log_i("Unrecognized PCI");
                            break;
                        }
                    }
                }
                else
                {
                    if (can_controller::debugState) log_i("Frame ID %d doesn't match any target IDs, ignoring", frame.identifier);
                }
            }
        }

        xSemaphoreGive(canIsoTpSemaphore);
    }
    return ret;
}
