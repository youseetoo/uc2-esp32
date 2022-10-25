#include "../../config.h"

#include "AnalogInController.h"

AnalogInController::AnalogInController(/* args */){};
AnalogInController::~AnalogInController(){};

void AnalogInController::loop() {}

// Custom function accessible by the API
void AnalogInController::act() {

  // here you can do something
  if (DEBUG) Serial.println("readanalogin_act_fct");
  int readanaloginID = (int)(*WifiController::getJDoc())["readanaloginID"];
  int mN_analogin_avg = N_analogin_avg;
  if (WifiController::getJDoc()->containsKey("N_analogin_avg"))
    mN_analogin_avg = (int)(*WifiController::getJDoc())["N_analogin_avg"];
  int analoginpin = 0 ;

  if (DEBUG) Serial.print("readanaloginID "); Serial.println(readanaloginID);
  switch (readanaloginID) {
    case 0:
      analoginpin = pins.analogin_PIN_0;
      break;
    case 1:
      analoginpin = pins.analogin_PIN_1;
      break;
    case 2:
      analoginpin = pins.analogin_PIN_2;
      break;
  }

  float analoginValueAvg = 0;
  for (int imeas=0; imeas < N_analogin_avg; imeas++) {
    analoginValueAvg += analogRead(analoginpin);
  }
  float returnValue = (float)analoginValueAvg / (float)N_analogin_avg;

  WifiController::getJDoc()->clear();
  (*WifiController::getJDoc())["analoginValue"] = returnValue;
  (*WifiController::getJDoc())["analoginpin"] = analoginpin;
  (*WifiController::getJDoc())["N_analogin_avg"] = N_analogin_avg;

}



void AnalogInController::get() {
  if (DEBUG) Serial.println("readanalogin_set_fct");
  int readanaloginID = (int)(*WifiController::getJDoc())["readanaloginID"];
  int readanaloginPIN = (int)(*WifiController::getJDoc())["readanaloginPIN"];
  if (WifiController::getJDoc()->containsKey("N_analogin_avg"))
    N_analogin_avg = (int)(*WifiController::getJDoc())["N_analogin_avg"];

  switch (readanaloginID) {
    case 0:
      pins.analogin_PIN_0 = readanaloginPIN;
      break;
    case 1:
      pins.analogin_PIN_1 = readanaloginPIN;
      break;
    case 2:
      pins.analogin_PIN_2 = readanaloginPIN;
      break;
  }


  WifiController::getJDoc()->clear();
  (*WifiController::getJDoc())["readanaloginPIN"] = readanaloginPIN;
  (*WifiController::getJDoc())["readanaloginID"] = readanaloginID;
}



// Custom function accessible by the API
void AnalogInController::set() {
if (DEBUG) Serial.println("readanalogin_get_fct");
  int readanaloginID = (int)(*WifiController::getJDoc())["readanaloginID"];
  int readanaloginPIN = 0;
  switch (readanaloginID) {
    case 0:
      readanaloginPIN = pins.analogin_PIN_0;
      break;
    case 1:
      readanaloginPIN = pins.analogin_PIN_1;
      break;
    case 2:
      readanaloginPIN = pins.analogin_PIN_2;
      break;
  }

  WifiController::getJDoc()->clear();
  (*WifiController::getJDoc())["N_analogin_avg"] = N_analogin_avg;
  (*WifiController::getJDoc())["readanaloginPIN"] = readanaloginPIN;
  (*WifiController::getJDoc())["readanaloginID"] = readanaloginID;
}


void AnalogInController::setup(){
  if(DEBUG) Serial.println("Setting up analogins...");
}

