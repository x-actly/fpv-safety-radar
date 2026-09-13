#include <Arduino.h>
#include <vector>
#include "cyd_pinout.h"
#include "aircraft.h"
#include "wifi_manager.h"
#include "adsb_client.h"
#include "ogn_client.h"
#include "radar_display.h"

// Global Modules & State
RadarWiFiManager wifiMgr;
AdsbClient       adsbClient;
OgnClient        ognClient;
RadarDisplay     radarDisplay;

std::vector<Aircraft> targetList;

double g_currentLat = 51.1657; // Default center of Germany
double g_currentLon = 10.4515;

uint32_t g_lastAdsbFetchMs = 0;
uint32_t g_lastPruneMs = 0;

void cleanupStaleTargets() {
    uint32_t now = millis();
    for (auto it = targetList.begin(); it != targetList.end(); ) {
        if (now - it->lastSeenMs > 60000) {
            it = targetList.erase(it);
        } else {
            it->distanceKm = calculateDistanceKm(g_currentLat, g_currentLon, it->lat, it->lon);
            it->bearingDeg  = calculateBearingDeg(g_currentLat, g_currentLon, it->lat, it->lon);
            ++it;
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESP32 CYD FPV Safety Radar Starting ---");
    
    radarDisplay.init();
    radarDisplay.drawBootScreen("Checking Wi-Fi...");
    
    cydBeep(1200, 80);
    cydBeep(1800, 100);
    
    // Check if stored credentials exist and connect
    if (wifiMgr.init(g_currentLat, g_currentLon)) {
        Serial.printf("[WIFI] Connected to %s! IP: %s\n", wifiMgr.getSSID().c_str(), wifiMgr.getIPAddress().c_str());
        radarDisplay.setAlertConfig(wifiMgr.getAlertDistKm(), wifiMgr.getAlertAltM());
        
        char bootMsg[64];
        snprintf(bootMsg, sizeof(bootMsg), "IP: %s", wifiMgr.getIPAddress().c_str());
        radarDisplay.drawBootScreen(bootMsg);
        delay(600);
        radarDisplay.setUiState(STATE_MAIN_MENU);
    } else {
        Serial.println("[WIFI] No saved network. Launching On-Screen Wi-Fi Scanner...");
        radarDisplay.drawBootScreen("Scanning Wi-Fi...");
        
        std::vector<String> ssids;
        std::vector<int> rssis;
        std::vector<bool> encs;
        wifiMgr.scanNetworks(ssids, rssis, encs);
        radarDisplay.renderWiFiScanScreen(ssids, rssis, encs);
        radarDisplay.setUiState(STATE_WIFI_SCAN);
    }
}

void loop() {
    uint32_t now = millis();
    
    // Handle incoming HTTP/UDP requests (e.g. Pixel 10 GPS sync & UDP NMEA stream)
    wifiMgr.handleClient();
    
    // Synchronize Alert Config
    radarDisplay.setAlertConfig(wifiMgr.getAlertDistKm(), wifiMgr.getAlertAltM());
    
    // Update live location from Pixel 10 GPS or IP
    wifiMgr.getPosition(g_currentLat, g_currentLon);
    
    // 1. Handle Touch Screen Interaction
    radarDisplay.handleTouch(targetList, wifiMgr, g_currentLat, g_currentLon);
    
    // 2. Perform rendering and operations based on active UI state
    if (radarDisplay.getUiState() == STATE_MAIN_MENU) {
        radarDisplay.renderMainMenuScreen(wifiMgr.isConnected(), wifiMgr.getIPAddress(), g_currentLat, g_currentLon, wifiMgr.hasPixelGps());
    } else if (radarDisplay.getUiState() == STATE_RADAR) {
        // Fetch ADS-B Air Traffic REST API (every 10 seconds)
        if (now - g_lastAdsbFetchMs > 10000) {
            g_lastAdsbFetchMs = now;
            adsbClient.updateTraffic(g_currentLat, g_currentLon, radarDisplay.getRangeKm(), targetList);
        }
        
        // Process Live OGN / FLARM APRS Stream (Non-blocking TCP socket)
        ognClient.process(g_currentLat, g_currentLon, radarDisplay.getRangeKm(), targetList);
        
        // Prune Stale Targets (every 5 seconds)
        if (now - g_lastPruneMs > 5000) {
            g_lastPruneMs = now;
            cleanupStaleTargets();
        }
        
        // Render Radar Display
        radarDisplay.render(g_currentLat, g_currentLon, targetList, wifiMgr.isConnected(), wifiMgr.getRSSI(), wifiMgr.getIPAddress(), wifiMgr.hasPixelGps(), wifiMgr.getHeadingDeg(), wifiMgr.hasCompass());
    }
    
    delay(30);
}
