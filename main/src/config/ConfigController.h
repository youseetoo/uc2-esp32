#pragma once


namespace Config
{
    void setup();
    bool resetPreferences();
    void setPsxMac(char* mac);
    char* getPsxMac();
    void setPsxControllerType(int type);
    int getPsxControllerType();
}
