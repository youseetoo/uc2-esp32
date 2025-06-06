#include "CanIsoTp.hpp"


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

    if (xSemaphoreTake(canIsoTpSemaphore, portMAX_DELAY) == pdTRUE)
    {
        pdu->cantpState = CANTP_SEND;
        while (pdu->cantpState != CANTP_IDLE && pdu->cantpState != CANTP_ERROR)
        {
            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("State: %d", pdu->cantpState);
            bs = false;
            switch (pdu->cantpState)
            {
            case CANTP_SEND: // 1
                // If data fits in a single frame (<= 7 bytes)
                if (pdu->len <= 7)
                {
                    ret = send_SingleFrame(pdu);
                    pdu->cantpState = CANTP_IDLE;
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Single Frame sent: %d", ret);
                }
                else
                {
                    ret = send_FirstFrame(pdu);
                    pdu->cantpState = CANTP_WAIT_FIRST_FC;
                    _timerFCWait = millis();
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("First Frame sent: %d", ret);
                }
                break;

            case CANTP_WAIT_FIRST_FC: // 2
            case CANTP_WAIT_FC:       // 3
                // Check for timeout
                if ((millis() - _timerFCWait) >= TIMEOUT_FC)
                {
                    pdu->cantpState = CANTP_IDLE;
                    ret = 1; // Timeout
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Timeout waiting for FC");
                }
                CanFrame frame;

                // Try reading a frame from TWAI
                if (ESP32CanTwai.readFrame(&frame, TIMEOUT_READ))
                {
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Frame received: %d, rxId %d, len %d", frame.identifier, pdu->rxId, frame.data_length_code);
                    if (frame.identifier == pdu->txId && frame.data_length_code > 0)
                    {
                        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Data length: %d", frame.data_length_code);
                        // Check if it's a Flow Control frame
                        if ((frame.data[0] & 0xF0) == N_PCItypeFC)
                        {
                            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("FC frame received");
                            if (receive_FlowControlFrame(pdu, &frame) == 0)
                            {
                                if (pinConfig.DEBUG_CAN_ISO_TP) log_i("FC received successfully");
                                // Received FC successfully, now we can continue sending CF
                                pdu->cantpState = CANTP_SEND_CF;
                            }
                            else
                            {
                                if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Error in FC frame");
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
                if (pdu->blockSize == 0)
                {
                    _bsCounter = 4095;

                    bs = false;
                    while (pdu->len > 7 && _bsCounter > 0)
                    {
                        delay(pdu->separationTimeMin);

                        if (!(ret = send_ConsecutiveFrame(pdu)))
                        {
                            if (pdu->len == 0)
                                pdu->cantpState = CANTP_IDLE;
                        } // End if
                    } // End while
                    if (pdu->len <= 7 && _bsCounter > 0 && pdu->cantpState == CANTP_SEND_CF) // Last block. /!\ bsCounter can be 0.
                    {
                        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending Last block");
                        delay(pdu->separationTimeMin);
                        ret = send_ConsecutiveFrame(pdu);
                        pdu->cantpState = CANTP_IDLE;
                    } // End if
                }

                // BS != 0, send by blocks.
                else
                {
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Block size: %d", pdu->blockSize);
                    bs = false;
                    _bsCounter = pdu->blockSize;
                    while (pdu->len > 7 && !bs)
                    {
                        delay(pdu->separationTimeMin);
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
                        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Last block");
                        delay(pdu->separationTimeMin);
                        ret = send_ConsecutiveFrame(pdu);
                        pdu->cantpState = CANTP_IDLE;
                    } // End if
                }
                break;

            case CANTP_IDLE:
            default:
                // Do nothing, just exit
                if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Idle");
                break;
            }
        }

        pdu->cantpState = CANTP_IDLE;
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Return: %d", ret);
        xSemaphoreGive(canIsoTpSemaphore);
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
            // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("State in receive: %d", rxpdu->cantpState);
            if (millis() - _timerSession >= TIMEOUT_SESSION)
            {
                if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Session timeout");
                ret = 1; // Session timeout
                break;
            }
            CanFrame frame;

            if (ESP32CanTwai.readFrame(&frame, timeout)) // TODO: As far as I understand, this pulls messages from the internal queue, so we should not need to wait for the timeout here too long
            {
                if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Frame.identifier: %d, rxId: %d, txId: %d", frame.identifier, rxpdu->rxId, rxpdu->txId);
                // if 0 we accept all frames (i.e. broadcasting) - this we do by overwriting the rxId
                // frame.identifier is the ID to which the message was sent
                if (rxpdu->rxId == 0) // Broadcast: accept all frames // TODO: not implemented yet
                {
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Broadcasting");
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
                            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("SF received");
                            ret = receive_SingleFrame(rxpdu, &frame);
                            break;
                        case N_PCItypeFF: // 0x10
                            ret = receive_FirstFrame(rxpdu, &frame);
                            break;
                        case N_PCItypeFC: // 0x30
                            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("FC received - but it doesn'T make sense!");
                            // TODO: This does not make any sense as the receiver won'T receive a flow control frame
                            // in case this case is activated, maybe the last frame from the receiver is still in the buffer, we can ignore it anyway
                            // ret = receive_FlowControlFrame(rxpdu, &frame);
                            break;
                        case N_PCItypeCF: // 0x20
                            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("CF received");
                            ret = receive_ConsecutiveFrame(rxpdu, &frame);
                            break;
                        default:
                            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Unrecognized PCI");
                            // Unrecognized PCI, do nothing or set error
                            break;
                        }
                    }
                }
                else
                {
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Frame ID mismatch");
                }
            }
            else
            {
                // FIXME: there are still some packages not read at the end... not sure why. Timeout issues?
                if (rxpdu->cantpState == CANTP_IDLE)
                    break;
                else
                    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("No frame received and not idling");
            }
        }
        // if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Return: %d", ret);
        xSemaphoreGive(canIsoTpSemaphore);
    }
    return ret;
}

int CanIsoTp::send_SingleFrame(pdu_t *pdu)
{
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending SF");
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
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending FF");
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

    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending CF: (seqId): %d", pdu->seqId);
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
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending FC: frame.identifier = pdu->txId (%u); we are listening on pdu->rxId (%u)", pdu->txId, pdu->rxId);
    CanFrame frame = {0};
    frame.identifier = pdu->rxId;
    frame.extd = 0;
    frame.data_length_code = 3;

    frame.data[0] = N_PCItypeFC | CANTP_FLOWSTATUS_CTS; // PCI: Flow Control + CTS
    frame.data[1] = pdu->blockSize;                     // Block Size
    frame.data[2] = pdu->separationTimeMin;             // Separation Time

    return ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
}

int CanIsoTp::receive_SingleFrame(pdu_t *pdu, CanFrame *frame)
{
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Single Frame received");
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
        if (pinConfig.DEBUG_CAN_ISO_TP) log_e("Error: Data length exceeds frame length");
        pdu->cantpState = CANTP_ERROR;
        return 1;
    }

    // we have to perform a malloc here, as we don't know the datatype to cast on default
    // Allocate enough space for the entire payload and cast it later
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Allocating memory for pdu->data");
    pdu->data = (uint8_t *)malloc(pdu->len);
    if (!pdu->data)
    {
        // Could not allocate; set an error
        if (pinConfig.DEBUG_CAN_ISO_TP) log_e("Could not allocate memory for data");
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
        if (pinConfig.DEBUG_CAN_ISO_TP) log_e("Invalid First Frame length code: %d", frame->data_length_code);
        pdu->cantpState = CANTP_ERROR;
        return 1;
    }
    // Determine total length from FF
    uint16_t totalLen = ((frame->data[0] & 0x0F) << 8) | frame->data[1]; // len is 16bits: 0000 XXXX. 0000 0000.
    pdu->len = totalLen;
    _rxRestBytes = pdu->len;
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("First Frame received, txID %d, rxID %d, size: %i, totalLen: %i", pdu->txId, pdu->rxId, frame->data_length_code, totalLen);

    // If totalLen < 6, we cannot safely copy 6 bytes.
    if (totalLen < 6) {
        if (pinConfig.DEBUG_CAN_ISO_TP) log_e("Invalid totalLen < 6 in First Frame: %d", totalLen);
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
            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Freeing existing buffer");
        }
        */
       
        // Allocate enough space for the entire payload and cast it later
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Allocating memory for pdu->data");
        pdu->data = (uint8_t *)malloc(totalLen);
        if (!pdu->data)
        {
            // Could not allocate; set an error
            if (pinConfig.DEBUG_CAN_ISO_TP) log_e("Could not allocate memory for data");
            pdu->cantpState = CANTP_ERROR;
            return 1;
        }
    // Copy the first 6 bytes
    memcpy(pdu->data, &frame->data[2], 6); // Copy first 6 bytes
    _rxRestBytes -= 6;
    pdu->seqId = 1;                                // Start sequence ID
    pdu->cantpState = IsoTpState::CANTP_WAIT_DATA; // Awaiting consecutive frames
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sending Flow Control FC");

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
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("receive_ConsecutiveFrame: Invalid state: %d", pdu->cantpState);
        return 0;
    }

    // Check sequence number to ensure correct order
    uint8_t seqId = frame->data[0] & 0x0F;
    if (seqId != pdu->seqId)
    {
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Sequence mismatch");
        return 1; // Sequence mismatch
    }

    // Determine how many bytes to copy
    // Copy up to 7 bytes (or fewer if _rxRestBytes < 7)
    uint8_t sizeToCopy = (_rxRestBytes > 7) ? 7 : _rxRestBytes;
    // Original offset logic: 6 + (seqId - 1)*7
    uint16_t offset = 6 + 7 * (pdu->seqId - 1);
    memcpy(pdu->data + offset, &frame->data[1], sizeToCopy);

    // Decrease the remaining bytes count
    _rxRestBytes -= sizeToCopy;
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Consecutive Frame received, state: %d and seqID (pdu) %d, seqID incoming: %d with size %d, Rest bytes: %d", pdu->cantpState, pdu->seqId, seqId, frame->data_length_code - 1, _rxRestBytes);

    // If we've received all data, update state to CANTP_END
    if (_rxRestBytes <= 0)
    {
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("All data received");
        pdu->cantpState = CANTP_END;
    }

    // Increment sequence ID as original code does (no modulo)
    pdu->seqId++;
    return 0;
}

int CanIsoTp::receive_FlowControlFrame(pdu_t *pdu, CanFrame *frame)
{
    if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Flow Control Frame received");
    uint8_t flowStatus = frame->data[0] & 0x0F;
    int ret = 0;

    if (pdu->cantpState != CANTP_WAIT_DATA && pdu->cantpState != CANTP_WAIT_FIRST_FC && pdu->cantpState != CANTP_WAIT_FC)
    {
        if (pinConfig.DEBUG_CAN_ISO_TP) log_e("receive_FlowControlFrame: Invalid state: %d", pdu->cantpState);
        return 0;
    }

    // Update blockSize and separationTimeMin only when waiting for the first FC
    if (pdu->cantpState == CANTP_WAIT_FIRST_FC) // 2
    {
        pdu->blockSize = frame->data[1];
        pdu->separationTimeMin = frame->data[2]; // 0x7F, by defaul 127ms
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Updating BS and STmin to: %d, %d", pdu->blockSize, pdu->separationTimeMin);
        // Ensure STmin is within allowed range
        if ((pdu->separationTimeMin > 127 && pdu->separationTimeMin < 241) || (pdu->separationTimeMin > 249))
        {
            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("STmin out of range, setting to 127ms");
            pdu->separationTimeMin = 10; // Default to max 127ms if out-of-range
        }
    }

    switch (flowStatus)
    {
    case CANTP_FLOWSTATUS_CTS: // Continue to send
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("CTS (Continue to send) frame received");
        pdu->cantpState = CANTP_SEND_CF;
        break;

    case CANTP_FLOWSTATUS_WT: // Wait
        _receivedFCWaits++;
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("WT (Wait) frame received, count: %d", _receivedFCWaits);
        if (_receivedFCWaits >= WFTmax)
        {
            // Too many waits, abort transmission
            _receivedFCWaits = 0;
            pdu->cantpState = CANTP_IDLE;
            ret = 1;
            if (pinConfig.DEBUG_CAN_ISO_TP) log_i("WFTmax exceeded, aborting transmission.");
        }
        // If WFTmax not reached, remain in current waiting state
        break;

    case CANTP_FLOWSTATUS_OVFL: // Overflow
    default:
        // Any unrecognized flow status leads to abort
        pdu->cantpState = CANTP_IDLE;
        ret = 1;
        if (pinConfig.DEBUG_CAN_ISO_TP) log_i("Overflow or unknown flow status, aborting.");
        break;
    }

    return ret;
}
