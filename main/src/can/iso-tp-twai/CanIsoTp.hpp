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
#include <PinConfig.h>

#define N_PCItypeSF 0x00 // Single Frame
#define N_PCItypeFF 0x10 // First Frame
#define N_PCItypeCF 0x20 // Consecutive Frame
#define N_PCItypeFC 0x30 // Flow Control Frame

#define WFTmax 3 // Maximum number of wait frames

#define CANTP_FLOWSTATUS_CTS 0x00  // Continue to send
#define CANTP_FLOWSTATUS_WT 0x01   // Wait
#define CANTP_FLOWSTATUS_OVFL 0x02 // Overflow

#define PACKET_SIZE 8
#define TIMEOUT_SESSION 1000
#define TIMEOUT_FC 300
#define TIMEOUT_READ 300


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
#define TIMEOUT_SESSION   400 //1000 // Timeout for receiving complete message (ms)
#define TIMEOUT_FC        50 //500  // Timeout for Flow Control (ms)

// ISO-TP State Definitions
enum IsoTpState {
    CANTP_IDLE, // 0
    CANTP_SEND, // 1
    CANTP_WAIT_FIRST_FC, // 2
    CANTP_WAIT_FC, // 3
    CANTP_SEND_CF, // 4
    CANTP_WAIT_DATA, // 5
    CANTP_END, // 6 
    CANTP_ERROR // 7
};

// ISO-TP PDU Structure (PDU = Protocol Data Unit)
typedef struct {
    uint8_t txId;        /**< Transmit CAN ID */
    uint8_t rxId;        /**< Receive CAN ID */
    uint8_t *data;        /**< Pointer to data buffer */
    uint16_t len;         /**< Data length */
    uint8_t seqId;        /**< Sequence ID for Consecutive Frames */
    uint8_t fcStatus;     /**< Flow Control status */
    uint8_t blockSize;    /**< Block size for Flow Control */
    uint8_t separationTimeMin = 10; /**< Separation time between consecutive frames */
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
    int receive(pdu_t *pdu, uint32_t timeout);  // Receive ISO-TP message
    int receive(pdu_t *pdu, uint8_t *rxIDs, uint8_t numIDs, uint32_t timeout);  // Receive ISO-TP message with multiple addresses

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
