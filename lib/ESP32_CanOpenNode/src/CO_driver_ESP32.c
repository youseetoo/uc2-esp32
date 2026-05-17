/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "301/CO_driver.h"
#include "CO_driver_target.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <Arduino.h>
#include <hal/twai_types.h>

#define CANID_MASK 0x07FF /*!< CAN standard ID mask */

typedef struct CANMessages {
    twai_message_t message;
} TX_Messages, RX_Messages;

extern QueueHandle_t CAN_TX_queue;
extern QueueHandle_t CAN_RX_queue;


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANptr)
{
    /* Put CAN module in configuration mode */
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    /* Put CAN module in normal mode */

    CANmodule->CANnormal = true;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t *CANmodule,
                                   void *CANptr,
                                   CO_CANrx_t rxArray[],
                                   uint16_t rxSize,
                                   CO_CANtx_t txArray[],
                                   uint16_t txSize,
                                   uint16_t CANbitRate)
{
    uint16_t i;

    /* verify arguments */
    if (CANmodule == NULL || rxArray == NULL || txArray == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = false; /* microcontroller dependent */
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    for (i = 0U; i < rxSize; i++) {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for (i = 0U; i < txSize; i++) {
        txArray[i].bufferFull = false;
    }


    /* Configure CAN module registers */


    /* Configure CAN timing */


    /* Configure CAN module hardware filters */
    if (CANmodule->useCANrxFilters) {
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
    } else {
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are
         * accepted */
    }


    /* configure CAN interrupt registers */
    CANmodule->xOD_Mutex = xSemaphoreCreateMutex();
    CANmodule->xCAN_SEND_Mutex = xSemaphoreCreateMutex();
    CANmodule->xCAN_ERROR_Mutex = xSemaphoreCreateMutex();

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    if (CANmodule != NULL) {
        /* turn off the module */
    }
}


/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t *CANmodule,
                                    uint16_t index,
                                    uint16_t ident,
                                    uint16_t mask,
                                    bool_t rtr,
                                    void *object,
                                    void (*CANrx_callback)(void *object,
                                                           void *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if ((CANmodule != NULL) && (object != NULL) && (CANrx_callback != NULL) &&
        (index < CANmodule->rxSize)) {
        /* buffer, which will be configured */
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;

        /* CAN identifier and CAN mask, bit aligned with CAN module. Different
         * on different microcontrollers. */
        buffer->ident = ident & 0x07FFU;
        if (rtr) {
            buffer->ident |= 0x0800U;
        }
        buffer->mask = (mask & 0x07FFU) | 0x0800U;

        /* Set CAN hardware module filter and mask. */
        if (CANmodule->useCANrxFilters) {
        }
    } else {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(CO_CANmodule_t *CANmodule,
                               uint16_t index,
                               uint16_t ident,
                               bool_t rtr,
                               uint8_t noOfBytes,
                               bool_t syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if ((CANmodule != NULL) && (index < CANmodule->txSize)) {
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, DLC and rtr, bit aligned with CAN module transmit
         * buffer. Microcontroller specific. */
        buffer->ident = ((uint32_t)ident & 0x07FFU) |
                        ((uint32_t)(((uint32_t)noOfBytes & 0xFU) << 12U)) |
                        ((uint32_t)(rtr ? 0x8000U : 0U));

        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}

/* Internal helper: build a TWAI frame from a CO_CANtx_t and try to enqueue
 * it to CAN_TX_queue (non-blocking). Caller MUST hold xCAN_SEND_Mutex.
 * Returns 1 on success, 0 if the queue is full. */
static uint8_t send_can_message_locked(CO_CANtx_t *buffer)
{
    struct CANMessages tx_msg;

    tx_msg.message.identifier        = buffer->ident & CANID_MASK;
    tx_msg.message.data_length_code  = 8;
    tx_msg.message.data[0]           = buffer->data[0];
    tx_msg.message.data[1]           = buffer->data[1];
    tx_msg.message.data[2]           = buffer->data[2];
    tx_msg.message.data[3]           = buffer->data[3];
    tx_msg.message.data[4]           = buffer->data[4];
    tx_msg.message.data[5]           = buffer->data[5];
    tx_msg.message.data[6]           = buffer->data[6];
    tx_msg.message.data[7]           = buffer->data[7];
    tx_msg.message.extd              = 0;
    tx_msg.message.rtr               = 0;
    tx_msg.message.ss                = 0;
    tx_msg.message.self              = 0;
    tx_msg.message.dlc_non_comp      = 1;

    return (xQueueSend(CAN_TX_queue, (void *)&tx_msg, 0) == pdTRUE) ? 1 : 0;
}

/* Public single-shot send. Takes the send-side lock and delegates. Used by
 * CO_CANsend when there are no pending retries. */
static uint8_t send_can_message(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    uint8_t ok;
    CO_LOCK_CAN_SEND(CANmodule);
    ok = send_can_message_locked(buffer);
    CO_UNLOCK_CAN_SEND(CANmodule);
    return ok;
}


/******************************************************************************/
/* ESP32 port — TX retry architecture (Option 1, upstream-compliant)
 *
 * Upstream CANopenNode expects two cooperating pieces in any port:
 *   - CO_CANsend(): writes the frame to the HW mailbox if it's free.
 *     Otherwise marks buffer->bufferFull=true, increments CANtxCount, and
 *     returns CO_ERROR_TX_OVERFLOW (a soft error — the upper layer just
 *     waits for the buffer to clear).
 *   - A TX-complete hardware interrupt that calls CO_CANinterrupt(), which
 *     walks txArray for the next bufferFull buffer, copies it into the HW
 *     mailbox, clears bufferFull, and decrements CANtxCount.
 *
 * The SDO client (CO_SDOclient.c:871-873) checks bufferFull *before* calling
 * CO_CANsend on each tick — so the only way for a frame stuck with
 * bufferFull=true to ever go out is the TX-complete-driven retry walk.
 *
 * On this ESP32 port:
 *   - "HW mailbox" = a slot in CAN_TX_queue (32-deep FreeRTOS queue).
 *   - CAN_ctrl_task (CANopenModule.cpp) drains CAN_TX_queue into the TWAI HW.
 *   - "TX-complete interrupt" = CAN_ctrl_task itself, after each successful
 *     dequeue+twai_transmit, calling CO_CANtx_retryQueued() to walk txArray
 *     for any buffers that CO_CANsend couldn't fit and re-enqueue them.
 *
 * This restores the upstream contract:
 *   - CO_CANsend is fast and non-blocking.
 *   - CANtxCount tracks pending retries (decrements as buffers drain).
 *   - CO_CANclearPendingSyncPDOs() works as designed (iterating txArray).
 *   - The "lost frame" failure mode that broke SDO block download
 *     (transient queue-full → permanent bufferFull → state-machine
 *     deadlock → slave SDO timeout 0x05040000) is gone, because
 *     CO_CANtx_retryQueued unconditionally retries stuck buffers on every
 *     CAN_ctrl_task tick. */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;

    CO_LOCK_CAN_SEND(CANmodule);

    /* Defensive: if the caller submitted a buffer whose bufferFull is still
     * set from a prior call, the retry walker already owns it. Don't
     * double-count CANtxCount — just report overflow and bail. */
    if (buffer->bufferFull) {
        if (!CANmodule->firstCANtxMessage) {
            CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        }
        CO_UNLOCK_CAN_SEND(CANmodule);
        return CO_ERROR_TX_OVERFLOW;
    }

    /* Try to enqueue immediately. We only take the fast path when there
     * are no pending retries — preserves FIFO order with the walker. */
    if (CANmodule->CANtxCount == 0 && send_can_message_locked(buffer)) {
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        CANmodule->firstCANtxMessage = false;
        /* bufferFull is already false (checked above). */
    } else {
        /* Either CAN_TX_queue is full, or earlier buffers are still
         * queued for retry. Defer to CO_CANtx_retryQueued. */
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
    }

    CO_UNLOCK_CAN_SEND(CANmodule);
    return err;
}


/******************************************************************************/
/* TX retry walker — the ESP32 port's stand-in for the TX-complete interrupt.
 *
 * Called by CAN_ctrl_task after each successful dequeue+twai_transmit, when
 * CAN_TX_queue has just gained free space. Walks txArray and re-tries every
 * buffer that CO_CANsend couldn't fit. Each successful re-send clears
 * bufferFull and decrements CANtxCount. Stops early if the queue refills
 * mid-walk (we'll catch the remainder on the next tick).
 *
 * Locking: takes xCAN_SEND_Mutex. CO_CANsend takes the same mutex, so the
 * walker and the producers serialize cleanly. */
void CO_CANtx_retryQueued(CO_CANmodule_t *CANmodule)
{
    if (CANmodule == NULL || CANmodule->txArray == NULL) return;

    /* Fast path: nothing pending. Avoid the lock entirely. CANtxCount is
     * volatile-by-convention here (it's only written under the lock; reading
     * it racy is fine — we'll see the change on a later tick). */
    if (CANmodule->CANtxCount == 0) return;

    CO_LOCK_CAN_SEND(CANmodule);

    CO_CANtx_t *buffer = &CANmodule->txArray[0];
    for (uint16_t i = CANmodule->txSize;
         i > 0U && CANmodule->CANtxCount > 0U;
         i--, buffer++)
    {
        if (!buffer->bufferFull) continue;

        if (!send_can_message_locked(buffer)) {
            /* CAN_TX_queue full again — abandon walk; next tick retries. */
            break;
        }

        buffer->bufferFull = false;
        CANmodule->CANtxCount--;
        CANmodule->bufferInhibitFlag = buffer->syncFlag;
        CANmodule->firstCANtxMessage = false;
    }

    CO_UNLOCK_CAN_SEND(CANmodule);
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    uint32_t tpdoDeleted = 0U;

    if (CANmodule == NULL) return;

    /* Hold the lock for the whole walk: the retry walker
     * (CO_CANtx_retryQueued) also iterates txArray and we must not race. */
    CO_LOCK_CAN_SEND(CANmodule);

    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if (CANmodule->bufferInhibitFlag) {
        CANmodule->bufferInhibitFlag = false;
        tpdoDeleted = 1U;
    }
    /* Drop pending synchronous TPDOs that are still waiting for retry. */
    if (CANmodule->CANtxCount != 0U) {
        CO_CANtx_t *buffer = &CANmodule->txArray[0];
        for (uint16_t i = CANmodule->txSize; i > 0U; i--, buffer++) {
            if (buffer->bufferFull && buffer->syncFlag) {
                buffer->bufferFull = false;
                CANmodule->CANtxCount--;
                tpdoDeleted = 2U;
            }
        }
    }

    CO_UNLOCK_CAN_SEND(CANmodule);

    if (tpdoDeleted != 0U) {
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}


/******************************************************************************/
/* Get error counters from the module. If necessary, function may use
 * different way to determine errors. */
static uint16_t rxErrors = 0, txErrors = 0, overflow = 0;

void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    uint32_t err;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

    if (CANmodule->errOld != err) {
        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if (txErrors >= 256U) {
            /* bus off */
            status |= CO_CAN_ERRTX_BUS_OFF;
        } else {
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF ^ (CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRRX_WARNING |
                                CO_CAN_ERRRX_PASSIVE | CO_CAN_ERRTX_WARNING |
                                CO_CAN_ERRTX_PASSIVE);

            /* rx bus warning or passive */
            if (rxErrors >= 128) {
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE;
            } else if (rxErrors >= 96) {
                status |= CO_CAN_ERRRX_WARNING;
            }

            /* tx bus warning or passive */
            if (txErrors >= 128) {
                status |= CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE;
            } else if (rxErrors >= 96) {
                status |= CO_CAN_ERRTX_WARNING;
            }

            /* if not tx passive clear also overflow */
            if ((status & CO_CAN_ERRTX_PASSIVE) == 0) {
                status &= 0xFFFF ^ CO_CAN_ERRTX_OVERFLOW;
            }
        }

        if (overflow != 0) {
            /* CAN RX bus overflow */
            status |= CO_CAN_ERRRX_OVERFLOW;
        }

        CANmodule->CANerrorStatus = status;
    }
}

/******************************************************************************/

void CO_CANinterrupt(CO_CANmodule_t *CANmodule)
{
    struct CANMessages rx_msg;
    /* receive interrupt */
    if (xQueueReceive(CAN_RX_queue, (void *)&rx_msg, 0) == pdTRUE) {
        CO_CANrxMsg_t *rcvMsg; /* pointer to received message in CAN module */
        uint16_t index;        /* index of received message */
        uint32_t rcvMsgIdent;  /* identifier of the received message */
        CO_CANrx_t *buffer =
            NULL; /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        // rcvMsg = 0; /* get message from module here */
        CO_CANrxMsg_t msg;
        rcvMsg = &msg;
        rcvMsg->ident = rx_msg.message.identifier;
        rcvMsgIdent = rcvMsg->ident;
        rcvMsg->DLC = rx_msg.message.data_length_code;
        for (int i = 0; i < 8; i++)
            rcvMsg->data[i] = rx_msg.message.data[i];

        // Log EVERY received frame so we can see what the bus carries
        {
            uint64_t rxData = 0;
            for (int i = 0; i < rx_msg.message.data_length_code; i++)
                rxData |= ((uint64_t)rx_msg.message.data[i] << (8 * i));
            if (0)
                log_i("CO_RX", "Received CAN message: id=0x%03X dlc=%u data=0x%016llX",
                    rx_msg.message.identifier,
                    rx_msg.message.data_length_code,
                    rxData);
        }

        if (CANmodule->useCANrxFilters) {
            /* CAN module filters are used. Message with known 11-bit identifier
             * has */
            /* been received */
            index = 0; /* get index of the received message here. Or something
                          similar */
            if (index < CANmodule->rxSize) {
                buffer = &CANmodule->rxArray[index];
                /* verify also RTR */
                if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U) {
                    msgMatched = true;
                }
            }
        } else {
            /* CAN module filters are not used, message with any standard 11-bit
             * identifier */
            /* has been received. Search rxArray form CANmodule for the same
             * CAN-ID. */
            buffer = &CANmodule->rxArray[0];
            for (index = CANmodule->rxSize; index > 0U; index--) {
                if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U) {
                    msgMatched = true;
                    break;
                }
                buffer++;
            }
        }

        uint64_t msgData = 0;
        for (int i = 0; i < rx_msg.message.data_length_code; i++) {
            msgData |= ((uint64_t)rx_msg.message.data[i] << (8 * i));
        }

        /* Call specific function, which will process the message */
        if (msgMatched && (buffer != NULL) &&
            (buffer->CANrx_callback != NULL)) {
            buffer->CANrx_callback(buffer->object, (void *)rcvMsg);


            ESP_LOGI("CO_INT_PROCESS",
                     "CAN:%X,%llX",
                     rx_msg.message.identifier,
                     msgData);
        } else {
            ESP_LOGI(
                "CO_INT", "CAN:%X,%llX", rx_msg.message.identifier, msgData);
        }
        /* Clear interrupt flag */
    }


    /* transmit interrupt */
    else if (0) {
        /* Clear interrupt flag */

        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = false;
        /* Are there any new messages waiting to be send */
        if (CANmodule->CANtxCount > 0U) {
            uint16_t i; /* index of transmitting message */

            /* first buffer */
            CO_CANtx_t *buffer = &CANmodule->txArray[0];
            /* search through whole array of pointers to transmit message
             * buffers. */
            for (i = CANmodule->txSize; i > 0U; i--) {
                /* if message buffer is full, send it. */
                if (buffer->bufferFull) {
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;

                    /* Copy message to CAN buffer */
                    CANmodule->bufferInhibitFlag = buffer->syncFlag;
                    /* canSend... */
                    break; /* exit for loop */
                }
                buffer++;
            } /* end of for loop */

            /* Clear counter if no more messages */
            if (i == 0U) {
                CANmodule->CANtxCount = 0U;
            }
        }
    } else {
        /* some other interrupt reason */
    }
}
