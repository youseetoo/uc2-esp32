#include "ConfigController.h"
// T1.3: direct IDF (no Arduino) — uc2 façade for prefs/string/log.
// JsonKeys.h dropped (unused here; Arduino/pgmspace).
#include <uc2_hal.h>
#include <cstring>
#include "nvs_flash.h"
#include "PinConfig.h"
namespace Config
{

	static const char *TAG = "ConfigController";

	uc2::Preferences preferences;

	const char *prefNamespace = "UC2";

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
		uc2::String macString = preferences.getString("mac", "00:00:00:00:00:00");
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
