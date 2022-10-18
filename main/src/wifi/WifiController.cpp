#include "../../config.h"
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
		WifiController::connect();
		serialize();
		// ESP.restart();
	}
}

namespace WifiController
{

	WebServer *server = nullptr;
	WebSocketsServer *webSocket = nullptr;
	DynamicJsonDocument *jsonDocument;
	WifiConfig * config;

	DynamicJsonDocument *getJDoc()
	{
		return jsonDocument;
	}

	String getSsid()
	{
		return config->mSSID;
	}
	String getPw()
	{
		return config->mPWD;
	}
	bool getAp()
	{
		return config->mAP;
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

	void connect()
	{
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
		WifiController::getJDoc()->clear();
		WifiController::setup();
		WifiController::begin();
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
			if (WifiController::getJDoc()->containsKey(keyLed) && moduleController.get(AvailableModules::led) != nullptr)
				moduleController.get(AvailableModules::led)->act();
			if (WifiController::getJDoc()->containsKey(key_motor) && moduleController.get(AvailableModules::motor) != nullptr)
				moduleController.get(AvailableModules::motor)->act();

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
		log_i("mssid:%s pw:%s ap:%s", config->mSSID, config->mPWD, boolToChar(config->mAP));
		config->mSSID = SSID;
		config->mPWD = PWD;
		config->mAP = ap;
		Config::setWifiConfig(config);
	}

	void createAp(String ssid, String password)
	{
		WiFi.disconnect();
		log_i("Ssid %s pw %s", ssid, password);

		if (ssid.isEmpty())
		{
			log_i("Ssid empty, start Uc2 open softap");
			WiFi.softAP(config->mSSIDAP.c_str());
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
		config = Config::getWifiConfig();
		if (config->mSSID != nullptr)
			log_i("mssid:%s pw:%s ap:%s", config->mSSID, config->mPWD, boolToChar(config->mAP));
		if (server != nullptr)
		{
			server->close();
			server = nullptr;
		}
		if (webSocket != nullptr)
			webSocket->close();
		if (config->mSSID == "")
		{
			config->mAP = true;
			createAp(config->mSSIDAP, config->mPWD);
		}
		else if (config->mAP)
		{
			createAp(config->mSSID, config->mPWD);
		}
		else
		{
			WiFi.softAPdisconnect();
			log_i("Connect to:%s", config->mSSID);
			WiFi.begin(config->mSSID.c_str(), config->mPWD.c_str());

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
				config->mAP = true;
				config->mSSID = config->mSSIDAP;
				config->mPWD = "";
				createAp(config->mSSIDAP, "");
			}
			else
			{
				log_i("Connected. IP: %s", WiFi.localIP());
				// server = new WebServer(WiFi.localIP(),80);
			}
		}
		server = new WebServer(80);
		webSocket = new WebSocketsServer(81);
		if (jsonDocument == nullptr)
		{
			createJsonDoc();
		}
	}

	void begin()
	{
		webSocket->begin();
		webSocket->onEvent(webSocketEvent);
		setup_routing();
		server->begin();
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

	void getOtaIndex()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/ota.html", "r");
		server->streamFile(file, "text/html");
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

		server->on(ota_endpoint, HTTP_GET, getOtaIndex);
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

		server->on(modules_get_endpoint, HTTP_GET,RestApi::getModules);
		server->on(modules_set_endpoint, HTTP_POST,RestApi::setModules);

		// POST
		if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			log_i("add motor endpoints");
			server->on(motor_act_endpoint, HTTP_POST, RestApi::FocusMotor_act);
			server->on(motor_get_endpoint, HTTP_GET, RestApi::FocusMotor_get);
			server->on(motor_set_endpoint, HTTP_POST, RestApi::FocusMotor_set);
		}

		if (moduleController.get(AvailableModules::dac) != nullptr)
		{
			log_i("add dac endpoints");
			server->on(dac_act_endpoint, HTTP_POST, RestApi::Dac_act);
			server->on(dac_get_endpoint, HTTP_POST, RestApi::Dac_get);
			server->on(dac_set_endpoint, HTTP_POST, RestApi::Dac_set);
		}

		if (moduleController.get(AvailableModules::laser) != nullptr)
		{
			log_i("add laser endpoints");
			server->on(laser_set_endpoint, HTTP_POST, RestApi::Laser_set);
			server->on(laser_get_endpoint, HTTP_GET, RestApi::Laser_get);
			server->on(laser_act_endpoint, HTTP_POST, RestApi::Laser_act);
		}

		if (moduleController.get(AvailableModules::analog) != nullptr)
		{
			log_i("add analog endpoints");
			server->on(analog_act_endpoint, HTTP_POST, RestApi::Analog_act);
			server->on(analog_get_endpoint, HTTP_POST, RestApi::Analog_get);
			server->on(analog_set_endpoint, HTTP_POST, RestApi::Analog_set);
		}

		if (moduleController.get(AvailableModules::digital) != nullptr)
		{
			log_i("add digital endpoints");
			server->on(digital_act_endpoint, HTTP_POST, RestApi::Digital_act);
			server->on(digital_get_endpoint, HTTP_POST, RestApi::Digital_get);
			server->on(digital_set_endpoint, HTTP_POST, RestApi::Digital_set);
		}

		if (moduleController.get(AvailableModules::pid) != nullptr)
		{
			log_i("add pid endpoints");
			server->on(PID_act_endpoint, HTTP_POST, RestApi::Pid_act);
			server->on(PID_get_endpoint, HTTP_POST, RestApi::Pid_get);
			server->on(PID_set_endpoint, HTTP_POST, RestApi::Pid_set);
		}

		if (moduleController.get(AvailableModules::led) != nullptr)
		{
			log_i("add led endpoints");
			server->on(ledarr_act_endpoint, HTTP_POST, RestApi::Led_act);
			server->on(ledarr_get_endpoint, HTTP_GET, RestApi::Led_get);
			server->on(ledarr_set_endpoint, HTTP_POST, RestApi::Led_set);
		}

		if (moduleController.get(AvailableModules::slm) != nullptr)
		{
			log_i("add slm endpoints");
			server->on(slm_act_endpoint, HTTP_POST, RestApi::Slm_act);
			server->on(slm_get_endpoint, HTTP_POST, RestApi::Slm_get);
			server->on(slm_set_endpoint, HTTP_POST, RestApi::Slm_set);
		}
		log_i("Setting up HTTP Routing END");
	}
}
