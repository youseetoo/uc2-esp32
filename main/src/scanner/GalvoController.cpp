
#include "GalvoController.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "../laser/LaserController.h"
#include "../../ModuleController.h"
#include "Wire.h"
GalvoController::GalvoController(){

};
GalvoController::~GalvoController(){

};

void GalvoController::loop()
{
}

// Custom function accessible by the API
int GalvoController::act(cJSON *ob)
{
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
    X_MIN = 0;
    X_MAX = 30000;
    Y_MIN = 0;
    Y_MAX = 30000;
    STEP = 1000;
    tPixelDwelltime = 1;
    nFrames = 10;
    setJsonInt(ob, "X_MIN", X_MIN);
    setJsonInt(ob, "X_MAX", X_MAX);
    setJsonInt(ob, "Y_MIN", Y_MIN);
    setJsonInt(ob, "Y_MAX", Y_MAX);
    setJsonInt(ob, "STEP", STEP);
    setJsonInt(ob, "tPixelDwelltime", tPixelDwelltime);
    setJsonInt(ob, "nFrames", nFrames);

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

    return qid;
}

// Custom function accessible by the API
cJSON *GalvoController::get(cJSON *ob)
{
    return NULL;
}

/***************************************************************************************************/
/******************************************* Galvo     *******************************************/
/***************************************************************************************************/
/****************************************** ********************************************************/

// Function to write an integer in network byte order (big endian)
void GalvoController::writeInt(int value)
{
    Wire.write((value >> 24) & 0xFF);
    Wire.write((value >> 16) & 0xFF);
    Wire.write((value >> 8) & 0xFF);
    Wire.write(value & 0xFF);
}

void GalvoController::setup()
{
    log_d("Setup GalvoController");
    Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL); // Start I2C as master
}
