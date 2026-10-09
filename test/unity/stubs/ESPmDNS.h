// Host stub: ESPmDNS.h — no-op mDNS responder.
#pragma once
#include <cstdint>

class MDNSResponder {
public:
    bool begin(const char* hostname) { (void)hostname; return true; }
};

extern MDNSResponder MDNS;
