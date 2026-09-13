#ifndef ADSB_CLIENT_H
#define ADSB_CLIENT_H

#include <Arduino.h>
#include <vector>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "aircraft.h"

class AdsbClient {
public:
    AdsbClient();
    
    // Fetch surrounding traffic via ADS-B API
    bool updateTraffic(double centerLat, double centerLon, float radiusKm, std::vector<Aircraft> &targetList);
    
    // Get time of last successful update millis()
    uint32_t getLastUpdateMs() const { return m_lastUpdateMs; }
    
    // Get total fetched ADS-B count
    uint16_t getFetchedCount() const { return m_fetchedCount; }

private:
    uint32_t m_lastUpdateMs;
    uint16_t m_fetchedCount;
};

#endif // ADSB_CLIENT_H
