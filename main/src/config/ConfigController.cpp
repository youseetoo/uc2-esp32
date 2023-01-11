#include "ConfigController.h"

namespace Config
{

	Preferences preferences;

	const char *prefNamespace = "UC2";

	void setWifiConfig(WifiConfig *config)
	{
		preferences.begin(prefNamespace, false);
		preferences.putBytes(keyWifiSSID, config, sizeof(WifiConfig));
		preferences.end();
	}

	WifiConfig *getWifiConfig()
	{
		preferences.begin(prefNamespace, false);
		WifiConfig *conf = new WifiConfig();
		preferences.getBytes(keyWifiSSID, conf, sizeof(WifiConfig));
		preferences.end();
		return conf;
	}

	void setup()
	{
	}

	bool resetPreferences()
	{
		log_i("resetPreferences");
		preferences.clear();
		return true;
	}

	void setPsxMac(String mac)
	{
		preferences.begin(prefNamespace, true);
		preferences.putString("mac", mac);
		preferences.end();
	}

	String getPsxMac()
	{
		preferences.begin(prefNamespace, true);
		String m = preferences.getString("mac");
		preferences.end();
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
