#include "WifiController.h"
#include "../analogin/AnalogInController.h"
#include "../digitalout/DigitalOutController.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "../dac/DacController.h"
#include "../pid/PidController.h"
#include "../laser/LaserController.h"
#include "../led/LedController.h"

WifiController::WifiController() {}
WifiController::~WifiController() {}

String WifiController::getSsid()
{
	return pinConfig.mSSID;
}
String WifiController::getPw()
{
	return pinConfig.mPWD;
}
bool WifiController::getAp()
{
	return pinConfig.mAP;
}

DynamicJsonDocument WifiController::connect(DynamicJsonDocument doc)
{
	log_i("connectToWifi");

	bool ap = doc[keyWifiAP];
	String ssid = doc[keyWifiSSID];
	String pw = doc[keyWifiPW];
	log_i("ssid: %s wifi:%s", ssid.c_str(), WifiController::getSsid().c_str());
	log_i("pw: %s wifi:%s", pw.c_str(), WifiController::getPw().c_str());
	// log_i("ap: %s wifi:%s", ap, WifiController::getAp());
	WifiController::setWifiConfig(ssid, pw, ap);
	log_i("ssid json: %s wifi:%s", ssid, WifiController::getSsid());
	log_i("pw json: %s wifi:%s", pw, WifiController::getPw());
	// log_i("ap json: %s wifi:%s", ap, WifiController::getAp());
	WifiController::setup();
	doc.clear();
	return doc;
}

DynamicJsonDocument WifiController::scan()
{
	return espWifiController.wifi_scan();
}

void WifiController::sendJsonWebSocketMsg(DynamicJsonDocument doc)
{
	// log_i("socket broadcast");
	String s;
	serializeJson(doc, s);
	httpsServer.sendText((char*)s.c_str());
}

void WifiController::setWifiConfig(String SSID, String PWD, bool ap)
{
	// log_i("mssid:%s pw:%s ap:%s", pinConfig.mSSID, pinConfig.mPWD, pinConfig.mAP);
	config->mSsid = SSID;
	config->pw = PWD;
	config->ap = ap;
	Config::setWifiConfig(config);
}

void WifiController::setup()
{
	// initialize the Wifi module
	log_d("Setup Wifi");
	// retrieve Wifi Settings from Config (e.g. AP or SSId settings)
	config = Config::getWifiConfig();

	if ((pinConfig.mSSID != nullptr) and (pinConfig.mPWD != nullptr))
		log_i("mssid:%s pw:%s", pinConfig.mSSID, pinConfig.mPWD); //, pinConfig.mAP);
	if (httpsServer.running())
	{
		httpsServer.stop_webserver();
	}

	if (config->mSsid == "")
	{
		config->ap = pinConfig.mAP;
		config->mSsid = pinConfig.mSSID;
		config->pw = pinConfig.mPWD;
	}
	espWifiController.setWifiConfig(config);
	espWifiController.connect();
	httpsServer.start_webserver();
}

void WifiController::loop()
{
}

int WifiController::act(DynamicJsonDocument doc) { return 1; }
DynamicJsonDocument WifiController::get(DynamicJsonDocument doc) { return doc; }

void WifiController::restartWebServer()
{

	if (httpsServer.running())
		httpsServer.stop_webserver();
	httpsServer.start_webserver();
}
