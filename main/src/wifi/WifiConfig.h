#pragma once
#include <Wstring.h>

struct WifiConfig
{
    const String mSSIDAP = F("UC2");
	const String hostname = F("youseetoo");
	String mSSID = "Uc2";
	String mPWD = "";
	bool mAP = true;
};
