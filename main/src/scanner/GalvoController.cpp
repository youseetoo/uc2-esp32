
#include "GalvoController.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "../laser/LaserController.h"
#include "Wire.h"
#include "cJsonTool.h"
#include "SPIRenderer.h"

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <vector>

#include "SPIRenderer.h"

#ifdef CAN_BUS_ENABLED
#include "../can/can_controller.h"
#endif

namespace GalvoController
{
    void loop()
    {
    }

    // Custom function accessible by the API
    int act(cJSON *ob)

    {   // {"task":"/galvo_act", "qid":1, "X_MIN":0, "X_MAX":30000, "Y_MIN":0, "Y_MAX":30000, "STEP":1000, "tPixelDwelltime":1, "nFrames":1}
        // {"task":"/galvo_act", "qid":1, "X_MIN":0, "X_MAX":100, "Y_MIN":0, "Y_MAX":100, "STEP":1, "tPixelDwelltime":0, "nFrames":1}
        // {"task":"/galvo_act", "qid":1, "X_MIN":0, "X_MAX":100, "Y_MIN":0, "Y_MAX":100, "STEP":1, "tPixelDwelltime":0, "nFrames":1, "fastMode":true}
        // here you can do something
        int qid = cJsonTool::getJsonInt(ob, "qid");
        /*
        int X_MIN = 0;
        int X_MAX = 30000;
        int Y_MIN = 0;
        int Y_MAX = 30000;
        int STEP = 1000;
        int tPixelDwelltime = 1;
        int nFrames = 10;
        */

        X_MIN = cJsonTool::getJsonInt(ob, "X_MIN", X_MIN);
        X_MAX = cJsonTool::getJsonInt(ob, "X_MAX", X_MAX);
        Y_MIN = cJsonTool::getJsonInt(ob, "Y_MIN", Y_MIN);
        Y_MAX = cJsonTool::getJsonInt(ob, "Y_MAX", Y_MAX);
        STEP = cJsonTool::getJsonInt(ob, "STEP", STEP);
        tPixelDwelltime = cJsonTool::getJsonInt(ob, "tPixelDwelltime", tPixelDwelltime);
        nFrames = cJsonTool::getJsonInt(ob, "nFrames", nFrames);
        
        // Check for fast mode parameter
        bool requestedFastMode = cJsonTool::getJsonBool(ob, "fastMode", fastMode);
        if (requestedFastMode != fastMode) {
            fastMode = requestedFastMode;
            renderer->setFastMode(fastMode);
            log_i("Fast mode %s", fastMode ? "enabled" : "disabled");
        }
        
#if defined(CAN_BUS_ENABLED) && !defined(CAN_RECEIVE_GALVO)
        // Send galvo command over CAN bus (master mode)
        GalvoData galvoData;
        galvoData.qid = qid;
        galvoData.X_MIN = X_MIN;
        galvoData.X_MAX = X_MAX;
        galvoData.Y_MIN = Y_MIN;
        galvoData.Y_MAX = Y_MAX;
        galvoData.STEP = STEP;
        galvoData.tPixelDwelltime = tPixelDwelltime;
        galvoData.nFrames = nFrames;
        galvoData.fastMode = fastMode;
        galvoData.isRunning = false; // Will be set to true when slave starts scanning
        
        can_controller::sendGalvoDataToCANDriver(galvoData);
        log_i("GalvoController CAN command sent: X_MIN: %i, X_MAX: %i, Y_MIN: %i, Y_MAX: %i, STEP: %i, tPixelDwelltime: %i, nFrames: %i, fastMode: %s", 
              X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames, fastMode ? "true" : "false");
#else
        // Local galvo control (slave mode or direct control)
        renderer->setParameters(X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames);
        renderer->start();
        log_i("GalvoController local act: X_MIN: %i, X_MAX: %i, Y_MIN: %i, Y_MAX: %i, STEP: %i, tPixelDwelltime: %i, nFrames: %i, fastMode: %s", 
              X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames, fastMode ? "true" : "false");
#endif

        /*
            Wire.beginTransmission(SLAVE_ADDR);
            writeInt(X_MIN);
            writeInt(X_MAX);
            writeInt(Y_MIN);
            writeInt(Y_MAX);
            writeInt(STEP);
            writeInt(tPixelDwelltime);
            writeInt(nFrames);
            byte error = Wire.endTransmission();

            if (error == 0)
            {
                Serial.println("Parameters sent successfully.");
            }
            else
            {
                Serial.print("Error sending parameters. Error code: ");
                Serial.println(error);
            }
    */
        return qid;
    }

    // Custom function accessible by the API
    cJSON *get(cJSON *ob)
    {
        return NULL;
    }

    /***************************************************************************************************/
    /******************************************* Galvo     *******************************************/
    /***************************************************************************************************/
    /****************************************** ********************************************************/

    // Function to write an integer in network byte order (big endian)
    void writeInt(int value)
    {
        Wire.write((value >> 24) & 0xFF);
        Wire.write((value >> 16) & 0xFF);
        Wire.write((value >> 8) & 0xFF);
        Wire.write(value & 0xFF);
    }

    void setup()
    {

#if defined(GALVO_CONTROLLER) &&  !defined(CAN_RECEIVE_GALVO) && defined(CAN_SEND_COMMANDS)
        log_d("Setup GalvoController as master");
        #else
        log_d("Setup GalvoController as slave");
        renderer = new SPIRenderer(X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames, 
        pinConfig.galvo_sdi, pinConfig.galvo_miso, pinConfig.galvo_sck, pinConfig.galvo_cs,
        pinConfig.galvo_ldac, pinConfig.galvo_trig_pixel, pinConfig.galvo_trig_line, pinConfig.galvo_trig_frame);
        /*
          SPIRenderer(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI, 
            uint8_t galvo_sdi, uint8_t galvo_miso, uint8_t galvo_sck, uint8_t galvo_cs, 
            uint8_t galvo_ldac, uint8_t galvo_trig_pixel, uint8_t galvo_trig_line, uint8_t galvo_trig_frame);
            */
        
        // Enable fast mode by default for galvo scanning
        renderer->setFastMode(fastMode);
        log_i("GalvoController setup complete, fast mode: %s", fastMode ? "enabled" : "disabled");
        #endif

    }

    void setFastMode(bool enabled)
    {
        fastMode = enabled;
        if (renderer != nullptr) {
            renderer->setFastMode(enabled);
        }
        log_i("GalvoController fast mode %s", enabled ? "enabled" : "disabled");
    }

    GalvoData getCurrentGalvoData()
    {
        GalvoData data;
        data.X_MIN = X_MIN;
        data.X_MAX = X_MAX;
        data.Y_MIN = Y_MIN;
        data.Y_MAX = Y_MAX;
        data.STEP = STEP;
        data.tPixelDwelltime = tPixelDwelltime;
        data.nFrames = nFrames;
        data.fastMode = fastMode;
        data.isRunning = isGalvoScanRunning;
        data.qid = -1; // Will be set by caller if needed
        return data;
    }

    void setFromGalvoData(const GalvoData& data)
    {
        X_MIN = data.X_MIN;
        X_MAX = data.X_MAX;
        Y_MIN = data.Y_MIN;
        Y_MAX = data.Y_MAX;
        STEP = data.STEP;
        tPixelDwelltime = data.tPixelDwelltime;
        nFrames = data.nFrames;
        
        if (data.fastMode != fastMode) {
            setFastMode(data.fastMode);
        }
        
        log_i("GalvoController updated from CAN data: X=[%d,%d], Y=[%d,%d], STEP=%d, dwell=%d, frames=%d, fast=%s",
              X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames, fastMode ? "true" : "false");
    }

    void sendCurrentStateToMaster()
    {
#if defined(GALVO_CONTROLLER) &&  defined(CAN_RECEIVE_GALVO)
        GalvoData currentState = getCurrentGalvoData();
        can_controller::sendGalvoStateToMaster(currentState);
#endif
    }

}
