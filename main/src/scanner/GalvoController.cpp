
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

namespace GalvoController
{
    void loop()
    {
    }

    // Custom function accessible by the API
    int act(cJSON *ob)

    {   // {"task":"/galvo_act", "qid":1, "X_MIN":0, "X_MAX":30000, "Y_MIN":0, "Y_MAX":30000, "STEP":1000, "tPixelDwelltime":1, "nFrames":1}
        // {"task":"/galvo_act", "qid":1, "X_MIN":0, "X_MAX":30000, "Y_MIN":0, "Y_MAX":30000, "STEP":1000, "tPixelDwelltime":0, "nFrames":100}
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


        tPixelDwelltime += 1;
        renderer->setParameters(X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames);
        renderer->start();

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
        log_d("Setup GalvoController");
        Serial.println("Setup GalvoController");
        renderer = new SPIRenderer(X_MIN, X_MAX, Y_MIN, Y_MAX, STEP, tPixelDwelltime, nFrames);
        //Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL); // Start I2C as master
    }

}
