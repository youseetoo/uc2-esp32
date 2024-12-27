#include "CanIsoTp.hpp"
#define N_PCItypeSF 0x00 // Single Frame
#define N_PCItypeFF 0x10 // First Frame
#define N_PCItypeCF 0x20 // Consecutive Frame
#define N_PCItypeFC 0x30 // Flow Control Frame

#define WFTmax 3 // Maximum number of wait frames

#define CANTP_FLOWSTATUS_CTS 0x00  // Continue to send
#define CANTP_FLOWSTATUS_WT 0x01   // Wait
#define CANTP_FLOWSTATUS_OVFL 0x02 // Overflow

#define PACKET_SIZE 8
#define TIMEOUT_SESSION 100
#define TIMEOUT_FC 100
#define TIMEOUT_READ 100

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

    pdu->cantpState = CANTP_SEND;
    while (pdu->cantpState != CANTP_IDLE && pdu->cantpState != CANTP_ERROR)
    {
        log_i("State: %d", pdu->cantpState);
        bs = false;
        switch (pdu->cantpState)
        {
        case CANTP_SEND:
            // If data fits in a single frame (<= 7 bytes)
            if (pdu->len <= 7)
            {
                ret = send_SingleFrame(pdu);
                pdu->cantpState = CANTP_IDLE;
                log_i("Single Frame sent: %d", ret);
            }
            else
            {
                ret = send_FirstFrame(pdu);
                pdu->cantpState = CANTP_WAIT_FIRST_FC;
                _timerFCWait = millis();
                log_i("First Frame sent: %d", ret);
            }
            break;

        case CANTP_WAIT_FIRST_FC:
            // Check for timeout
            if ((millis() - _timerFCWait) >= TIMEOUT_FC)
            {
                pdu->cantpState = CANTP_IDLE;
                ret = 1; // Timeout
                log_i("Timeout waiting for First FC");
            }
            break;

        case CANTP_WAIT_FC:
            // Check for timeout
            if ((millis() - _timerFCWait) >= TIMEOUT_FC)
            {
                pdu->cantpState = CANTP_IDLE;
                ret = 1; // Timeout
                log_i("Timeout waiting for FC");
            }
            break;

        case CANTP_SEND_CF:
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
                        pdu->seqId++;
                        pdu->data += 7;
                        pdu->len -= 7;
                        if (pdu->len == 0)
                            pdu->cantpState = CANTP_IDLE;
                    } // End if
                } // End while
                if (pdu->len <= 7 && _bsCounter > 0 && pdu->cantpState == CANTP_SEND_CF) // Last block. /!\ bsCounter can be 0.
                {
                    delay(pdu->separationTimeMin);
                    ret = send_ConsecutiveFrame(pdu);
                    pdu->cantpState = CANTP_IDLE;
                } // End if
                log_i("Consecutive Frame sent: %d", ret);
            }

            // BS != 0, send by blocks.
            else
            {
                log_i("Block size: %d", pdu->blockSize);
                bs = false;
                _bsCounter = pdu->blockSize;
                while (pdu->len > 7 && !bs)
                {
                    delay(pdu->separationTimeMin);
                    if (!(ret = send_ConsecutiveFrame(pdu)))
                    {
                        log_i("Consecutive Frame sent: %d", ret);
                        pdu->data += 7; // Update pointer.
                        pdu->len -= 7;  // 7 Bytes sended.
                        pdu->seqId++;
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
                    log_i("Last block");
                    delay(pdu->separationTimeMin);
                    ret = send_ConsecutiveFrame(pdu);
                    pdu->cantpState = CANTP_IDLE;
                } // End if
            }
            break;

        case CANTP_IDLE:
        default:
            // Do nothing, just exit
            log_i("Idle");
            break;
        }

        // If waiting for flow control, check for incoming frames
        if (pdu->cantpState == CANTP_WAIT_FIRST_FC || pdu->cantpState == CANTP_WAIT_FC)
        {
            CanFrame frame;

            // Try reading a frame from TWAI
            if (ESP32CanTwai.readFrame(&frame, TIMEOUT_READ))
            {
                log_i("Frame received: %d", frame.identifier);
                if (frame.identifier == pdu->rxId && frame.data_length_code > 0)
                {
                    log_i("Data length: %d", frame.data_length_code);
                    // Check if it's a Flow Control frame
                    if ((frame.data[0] & 0xF0) == N_PCItypeFC)
                    {
                        log_i("FC frame received");
                        if (receive_FlowControlFrame(pdu, &frame) == 0)
                        {
                            log_i("FC received successfully");
                            // Received FC successfully, now we can continue sending CF
                            pdu->cantpState = CANTP_SEND_CF;
                        }
                        else
                        {
                            log_i("Error in FC frame");
                            // Error in FC frame
                            pdu->cantpState = CANTP_IDLE;
                            ret = 1;
                        }
                    }
                }
            }
            else
            {
                // log_i("No frame received");
            }
            // If no frame is received this iteration, loop continues until timeout or receive FC
        }
    }

    pdu->cantpState = CANTP_IDLE;
    log_i("Return: %d", ret);
    return ret;
}

int CanIsoTp::receive(pdu_t *rxpdu)
{
    uint8_t ret = -1;
    uint8_t N_PCItype = 0;
    uint32_t _timerSession = millis();
    rxpdu->cantpState = CANTP_IDLE;

    while (rxpdu->cantpState != CANTP_END && rxpdu->cantpState != CANTP_ERROR)
    {
        log_i("State in receive: %d", rxpdu->cantpState);
        if (millis() - _timerSession >= TIMEOUT_SESSION)
        {
            return 1; // Session timeout
        }

        CanFrame frame;
        if (ESP32CanTwai.readFrame(&frame, TIMEOUT_READ))
        {
            log_i("Frame id received: %d, receive id: %d", frame.identifier, rxpdu->rxId);
            // if 0 we accept all frames (i.e. broadcasting) - this we do by overwriting the rxId
            if (rxpdu->rxId == 0)
            {
                rxpdu->rxId = frame.identifier;
            }
            if (frame.identifier == rxpdu->rxId)
            {
                log_i("Data length: %d", frame.data_length_code);
                // Extract N_PCItype
                if (frame.data_length_code > 0)
                {
                    N_PCItype = (frame.data[0] & 0xF0);
                    log_i("N_PCItype: %d", N_PCItype);
                    switch (N_PCItype)
                    {
                    case N_PCItypeSF: // 0x00
                        log_i("SF received");
                        ret = receive_SingleFrame(rxpdu, &frame);
                        break;
                    case N_PCItypeFF: // 0x10
                        log_i("FF received");
                        ret = receive_FirstFrame(rxpdu, &frame);
                        break;
                    case N_PCItypeFC: // 0x30
                        log_i("FC received");
                        ret = receive_FlowControlFrame(rxpdu, &frame);
                        break;
                    case N_PCItypeCF: // 0x20
                        log_i("CF received");
                        ret = receive_ConsecutiveFrame(rxpdu, &frame);
                        break;
                    default:
                        log_i("Unrecognized PCI");
                        // Unrecognized PCI, do nothing or set error
                        break;
                    }
                }
            }
            else
            {
                log_i("Frame ID mismatch");
            }
        }
        else
        {
            log_i("No frame received");
        }
    }
    log_i("Return: %d", ret);
    return ret;
}

int CanIsoTp::send_SingleFrame(pdu_t *pdu)
{
    log_i("Sending SF");
    CanFrame frame = {0};
    frame.identifier = pdu->txId;
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
    log_i("Sending FF");
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

    frame.data[0] = N_PCItypeCF | (pdu->seqId & 0x0F); // PCI: Consecutive Frame with sequence number
    uint8_t sizeToSend = (pdu->len > 7) ? 7 : pdu->len;
    log_i("Sending CF, len: %d, seqID: %d", sizeToSend, frame.data[0] & 0x0F);

    memcpy(&frame.data[1], pdu->data, sizeToSend);
    pdu->data += sizeToSend;
    pdu->len -= sizeToSend;
    pdu->seqId = (pdu->seqId + 1) % 16; // Sequence number wraps after 15

    return ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
}

int CanIsoTp::send_FlowControlFrame(pdu_t *pdu)
{
    log_i("Sending FC");
    CanFrame frame = {0};
    frame.identifier = pdu->txId;
    frame.extd = 0;
    frame.data_length_code = 3;

    frame.data[0] = N_PCItypeFC | CANTP_FLOWSTATUS_CTS; // PCI: Flow Control + CTS
    frame.data[1] = pdu->blockSize;                     // Block Size
    frame.data[2] = pdu->separationTimeMin;             // Separation Time

    return ESP32CanTwai.writeFrame(&frame) ? 0 : 1;
}

int CanIsoTp::receive_SingleFrame(pdu_t *pdu, CanFrame *frame)
{
    log_i("Single Frame received");
    pdu->len = frame->data[0] & 0x0F; // Extract data length
    memcpy(pdu->data, &frame->data[1], pdu->len);
    log_i("Data copied, %d bytes", pdu->len);
    pdu->cantpState = IsoTpState::CANTP_END; // Transmission complete
    return 0;
}

int CanIsoTp::receive_FirstFrame(pdu_t *pdu, CanFrame *frame)
{
    log_i("First Frame received");
    // Determine total length from FF
    uint16_t totalLen = ((frame->data[0] & 0x0F) << 8) | frame->data[1];
    pdu->len = totalLen;

    // introduce a special case where we don't know the datatype to cast on default, so we create the array and cast it later
    if (pdu->data == nullptr)
    {
        // Free existing buffer if needed
        if (pdu->data)
        {
            free(pdu->data);
            pdu->data = nullptr;
        }

        // Allocate enough space for the entire payload and cast it later
        pdu->data = (uint8_t *)malloc(totalLen);
        if (!pdu->data)
        {
            // Could not allocate; set an error
            pdu->cantpState = CANTP_ERROR;
            return 1;
        }
    }
    // Copy the first 6 bytes
    memcpy(pdu->data, &frame->data[2], 6);         // Copy first 6 bytes
    pdu->seqId = 1;                                // Start sequence ID
    pdu->cantpState = IsoTpState::CANTP_WAIT_DATA; // Awaiting consecutive frames
    log_i("Sending FC");

    return send_FlowControlFrame(pdu);
}

int CanIsoTp::receive_ConsecutiveFrame(pdu_t *pdu, CanFrame *frame)
{
    // Update the time difference and reset _timerCFWait (as in original)
    uint32_t timeDiff = millis() - _timerCFWait;
    _timerCFWait = millis();

    // The original code checks if we are waiting for data
    if (pdu->cantpState != CANTP_WAIT_DATA)
        return 0;

    // Check sequence number to ensure correct order
    uint8_t seqId = frame->data[0] & 0x0F;
    log_i("Consecutive Frame received, state: %d and seqID (pdu) %d, seqID incoming: %d", pdu->cantpState, pdu->seqId, seqId);
    if (seqId != pdu->seqId)
    {
        log_i("Sequence mismatch");
        return 1; // Sequence mismatch
    }

    // Determine how many bytes to copy
    // Copy up to 7 bytes (or fewer if _rxRestBytes < 7)
    uint8_t sizeToCopy = (_rxRestBytes > 7) ? 7 : _rxRestBytes;
    // Original offset logic: 6 + (seqId - 1)*7
    uint16_t offset = 6 + (pdu->seqId - 1) * 7; 
    memcpy(pdu->data + offset, &frame->data[1], sizeToCopy);

    // Decrease the remaining bytes count
    _rxRestBytes -= sizeToCopy;
    log_i("Rest bytes: %d", _rxRestBytes);

    // If we've received all data, update state to CANTP_END
    if (_rxRestBytes <= 0)
    {
        log_i("All data received");
        pdu->cantpState = CANTP_END;
    }
    else
    {
        // indicate how many bytes are left
        log_i("Bytes left: %d", _rxRestBytes);
    }

    // Increment sequence ID as original code does (no modulo)
    pdu->seqId++;

    return 0;
}

int CanIsoTp::receive_FlowControlFrame(pdu_t *pdu, CanFrame *frame)
{
    log_i("Flow Control Frame received");
    uint8_t flowStatus = frame->data[0] & 0x0F;
    int ret = 0;

    // Update blockSize and separationTimeMin only when waiting for the first FC
    if (pdu->cantpState == CANTP_WAIT_FIRST_FC)
    {
        log_i("Updating BS and STmin");
        pdu->blockSize = frame->data[1];
        pdu->separationTimeMin = frame->data[2];
        // Ensure STmin is within allowed range
        if ((pdu->separationTimeMin > 0x7F && pdu->separationTimeMin < 0xF1) || (pdu->separationTimeMin > 0xF9))
        {
            pdu->separationTimeMin = 0x7F; // Default to max 127ms if out-of-range
        }
    }

    switch (flowStatus)
    {
    case CANTP_FLOWSTATUS_CTS: // Continue to send
        pdu->cantpState = CANTP_SEND_CF;
        _receivedFCWaits = 0;
        log_i("CTS received, BS: %d, STmin: %d", pdu->blockSize, pdu->separationTimeMin);
        break;

    case CANTP_FLOWSTATUS_WT: // Wait
        _receivedFCWaits++;
        log_i("WT (Wait) frame received, count: %d", _receivedFCWaits);
        if (_receivedFCWaits >= WFTmax)
        {
            // Too many waits, abort transmission
            _receivedFCWaits = 0;
            pdu->cantpState = CANTP_IDLE;
            ret = 1;
            log_i("WFTmax exceeded, aborting transmission.");
        }
        // If WFTmax not reached, remain in current waiting state
        break;

    case CANTP_FLOWSTATUS_OVFL: // Overflow
    default:
        // Any unrecognized flow status leads to abort
        pdu->cantpState = CANTP_IDLE;
        ret = 1;
        log_i("Overflow or unknown flow status, aborting.");
        break;
    }

    return ret;
}
