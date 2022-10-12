#include "../../config.h"
#ifdef IS_WIFI
#include "WifiController.h"

namespace RestApi
{
	void scanWifi()
	{
		log_i("scanWifi");
		int networkcount = WiFi.scanNetworks();
		if (networkcount == -1)
		{
			while (true)
				;
		}
		WifiController::getJDoc()->clear();
		for (int i = 0; i < networkcount; i++)
		{
			(*WifiController::getJDoc()).add(WiFi.SSID(i));
		}
		serialize();
	}

	void connectToWifi()
	{
		deserialize();
		log_i("connectToWifi");
		bool ap = (*WifiController::getJDoc())[keyWifiAP];
		String ssid = (*WifiController::getJDoc())[keyWifiSSID];
		String pw = (*WifiController::getJDoc())[keyWifiPW];
		log_i("ssid json: %s wifi:%s", ssid, WifiController::getSsid());
		log_i("pw json: %s wifi:%s", pw, WifiController::getPw());
		log_i("ap json: %s wifi:%s", boolToChar(ap), boolToChar(WifiController::getAp()));
		WifiController::setWifiConfig(ssid, pw, ap);
		log_i("ssid json: %s wifi:%s", ssid, WifiController::getSsid());
		log_i("pw json: %s wifi:%s", pw, WifiController::getPw());
		log_i("ap json: %s wifi:%s", boolToChar(ap), boolToChar(WifiController::getAp()));
		Config::setWifiConfig(ssid, pw, ap, false);
		WifiController::getJDoc()->clear();
		serialize();
		WifiController::setup();
		// ESP.restart();
	}
}

namespace WifiController
{

	const String mSSIDAP = F("UC2");
	const String hostname = F("youseetoo");
	String mSSID = "Uc2";
	String mPWD = "";
	bool mAP = true;
	WebServer *server = nullptr;
	WebSocketsServer *webSocket = nullptr;
	DynamicJsonDocument *jsonDocument;

	DynamicJsonDocument *getJDoc()
	{
		return jsonDocument;
	}

	String getSsid()
	{
		return mSSID;
	}
	String getPw()
	{
		return mPWD;
	}
	bool getAp()
	{
		return mAP;
	}

	WebServer *getServer()
	{
		return server;
	}

	void handelMessages()
	{
		if (webSocket != nullptr)
			webSocket->loop();
		if (server != nullptr)
			server->handleClient();
	}

	void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
	{

		switch (type)
		{
		case WStype_DISCONNECTED:
			log_i("[%u] Disconnected!\n", num);
			break;
		case WStype_CONNECTED:
		{
			IPAddress ip = webSocket->remoteIP(num);
			log_i("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

			// send message to client
			webSocket->sendTXT(num, "Connected");
		}
		break;
		case WStype_TEXT:
			log_i("[%u] get Text: %s\n", num, payload);
			deserializeJson(*WifiController::getJDoc(), payload);
			if (WifiController::getJDoc()->containsKey(keyLed))
				led.act();
			if (WifiController::getJDoc()->containsKey(key_motor))
				motor.act();

			break;
		}
	}

	void sendJsonWebSocketMsg()
	{
		String s;
		serializeJson((*WifiController::getJDoc()), s);
		webSocket->broadcastTXT(s);
	}

	void createJsonDoc()
	{
		jsonDocument = new DynamicJsonDocument(16128);
		log_i("WifiController::createJsonDoc is null:%s", boolToChar(jsonDocument == nullptr));
	}

	void setWifiConfig(String SSID, String PWD, bool ap)
	{
		log_i("mssid:%s pw:%s ap:%s", mSSID, mPWD, boolToChar(ap));
		mSSID = SSID;
		mPWD = PWD;
		mAP = ap;
	}

	void createAp(String ssid, String password)
	{
		WiFi.disconnect();
		log_i("Ssid %s pw %s", ssid, password);

		if (ssid.isEmpty())
		{
			log_i("Ssid empty, start Uc2 open softap");
			WiFi.softAP(mSSIDAP.c_str());
		}
		else if (password.isEmpty())
		{
			log_i("Ssid start %s open softap", ssid);
			WiFi.softAP(ssid.c_str());
		}
		else
		{
			log_i("Ssid start %s close softap", ssid);
			WiFi.softAP(ssid.c_str(), password.c_str());
		}
		log_i("Connected. IP: %s", WiFi.softAPIP().toString());
		// server = new WebServer(WiFi.softAPIP(),80);
	}

	void setup()
	{
		if (mSSID != nullptr)
			log_i("mssid:%s pw:%s ap:%s", mSSID, mPWD, boolToChar(mAP));
		if (server != nullptr)
		{
			server->close();
			server = nullptr;
		}
		if (mSSID == "")
		{
			mAP = true;
			createAp(mSSIDAP, mPWD);
		}
		else if (mAP)
		{
			createAp(mSSID, mPWD);
		}
		else
		{
			WiFi.softAPdisconnect();
			log_i("Connect to:%s", mSSID);
			WiFi.begin(mSSID.c_str(), mPWD.c_str());

			int nConnectTrials = 0;
			while (WiFi.status() != WL_CONNECTED && nConnectTrials <= 10)
			{
				log_i("Wait for connection");
				delay(200);
				nConnectTrials += 1;
				// we can even make the ESP32 to sleep
			}
			if (nConnectTrials == 10)
			{
				log_i("failed to connect,Start softap");
				mAP = true;
				mSSID = mSSIDAP;
				mPWD = "";
				createAp(mSSIDAP, "");
			}
			else
			{
				log_i("Connected. IP: %s", WiFi.localIP());
				// server = new WebServer(WiFi.localIP(),80);
			}
		}
		if (server != nullptr)
			server->close();
		server = new WebServer(80);
		if (webSocket != nullptr)
			webSocket->close();
		webSocket = new WebSocketsServer(81);
		if (jsonDocument == nullptr)
		{
			createJsonDoc();
		}
		webSocket->begin();
		webSocket->onEvent(webSocketEvent);
		setup_routing();
		server->begin();
		log_i("HTTP Running server  nullptr: %s jsondoc  nullptr: %s", boolToChar(server == nullptr), boolToChar(jsonDocument == nullptr));
	}

	void getIndexPage()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/index.html", "r");
		server->streamFile(file, "text/html");
		file.close();
		SPIFFS.end();
	}

	void getjquery()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/jquery-3.6.1.min.js", "r");
		server->streamFile(file, "text/javascript");
		file.close();
		SPIFFS.end();
	}
	void getjs()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/script.js", "r");
		server->streamFile(file, "text/javascript");
		file.close();
		SPIFFS.end();
	}

	void getCSS()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/styles.css", "r");
		server->streamFile(file, "text/css");
		file.close();
		SPIFFS.end();
	}

	void setup_routing()
	{
		log_i("Setting up HTTP Routing");
		server->onNotFound(RestApi::handleNotFound);
		server->on(state_act_endpoint, HTTP_POST, RestApi::State_act);
		server->on(state_get_endpoint, HTTP_POST, RestApi::State_get);
		server->on(state_set_endpoint, HTTP_POST, RestApi::State_set);

		server->on(identity_endpoint, RestApi::getIdentity);
		server->on(features_endpoint, RestApi::getEndpoints);

		server->on(ota_endpoint, HTTP_GET, RestApi::ota);
		/*handling uploading firmware file */
		server->on(update_endpoint, HTTP_POST, RestApi::update, RestApi::upload);

		server->on("/", HTTP_GET, getIndexPage);
		server->on("/styles.css", HTTP_GET, getCSS);
		server->on("/script.js", HTTP_GET, getjs);
		server->on("/jquery-3.6.1.min.js", HTTP_GET, getjquery);
		server->on(scanwifi_endpoint, HTTP_GET, RestApi::scanWifi);
		server->on(connectwifi_endpoint, HTTP_POST, RestApi::connectToWifi);
		server->on(reset_nv_flash_endpoint, HTTP_GET, RestApi::resetNvFLash);

		server->on(bt_scan_endpoint, HTTP_GET, RestApi::Bt_startScan);
		server->on(bt_connect_endpoint, HTTP_POST, RestApi::Bt_connect);
		server->on(bt_remove_endpoint, HTTP_POST, RestApi::Bt_remove);
		server->on(bt_paireddevices_endpoint, HTTP_GET, RestApi::Bt_getPairedDevices);

		// POST
#ifdef IS_MOTOR
		server->on(motor_act_endpoint, HTTP_POST, RestApi::FocusMotor_act);
		server->on(motor_get_endpoint, HTTP_GET, RestApi::FocusMotor_get);
		server->on(motor_set_endpoint, HTTP_POST, RestApi::FocusMotor_set);
#endif

#ifdef IS_DAC
		server->on(dac_act_endpoint, HTTP_POST, RestApi::Dac_act);
		server->on(dac_get_endpoint, HTTP_POST, RestApi::Dac_get);
		server->on(dac_set_endpoint, HTTP_POST, RestApi::Dac_set);
#endif

#ifdef IS_LASER
		server->on(laser_act_endpoint, HTTP_POST, RestApi::Laser_act);
		server->on(laser_get_endpoint, HTTP_POST, RestApi::Laser_get);
		server->on(laser_set_endpoint, HTTP_POST, RestApi::Laser_set);
#endif

#ifdef IS_ANALOG
		server->on(analog_act_endpoint, HTTP_POST, RestApi::Analog_act);
		server->on(analog_get_endpoint, HTTP_POST, RestApi::Analog_get);
		server->on(analog_set_endpoint, HTTP_POST, RestApi::Analog_set);
#endif

#ifdef IS_DIGITAL
		server->on(digital_act_endpoint, HTTP_POST, RestApi::Digital_act);
		server->on(digital_get_endpoint, HTTP_POST, RestApi::Digital_get);
		server->on(digital_set_endpoint, HTTP_POST, RestApi::Digital_set);
#endif

#ifdef IS_PID
		server->on(PID_act_endpoint, HTTP_POST, RestApi::Pid_act);
		server->on(PID_get_endpoint, HTTP_POST, RestApi::Pid_get);
		server->on(PID_set_endpoint, HTTP_POST, RestApi::Pid_set);
#endif

#ifdef IS_LED
		server->on(ledarr_act_endpoint, HTTP_POST, RestApi::Led_act);
		server->on(ledarr_get_endpoint, HTTP_GET, RestApi::Led_get);
		server->on(ledarr_set_endpoint, HTTP_POST, RestApi::Led_set);
#endif

#ifdef IS_SLM
		server->on(slm_act_endpoint, HTTP_POST, RestApi::Slm_act);
		server->on(slm_get_endpoint, HTTP_POST, RestApi::Slm_get);
		server->on(slm_set_endpoint, HTTP_POST, RestApi::Slm_set);
#endif

		server->on(config_act_endpoint, HTTP_POST, RestApi::Config_act);
		server->on(config_get_endpoint, HTTP_POST, RestApi::Config_get);
		server->on(config_set_endpoint, HTTP_POST, RestApi::Config_set);

		log_i("Setting up HTTP Routing END");
	}
}

#endif
