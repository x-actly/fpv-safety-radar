#ifndef RADAR_WIFI_MANAGER_H
#define RADAR_WIFI_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "aircraft.h"

class RadarWiFiManager {
public:
    RadarWiFiManager();
    
    // Initialize Wi-Fi connection & start GPS Web Server + UDP Listener
    bool init(double &currentLat, double &currentLon);
    
    // Process incoming HTTP requests & UDP GPS packets (Pixel 10 GPS sync)
    void handleClient();
    
    // Connect using saved credentials from NVS
    bool connectStoredWiFi();
    
    // Connect using provided SSID and Password and save to NVS
    bool connectWithCredentials(const String &ssid, const String &pass);
    
    // Scan available surrounding 2.4GHz Wi-Fi networks
    int scanNetworks(std::vector<String> &ssidList, std::vector<int> &rssiList, std::vector<bool> &encryptedList);
    
    // IP Geolocation fallback
    bool fetchIpGeolocation(double &lat, double &lon);
    
    // NVS Preferences helper methods
    void saveCredentials(const String &ssid, const String &pass);
    String getSavedSSID();
    String getSavedPassword();
    
    // Get current live position (from IP, HTTP, or UDP GPS)
    void getPosition(double &lat, double &lon) { lat = m_lat; lon = m_lon; }
    
    // Status helpers
    bool isConnected();
    int getRSSI();
    String getIPAddress();
    String getSSID();
    bool hasPixelGps() const { return m_hasPixelGps && (millis() - m_lastGpsUpdateMs < 15000); }
    float getHeadingDeg() const { return m_headingDeg; }
    bool hasCompass() const { return m_hasCompass && (millis() - m_lastGpsUpdateMs < 15000); }
    
    // Alert Configuration
    float getAlertDistKm() const { return m_alertDistKm; }
    float getAlertAltM() const { return m_alertAltM; }
    void setAlertConfig(float dist, float alt);

private:
    void setupGpsServices();
    void processUdpGps();
    void parseNmeaSentence(const String &nmea);
    
    Preferences m_prefs;
    WebServer   m_server;
    WiFiUDP     m_udp;
    double      m_lat;
    double      m_lon;
    float       m_headingDeg;
    bool        m_hasCompass;
    bool        m_hasPixelGps;
    uint32_t    m_lastGpsUpdateMs;
    float       m_alertDistKm;
    float       m_alertAltM;
};

#endif // RADAR_WIFI_MANAGER_H
