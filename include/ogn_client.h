#ifndef OGN_CLIENT_H
#define OGN_CLIENT_H

#include <Arduino.h>
#include <vector>
#include <WiFiClient.h>
#include "aircraft.h"

class OgnClient {
public:
    OgnClient();
    ~OgnClient();
    
    // Non-blocking processing of incoming OGN APRS TCP lines
    void process(double centerLat, double centerLon, float radiusKm, std::vector<Aircraft> &targetList);
    
    bool isConnected();
    uint16_t getFetchedCount() const { return m_fetchedCount; }

private:
    bool connectServer(double centerLat, double centerLon, float radiusKm);
    void parseAprsPacket(const String &line, double centerLat, double centerLon, float radiusKm, std::vector<Aircraft> &targetList);
    
    WiFiClient m_tcpClient;
    uint32_t m_lastReconnectMs;
    uint32_t m_lastKeepAliveMs;
    uint16_t m_fetchedCount;
    String m_rxBuffer;
    double m_currentLat;
    double m_currentLon;
    float m_currentRadius;
};

#endif // OGN_CLIENT_H
