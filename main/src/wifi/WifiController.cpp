#include "WifiController.h"

namespace RestApi
{
	DynamicJsonDocument scanWifi()
	{
		log_i("scanWifi");
		int networkcount = WiFi.scanNetworks();
		if (networkcount == -1)
		{
			while (true)
				;
		}
		DynamicJsonDocument doc(4096);
		for (int i = 0; i < networkcount; i++)
		{
			doc["ssid"].add(WiFi.SSID(i));
		}
		serialize(doc);
		return doc;
	}

	void connectToWifi()
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		serialize(w->connect(deserialize()));
	}

	void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
	{
		if (type == WStype_DISCONNECTED)
			log_i("[%u] Disconnected!\n", num);
		else if (type == WStype_CONNECTED)
		{
			WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
			IPAddress ip = w->getSocket()->remoteIP(num);
			log_i("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
		}
		else if (type == WStype_TEXT)
		{
			log_i("[%u] get Text: %s\n", num, payload);
			DynamicJsonDocument doc(4096);
			deserializeJson(doc, payload);
			if (doc.containsKey(keyLed) && moduleController.get(AvailableModules::led) != nullptr)
				moduleController.get(AvailableModules::led)->act(doc);
			if (doc.containsKey(key_motor) && moduleController.get(AvailableModules::motor) != nullptr)
				moduleController.get(AvailableModules::motor)->act(doc);
		}
	}

	void getIndexPage()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/index.html", "r");
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->getServer()->streamFile(file, "text/html");
		file.close();
		SPIFFS.end();
	}

	void getjquery()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/jquery-3.6.1.min.js", "r");
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->getServer()->streamFile(file, "text/javascript");
		file.close();
		SPIFFS.end();
	}
	void getjs()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/script.js", "r");
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->getServer()->streamFile(file, "text/javascript");
		file.close();
		SPIFFS.end();
	}

	void getCSS()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/styles.css", "r");
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->getServer()->streamFile(file, "text/css");
		file.close();
		SPIFFS.end();
	}

	void getOtaIndex()
	{
		SPIFFS.begin();
		File file = SPIFFS.open("/ota.html", "r");
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->getServer()->streamFile(file, "text/html");
		file.close();
		SPIFFS.end();
	}
}

void processWebSocketTask(void *p)
{
	WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
	for (;;)
	{
		if (w->getSocket() != nullptr)
			w->getSocket()->loop();
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}

void processHttpTask(void *p)
{
	WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
	for (;;)
	{
		if (w->getServer() != nullptr)
			w->getServer()->handleClient();
		vTaskDelay(5 / portTICK_PERIOD_MS);
	}
}

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

WebServer *WifiController::getServer()
{
	return server;
}

WebSocketsServer *WifiController::getSocket()
{
	return webSocket;
}

void WifiController::createTasks()
{
	xTaskCreate(&processHttpTask, "http_task", 4096, NULL, 5, &httpTaskHandle);
	xTaskCreate(&processWebSocketTask, "socket_task", 4096, NULL, 5, &socketTaskHandle);
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
	WifiController::begin();
	createTasks();
	doc.clear();
	return doc;
}

void WifiController::sendJsonWebSocketMsg(DynamicJsonDocument doc)
{
	// log_i("socket broadcast");
	String s;
	serializeJson(doc, s);
	webSocket->broadcastTXT(s.c_str());
}

void WifiController::setWifiConfig(String SSID, String PWD, bool ap)
{
	// log_i("mssid:%s pw:%s ap:%s", pinConfig.mSSID, pinConfig.mPWD, pinConfig.mAP);
	config->mSsid = SSID;
	config->pw = PWD;
	config->ap = ap;
	Config::setWifiConfig(config);
}

void WifiController::createAp(String ssid, String password)
{
	// This creates an access point with the specified name and password

	WiFi.disconnect();
	log_i("Ssid %s pw %s", ssid, password);

	// load default config
	if (ssid.isEmpty())
	{
		log_i("Ssid empty, start Uc2 open softap");
		WiFi.softAP(pinConfig.mSSIDAP.c_str());
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
}

void WifiController::setup()
{
	// initialize the Wifi module
	log_d("Setup Wifi");
	// retrieve Wifi Settings from Config (e.g. AP or SSId settings)
	
	/* FIXME: Why would this be necessary @killerink
	if (socketTaskHandle != nullptr)
		vTaskDelete(socketTaskHandle);
	if (httpTaskHandle != nullptr)
		vTaskDelete(httpTaskHandle);
	*/
	config = Config::getWifiConfig();

	if ((pinConfig.mSSID != nullptr) and (pinConfig.mPWD != nullptr))
		log_i("mssid:%s pw:%s", pinConfig.mSSID, pinConfig.mPWD); //, pinConfig.mAP);

	// if the server is already open => close it
	if (server != nullptr)
		server->close();

	// if the webSocket is already open => close it
	if (webSocket != nullptr)
		webSocket->close();

	// load default settings for Wifi AP
	if (pinConfig.mSSID == "")
	{
		log_i("No SSID is given: Create AP with default credentials Uc2 and no password");
		config->ap = true;
		createAp(pinConfig.mSSIDAP, pinConfig.mPWD);
	}
	else if (pinConfig.mAP)
	{
		log_i("AP is true: Create AP with default credentials Uc2 and no password");
		createAp(pinConfig.mSSID, pinConfig.mPWD);
	}
	else
	{
		// if the Wifi is not in AP mode => connect to an available Wifi Hotspot
		WiFi.softAPdisconnect();
		log_i("Connect to:%s", pinConfig.mSSID);
		WiFi.waitForConnectResult();
		WiFi.begin(pinConfig.mSSID.c_str(), pinConfig.mPWD.c_str());

		// wait for connection 5-times, if not connected => start AP
		int nConnectTrials = 0;
		int nConnectTrialsMax = 20;
		while (WiFi.status() != WL_CONNECTED && nConnectTrials <= nConnectTrialsMax)
		{
			log_i("Wait for connection");
			delay(500);
			nConnectTrials++;
		}
		if (nConnectTrials >= nConnectTrialsMax)
		{
			log_i("failed to connect,Start softap");
			config->ap = true;
			config->mSsid = pinConfig.mSSIDAP;
			config->pw = "";
			createAp(pinConfig.mSSIDAP, "");
		}
		else
		{
			log_i("Connected. IP: %s", WiFi.localIP());
		}
	}

	// initialize the webserver
	if (server == nullptr)
		server = new WebServer(80);

	// initialize the webSocket
	if (webSocket == nullptr)
		webSocket = new WebSocketsServer(81);
}

void WifiController::loop()
{
}

int WifiController::act(DynamicJsonDocument doc) {return 1;}
DynamicJsonDocument WifiController::get(DynamicJsonDocument doc) {return doc;}

void WifiController::restartWebServer()
{
	if (server != nullptr)
	{
		server->close();
	}
	setup_routing();
	server->begin();
}

void WifiController::begin()
{
	webSocket->begin();
	webSocket->onEvent(RestApi::webSocketEvent);
	setup_routing();
	server->begin();
}

void WifiController::setup_routing()
{
	log_i("Setting up HTTP Routing");
	server->onNotFound(RestApi::handleNotFound);

	server->on(features_endpoint, RestApi::getEndpoints);

	server->on(ota_endpoint, HTTP_GET, RestApi::getOtaIndex);
	/*handling uploading firmware file */
	server->on(update_endpoint, HTTP_POST, RestApi::update, RestApi::upload);

	server->on("/", HTTP_GET, RestApi::getIndexPage);
	server->on("/styles.css", HTTP_GET, RestApi::getCSS);
	server->on("/script.js", HTTP_GET, RestApi::getjs);
	server->on("/jquery-3.6.1.min.js", HTTP_GET, RestApi::getjquery);
	server->on(scanwifi_endpoint, HTTP_GET, RestApi::scanWifi);
	server->on(connectwifi_endpoint, HTTP_POST, RestApi::connectToWifi);
	server->on(reset_nv_flash_endpoint, HTTP_GET, RestApi::resetNvFLash);

	server->on(bt_scan_endpoint, HTTP_GET, RestApi::Bt_startScan);
	server->on(bt_connect_endpoint, HTTP_POST, RestApi::Bt_connect);
	server->on(bt_remove_endpoint, HTTP_POST, RestApi::Bt_remove);
	server->on(bt_paireddevices_endpoint, HTTP_GET, RestApi::Bt_getPairedDevices);

	server->on(modules_get_endpoint, HTTP_GET, RestApi::getModules);

	// POST
	if (moduleController.get(AvailableModules::motor) != nullptr)
	{
		log_i("add motor endpoints");
		server->on(motor_act_endpoint, HTTP_POST, RestApi::FocusMotor_act);
		server->on(motor_get_endpoint, HTTP_GET, RestApi::FocusMotor_get);
	}
	if (moduleController.get(AvailableModules::rotator) != nullptr)
	{
		log_i("add rotator endpoints");
		server->on(rotator_act_endpoint, HTTP_POST, RestApi::Rotator_act);
		server->on(rotator_get_endpoint, HTTP_GET, RestApi::Rotator_get);
	}
	if (moduleController.get(AvailableModules::state) != nullptr)
	{
		log_i("add state endpoints");
		server->on(state_act_endpoint, HTTP_POST, RestApi::State_act);
		server->on(state_get_endpoint, HTTP_GET, RestApi::State_get);
	}	

	if (moduleController.get(AvailableModules::dac) != nullptr)
	{
		log_i("add dac endpoints");
		server->on(dac_act_endpoint, HTTP_POST, RestApi::Dac_act);
		server->on(dac_get_endpoint, HTTP_POST, RestApi::Dac_get);
	}

	if (moduleController.get(AvailableModules::laser) != nullptr)
	{
		log_i("add laser endpoints");
		server->on(laser_get_endpoint, HTTP_GET, RestApi::Laser_get);
		server->on(laser_act_endpoint, HTTP_POST, RestApi::Laser_act);
	}

	if (moduleController.get(AvailableModules::analogout) != nullptr)
	{
		log_i("add analogout endpoints");
		server->on(analogout_act_endpoint, HTTP_POST, RestApi::AnalogOut_act);
		server->on(analogout_get_endpoint, HTTP_POST, RestApi::AnalogOut_get);
	}

	if (moduleController.get(AvailableModules::digitalout) != nullptr)
	{
		log_i("add digitalout endpoints");
		server->on(digitalout_act_endpoint, HTTP_POST, RestApi::DigitalOut_act);
		server->on(digitalout_get_endpoint, HTTP_POST, RestApi::DigitalOut_get);
	}

	if (moduleController.get(AvailableModules::digitalin) != nullptr)
	{
		log_i("add digitalin endpoints");
		server->on(digitalin_act_endpoint, HTTP_POST, RestApi::DigitalIn_act);
		server->on(digitalin_get_endpoint, HTTP_POST, RestApi::DigitalIn_get);
	}

	if (moduleController.get(AvailableModules::pid) != nullptr)
	{
		log_i("add pid endpoints");
		server->on(PID_act_endpoint, HTTP_POST, RestApi::Pid_act);
		server->on(PID_get_endpoint, HTTP_POST, RestApi::Pid_get);
	}

	if (moduleController.get(AvailableModules::led) != nullptr)
	{
		log_i("add led endpoints");
		server->on(ledarr_act_endpoint, HTTP_POST, RestApi::Led_act);
		server->on(ledarr_get_endpoint, HTTP_GET, RestApi::Led_get);
	}

	if (moduleController.get(AvailableModules::analogJoystick) != nullptr)
	{
		log_i("add analog joystick endpoints");
		server->on(analog_joystick_get_endpoint, HTTP_POST, RestApi::AnalogJoystick_get);
	}
	log_i("Setting up HTTP Routing END");
}
