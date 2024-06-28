#include "ConfigController.h"
#include "esp_log.h"
#include "Preferences.h"
#include "JsonKeys.h"
#include "nvs_flash.h"
#include "PinConfig.h"
namespace Config
{

	Preferences preferences;

	const char *prefNamespace = "UC2";

	void setWifiConfig(WifiConfig *config)
	{
		#ifdef WIFI
		preferences.begin(prefNamespace, false);
		preferences.putBytes(keyWifiSSID, config, sizeof(WifiConfig));
		preferences.end();
		#endif
	}

	WifiConfig *getWifiConfig()
	{
		#ifdef WIFI
		preferences.begin(prefNamespace, false);
		WifiConfig *conf = new WifiConfig();
		preferences.getBytes(keyWifiSSID, conf, sizeof(WifiConfig));
		preferences.end();
		return conf;
		#else
		WifiConfig *conf = new WifiConfig();
		return conf;
		#endif
	}

	void setup()
	{
		nvs_flash_init();
		log_d("Setup ConfigController");
		log_d("Using PinConfig: %s", pinConfig.pindefName);
		log_d("Compile time: %s %s", __DATE__, __TIME__);
	}

	bool resetPreferences()
	{
		log_i("resetPreferences");
		preferences.clear();
		return true;
	}

	void setPsxMac(char* mac)
	{
		preferences.begin(prefNamespace, true);
		preferences.putString("mac", mac);
		preferences.end();
	}

	char* getPsxMac()
	{
		preferences.begin(prefNamespace, true);
		String macString = preferences.getString("mac", "00:00:00:00:00:00");
		char* m = new char[macString.length() + 1];
		strcpy(m, macString.c_str());
		
		return m;
	}

	int getPsxControllerType()
	{
		preferences.begin(prefNamespace, true);
		int m = preferences.getInt("controllertype");
		preferences.end();
		return m;
	}

	void setPsxControllerType(int type)
	{
		preferences.begin(prefNamespace, true);
		preferences.putInt("controllertype", type);
		preferences.end();
	}
}
