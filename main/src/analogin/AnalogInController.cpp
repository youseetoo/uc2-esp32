#include "../../config.h"

#include "AnalogInController.h"

AnalogInController::AnalogInController(/* args */){};
AnalogInController::~AnalogInController(){};

void AnalogInController::loop() {}

// Custom function accessible by the API
void AnalogInController::act() {

  // here you can do something
  log_i("readanalogin_act_fct");
  int readanaloginID = (int)(*WifiController::getJDoc())["readanaloginID"];
  int mnanaloginavg = nanaloginavg;
  if (WifiController::getJDoc()->containsKey("nanaloginavg"))
    mnanaloginavg = (int)(*WifiController::getJDoc())["nanaloginavg"];
  int analoginpin = 0 ;

  log_i("readanaloginID %i", readanaloginID);
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
  for (int imeas=0; imeas < nanaloginavg; imeas++) {
    analoginValueAvg += analogRead(analoginpin);
  }
  float returnValue = (float)analoginValueAvg / (float)nanaloginavg;

  WifiController::getJDoc()->clear();
  (*WifiController::getJDoc())["analoginVAL"] = returnValue;
  (*WifiController::getJDoc())["readanaloginPIN"] = analoginpin;
  (*WifiController::getJDoc())["nanaloginavg"] = nanaloginavg;
  (*WifiController::getJDoc())["readanaloginID"] = readanaloginID;
}



void AnalogInController::get() {
  log_i("readanalogin_set_fct");
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
    case 3:
      readanaloginPIN = pins.analogin_PIN_3;
      break;      
  }

  WifiController::getJDoc()->clear();
  (*WifiController::getJDoc())["readanaloginPIN"] = readanaloginPIN;
  (*WifiController::getJDoc())["readanaloginID"] = readanaloginID;
  (*WifiController::getJDoc())["nanaloginavg"] = nanaloginavg;
}



// Custom function accessible by the API
void AnalogInController::set() {
log_i("readanalogin_get_fct");
  int readanaloginID = (int)(*WifiController::getJDoc())["readanaloginID"];
  int readanaloginPIN = (int)(*WifiController::getJDoc())["readanaloginPIN"];
  nanaloginavg = (int)(*WifiController::getJDoc())["nanaloginavg"];
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
    case 3:
      pins.analogin_PIN_3 = readanaloginPIN;
      break;      
  }

  WifiController::getJDoc()->clear();
  (*WifiController::getJDoc())["nanaloginavg"] = nanaloginavg;
  (*WifiController::getJDoc())["readanaloginPIN"] = readanaloginPIN;
  (*WifiController::getJDoc())["readanaloginID"] = readanaloginID;
}


void AnalogInController::setup(){
  log_i("Setting up analogins...");
}

