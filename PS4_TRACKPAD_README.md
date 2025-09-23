# PS4 Trackpad Support for UC2-ESP32

## Overview

Das PlayStation 4 Controller Trackpad kann auf dem ESP32 genutzt werden, um Swipe-Gesten und Touch-Koordinaten zu erkennen. Diese Implementierung erweitert den bestehenden BtController um Trackpad-Funktionalität.

## Features

### 1. Touch-Koordinaten
- **Resolution**: 1920x943 Pixel (normalisiert auf 0-1000 für einfachere Verwendung)
- **Multi-Touch**: Unterstützung für bis zu 2 simultane Touch-Punkte
- **Touch-ID**: Eindeutige IDs für Touch-Tracking

### 2. Swipe-Erkennung
- **Richtungen**: Links, Rechts, Auf, Ab
- **Konfigurierbare Schwellenwerte**: Mindestdistanz und maximale Zeitdauer
- **Event-basiert**: Callback-Funktionen für Swipe-Events

## Implementierung

### Header-Erweiterungen (BtController.h)

```cpp
// Swipe-Richtungen
enum SwipeDirection {
    SWIPE_NONE = 0,
    SWIPE_LEFT = 1,
    SWIPE_RIGHT = 2,
    SWIPE_UP = 3,
    SWIPE_DOWN = 4
};

// Touch-Datenstruktur
struct TouchData {
    bool isActive;
    uint16_t x;        // 0-1000 (normalisiert)
    uint16_t y;        // 0-1000 (normalisiert)
    uint8_t id;        // Touch-ID
};

// Trackpad-Datenstruktur
struct TrackpadData {
    uint8_t reportCounter;
    TouchData touch1;
    TouchData touch2;
};

// Event-Handler
void setTrackpadSwipeEvent(void (*trackpad_swipe_event)(SwipeDirection direction));
void setTrackpadTouchEvent(void (*trackpad_touch_event)(TouchData touch1, TouchData touch2));
```

### Verwendungsbeispiel

```cpp
// In main.cpp oder einer anderen Komponente

void onTrackpadSwipe(BtController::SwipeDirection direction) {
    switch(direction) {
        case BtController::SWIPE_LEFT:
            log_i("Swipe nach links erkannt");
            // Z.B. Motor nach links bewegen
            break;
        case BtController::SWIPE_RIGHT:
            log_i("Swipe nach rechts erkannt");
            // Z.B. Motor nach rechts bewegen
            break;
        case BtController::SWIPE_UP:
            log_i("Swipe nach oben erkannt");
            // Z.B. Z-Achse nach oben bewegen
            break;
        case BtController::SWIPE_DOWN:
            log_i("Swipe nach unten erkannt");
            // Z.B. Z-Achse nach unten bewegen
            break;
    }
}

void onTrackpadTouch(BtController::TouchData touch1, BtController::TouchData touch2) {
    if (touch1.isActive) {
        log_i("Touch 1: (%d, %d)", touch1.x, touch1.y);
        // Verwende Touch-Koordinaten für präzise Steuerung
    }
    if (touch2.isActive) {
        log_i("Touch 2: (%d, %d)", touch2.x, touch2.y);
        // Zwei-Finger-Gesten, Zoom, etc.
    }
}

void setup() {
    // Registriere Trackpad-Event-Handler
    BtController::setTrackpadSwipeEvent(onTrackpadSwipe);
    BtController::setTrackpadTouchEvent(onTrackpadTouch);
}
```

## Technische Details

### PS4 HID Report Struktur

Das PS4 Trackpad sendet Daten in den HID-Reports:
- **Offset 32**: Report Counter
- **Offset 33-35**: Touch Point 1 (ID, X, Y)
- **Offset 36-38**: Touch Point 2 (ID, X, Y)

### Swipe-Algorithmus

1. **Touch Start**: Speichere Startposition und Zeit
2. **Touch Move**: Verfolge Bewegung (optional für Echtzeit-Feedback)
3. **Touch End**: Berechne Distanz und Richtung
4. **Validation**: Prüfe Mindestdistanz und maximale Zeit
5. **Event**: Sende Swipe-Event mit erkannter Richtung

### Konfigurierbare Parameter

```cpp
static const uint16_t SWIPE_THRESHOLD = 50;  // Mindestdistanz in Pixeln
static const uint32_t SWIPE_TIMEOUT = 500;   // Maximale Zeit in ms
```

## Integration in Motorkontrolle

### Beispiel: Präzise X/Y-Positionierung

```cpp
void onTrackpadTouch(BtController::TouchData touch1, BtController::TouchData touch2) {
    if (touch1.isActive) {
        // Normalisierte Koordinaten in Motorbefehle umwandeln
        float targetX = (touch1.x / 1000.0f) * MAX_X_POSITION;
        float targetY = (touch1.y / 1000.0f) * MAX_Y_POSITION;
        
        // Sende Positionierungsbefehl an LinearEncoderController
        cJSON* moveCmd = cJSON_CreateObject();
        cJSON* steppers = cJSON_CreateArray();
        
        // X-Achse Befehl
        cJSON* stepperX = cJSON_CreateObject();
        cJSON_AddNumberToObject(stepperX, "stepperid", 1);
        cJSON_AddNumberToObject(stepperX, "position", targetX);
        cJSON_AddNumberToObject(stepperX, "isabs", 1);
        cJSON_AddItemToArray(steppers, stepperX);
        
        // Y-Achse Befehl
        cJSON* stepperY = cJSON_CreateObject();
        cJSON_AddNumberToObject(stepperY, "stepperid", 2);
        cJSON_AddNumberToObject(stepperY, "position", targetY);
        cJSON_AddNumberToObject(stepperY, "isabs", 1);
        cJSON_AddItemToArray(steppers, stepperY);
        
        cJSON_AddItemToObject(moveCmd, "steppers", steppers);
        
        // Sende an LinearEncoderController
        LinearEncoderController::act(moveCmd);
        cJSON_Delete(moveCmd);
    }
}
```

### Beispiel: Swipe-basierte Motorsteuerung

```cpp
void onTrackpadSwipe(BtController::SwipeDirection direction) {
    const float SWIPE_DISTANCE = 1000.0f; // 1mm pro Swipe
    
    cJSON* moveCmd = cJSON_CreateObject();
    cJSON* steppers = cJSON_CreateArray();
    cJSON* stepper = cJSON_CreateObject();
    
    switch(direction) {
        case BtController::SWIPE_LEFT:
            cJSON_AddNumberToObject(stepper, "stepperid", 1); // X-Achse
            cJSON_AddNumberToObject(stepper, "position", -SWIPE_DISTANCE);
            cJSON_AddNumberToObject(stepper, "isabs", 0); // Relativ
            break;
        case BtController::SWIPE_RIGHT:
            cJSON_AddNumberToObject(stepper, "stepperid", 1);
            cJSON_AddNumberToObject(stepper, "position", SWIPE_DISTANCE);
            cJSON_AddNumberToObject(stepper, "isabs", 0);
            break;
        case BtController::SWIPE_UP:
            cJSON_AddNumberToObject(stepper, "stepperid", 3); // Z-Achse
            cJSON_AddNumberToObject(stepper, "position", SWIPE_DISTANCE);
            cJSON_AddNumberToObject(stepper, "isabs", 0);
            break;
        case BtController::SWIPE_DOWN:
            cJSON_AddNumberToObject(stepper, "stepperid", 3);
            cJSON_AddNumberToObject(stepper, "position", -SWIPE_DISTANCE);
            cJSON_AddNumberToObject(stepper, "isabs", 0);
            break;
    }
    
    cJSON_AddItemToArray(steppers, stepper);
    cJSON_AddItemToObject(moveCmd, "steppers", steppers);
    
    LinearEncoderController::act(moveCmd);
    cJSON_Delete(moveCmd);
}
```

## Nächste Schritte

1. **PSController-Bibliothek erweitern**: Zugang zu Raw-Packet-Daten für Trackpad-Parsing
2. **Alternative HID-Bibliothek**: ESP-IDF native HID-Implementation für vollständige Kontrolle
3. **Kalibrierung**: Touch-Koordinaten an spezifische Hardware anpassen
4. **Erweiterte Gesten**: Rotation, Zoom, Multi-Touch-Patterns

## Vorteile für UC2

- **Intuitive Steuerung**: Natürliche Touch-Gesten für Mikroskop-Steuerung
- **Präzise Positionierung**: Direkte X/Y-Koordinaten für Sample-Navigation
- **Schnelle Bewegung**: Swipes für schnelle Achsenbewegung
- **Benutzerfreundlich**: Bekannte Smartphone-ähnliche Bedienung
