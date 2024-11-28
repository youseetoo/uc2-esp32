#include <PinConfig.h>

#include "WifiController.h"
#include "../../cJsonTool.h"
#include "JsonKeys.h"
#include "../config/ConfigController.h"
#include "EspHttpsServer.h"
#include "esp_log.h"
#include "WifiConfig.h"
#include "EspWifiController.h"
#include "mdns.h"
#include "PinConfig.h"

namespace WifiController
{

	WifiConfig *config;
    EspWifiController espWifiController;
    EspHttpsServer httpsServer;
	char *getSsid()
	{
		return (char *)pinConfig.mSSID;
	}
	char *getPw()
	{
		return (char *)pinConfig.mPWD;
	}
	bool getAp()
	{
		return pinConfig.mAP;
	}

	cJSON *connect(cJSON *doc)
	{
		log_i("connectToWifi");
		bool ap = cJsonTool::getJsonInt(doc, keyWifiAP);

		char *ssid = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(doc, keyWifiSSID));
		char *pw = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(doc, keyWifiAP));
		log_i("ssid: %s wifi:%s", ssid, WifiController::getSsid());
		log_i("pw: %s wifi:%s", pw, WifiController::getPw());
		// log_i("ap: %s wifi:%s", ap, WifiController::getAp());
		WifiController::setWifiConfig(ssid, pw, ap);
		log_i("ssid json: %s wifi:%s", ssid, WifiController::getSsid());
		log_i("pw json: %s wifi:%s", pw, WifiController::getPw());
		// log_i("ap json: %s wifi:%s", ap, WifiController::getAp());
		WifiController::setup();
		return NULL;
	}

	cJSON *scan()
	{
		return espWifiController.wifi_scan();
	}

	void sendJsonWebSocketMsg(cJSON *doc)
	{
		// log_i("socket broadcast");
		char *s = cJSON_PrintUnformatted(doc);
		if(s != nullptr && strlen(s) > 0)
			httpsServer.sendText(s);
		free(s);
	}

	void setWifiConfig(char *SSID, char *PWD, bool ap)
	{
		// log_i("mssid:%s pw:%s ap:%s", pinConfig.mSSID, pinConfig.mPWD, pinConfig.mAP);
		config->mSsid = SSID;
		config->pw = PWD;
		config->ap = ap;
		Config::setWifiConfig(config);
	}

	void setup()
	{
		// initialize the Wifi module
		log_d("Setup Wifi");
		// retrieve Wifi Settings from Config (e.g. AP or SSId settings)
		config = Config::getWifiConfig();

		// if the Wifi settings are not set, use the default settings
		if ((pinConfig.mSSID != nullptr) && (pinConfig.mPWD != nullptr))
			log_i("mssid:%s pw:%s", pinConfig.mSSID, pinConfig.mPWD); //, pinConfig.mAP);
		
		// start the webserver
		if (httpsServer.running())
		{
			httpsServer.stop_webserver();
			mdns_free();
		}

		
		if (config->mSsid == NULL || strcmp(config->mSsid, ""))
		{
			config->ap = pinConfig.mAP;
			config->mSsid = (char *)pinConfig.mSSID;
			config->pw = (char *)pinConfig.mPWD;
		}
		espWifiController.setWifiConfig(config);
		espWifiController.connect();
		httpsServer.start_webserver();
		mdns_init();
		mdns_hostname_set(pinConfig.pindefName);
		mdns_instance_name_set(pinConfig.pindefName);
		mdns_service_add(pinConfig.pindefName, "_http", "_tcp", 80, NULL, 0);
	}

	int act(cJSON *doc) { return 1; }
	cJSON *get(cJSON *doc) { return doc; }

	void restartWebServer()
	{
		mdns_free();
		if (httpsServer.running())
			httpsServer.stop_webserver();
		httpsServer.start_webserver();
		mdns_init();
		mdns_hostname_set(pinConfig.pindefName);
		mdns_instance_name_set(pinConfig.pindefName);
		mdns_service_add(pinConfig.pindefName, "_http", "_tcp", 80, NULL, 0);
	}
}
