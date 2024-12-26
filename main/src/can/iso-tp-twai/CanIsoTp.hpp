#ifndef CAN_ISO_TP_HPP
#define CAN_ISO_TP_HPP

#include <ESP32-TWAI-CAN.hpp> // TWAI CAN Library for ESP32
#include <cstdint>            // Fixed-width integer types
#ifdef ARDUINO
#include <Arduino.h>
#else
#include "inttypes.h"
#endif
#include "driver/twai.h"

// PCI Types
#define N_PCItypeSF 0x00  // Single Frame
#define N_PCItypeFF 0x10  // First Frame
#define N_PCItypeCF 0x20  // Consecutive Frame
#define N_PCItypeFC 0x30  // Flow Control Frame

// Flow Control States
#define CANTP_FLOWSTATUS_CTS 0x00 // Continue to send
#define CANTP_FLOWSTATUS_WT  0x01 // Wait
#define CANTP_FLOWSTATUS_OVFL 0x02 // Overflow

// General Timeouts
#define PACKET_SIZE       8
#define TIMEOUT_SESSION   1000 // Timeout for receiving complete message (ms)
#define TIMEOUT_FC        500  // Timeout for Flow Control (ms)

// ISO-TP State Definitions
enum IsoTpState {
    CANTP_IDLE,
    CANTP_SEND,
    CANTP_WAIT_FIRST_FC,
    CANTP_WAIT_FC,
    CANTP_SEND_CF,
    CANTP_WAIT_DATA,
    CANTP_END,
    CANTP_ERROR
};

// ISO-TP PDU Structure (PDU = Protocol Data Unit)
typedef struct {
    uint32_t txId;        /**< Transmit CAN ID */
    uint32_t rxId;        /**< Receive CAN ID */
    uint8_t *data;        /**< Pointer to data buffer */
    uint16_t len;         /**< Data length */
    uint8_t seqId;        /**< Sequence ID for Consecutive Frames */
    uint8_t fcStatus;     /**< Flow Control status */
    uint8_t blockSize;    /**< Block size for Flow Control */
    uint8_t separationTimeMin; /**< Separation time between consecutive frames */
    IsoTpState cantpState; /**< ISO-TP State */
} pdu_t;



class CanIsoTp
{
public:
    // TWAI Instance
    TwaiCAN ESP32CanTwai;
    CanIsoTp();
    ~CanIsoTp();
    bool begin(long baudRate, int8_t txPin, int8_t rxPin); // Initialize CAN bus

    void end();               // Terminate CAN bus

    int send(pdu_t *pdu);     // Send ISO-TP message
    int receive(pdu_t *pdu);  // Receive ISO-TP message

private:
    uint32_t _timerSession;
    uint32_t _timerFCWait;
    uint32_t _timerCFWait;
    uint8_t _rxPacketData[PACKET_SIZE];
    uint8_t _rxRestBytes;
    uint8_t _receivedFCWaits = 0; // Reset this when starting a new transmission

    int send_SingleFrame(pdu_t *pdu);
    int send_FirstFrame(pdu_t *pdu);
    int send_ConsecutiveFrame(pdu_t *pdu);
    int send_FlowControlFrame(pdu_t *pdu);

    int receive_SingleFrame(pdu_t *pdu, CanFrame *frame);
    int receive_FirstFrame(pdu_t *pdu, CanFrame *frame);
    int receive_ConsecutiveFrame(pdu_t *pdu, CanFrame *frame);
    int receive_FlowControlFrame(pdu_t *pdu, CanFrame *frame);
};

#endif // CAN_ISO_TP_HPP
