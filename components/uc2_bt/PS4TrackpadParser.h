#pragma once

#include "BtController.h"

namespace BtController {

// Extended PS4 packet parser that extracts trackpad data
class PS4TrackpadParser {
public:
    // PS4 HID report trackpad data offsets (for standard PS4 controller)
    static const uint8_t TRACKPAD_DATA_OFFSET = 33;
    static const uint8_t TRACKPAD_COUNTER_OFFSET = 32;
    
    // Trackpad resolution (PS4 touchpad is 1920x943 pixels)
    static const uint16_t TRACKPAD_MAX_X = 1920;
    static const uint16_t TRACKPAD_MAX_Y = 943;
    
    // Parse trackpad data from PS4 HID report
    static TrackpadData parseTrackpadData(const uint8_t* packet, size_t packetSize) {
        TrackpadData trackpadData = {};
        
        if (packet == nullptr || packetSize < TRACKPAD_DATA_OFFSET + 6) {
            return trackpadData; // Return empty data if packet is invalid
        }
        
        // Extract report counter
        trackpadData.reportCounter = packet[TRACKPAD_COUNTER_OFFSET];
        
        // Extract first touch point
        if (packetSize >= TRACKPAD_DATA_OFFSET + 3) {
            uint8_t touch1Data[3];
            touch1Data[0] = packet[TRACKPAD_DATA_OFFSET];
            touch1Data[1] = packet[TRACKPAD_DATA_OFFSET + 1];
            touch1Data[2] = packet[TRACKPAD_DATA_OFFSET + 2];
            
            trackpadData.touch1.id = touch1Data[0] & 0x7F;
            trackpadData.touch1.isActive = (touch1Data[0] & 0x80) == 0; // Active when bit 7 is 0
            trackpadData.touch1.x = touch1Data[1] | ((touch1Data[2] & 0x0F) << 8);
            trackpadData.touch1.y = (touch1Data[2] >> 4) | (packet[TRACKPAD_DATA_OFFSET + 3] << 4);
        }
        
        // Extract second touch point
        if (packetSize >= TRACKPAD_DATA_OFFSET + 6) {
            uint8_t touch2Data[3];
            touch2Data[0] = packet[TRACKPAD_DATA_OFFSET + 4];
            touch2Data[1] = packet[TRACKPAD_DATA_OFFSET + 5];
            touch2Data[2] = packet[TRACKPAD_DATA_OFFSET + 6];
            
            trackpadData.touch2.id = touch2Data[0] & 0x7F;
            trackpadData.touch2.isActive = (touch2Data[0] & 0x80) == 0; // Active when bit 7 is 0
            trackpadData.touch2.x = touch2Data[1] | ((touch2Data[2] & 0x0F) << 8);
            trackpadData.touch2.y = (touch2Data[2] >> 4) | (packet[TRACKPAD_DATA_OFFSET + 7] << 4);
        }
        
        return trackpadData;
    }
    
    // Normalize coordinates to a more manageable range (0-1000)
    static void normalizeCoordinates(TouchData& touch) {
        if (touch.isActive) {
            touch.x = (touch.x * 1000) / TRACKPAD_MAX_X;
            touch.y = (touch.y * 1000) / TRACKPAD_MAX_Y;
        }
    }
};

} // namespace BtController
