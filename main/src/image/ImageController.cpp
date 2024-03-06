#include "ImageController.h"

ImageController::ImageController(/* args */){};
ImageController::~ImageController(){};


void ImageController::loop() {
	// nothing to do here
}




// Custom function accessible by the API
int ImageController::act(cJSON *ob)
{ 
	// nothing to do here
	return 1;
}

// Custom function accessible by the API to get the image as base64
String ImageController::getBase64Image() {
	return image_base64;
}

cJSON *ImageController::get(cJSON *docin)
{
    // {"task": "/image_get"}
    log_i("ImageController::get");
	log_i("Length of image_base64: %d", strlen(image_base64));
	Serial.print("/{\"image_base64\": \"");
	Serial.print(image_base64);
	Serial.println("\"}");
    // return the base64 image "image_base64"
    cJSON *docout = cJSON_CreateObject();
    cJSON_AddStringToObject(docout, "success", "1");
    return docout;
}

void ImageController::setup()
{
}
