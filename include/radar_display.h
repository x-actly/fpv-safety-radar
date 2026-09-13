#ifndef RADAR_DISPLAY_H
#define RADAR_DISPLAY_H

#include <Arduino.h>
#include <vector>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "aircraft.h"
#include "cyd_pinout.h"
#include "wifi_manager.h"

enum UiState {
    STATE_MAIN_MENU = 0,
    STATE_RADAR = 1,
    STATE_WIFI_SCAN = 2,
    STATE_WIFI_KEYBOARD = 3
};

enum FilterMode {
    FILTER_ALL = 0,
    FILTER_ADSB_ONLY = 1,
    FILTER_OGN_ONLY = 2
};

class RadarDisplay {
public:
    RadarDisplay();
    
    // Hardware setup
    void init();
    
    // Draw welcome / connecting screen
    void drawBootScreen(const String &statusMsg);
    
    // Render Screens
    void renderMainMenuScreen(bool wifiConnected, const String &ipAddress, double lat, double lon, bool hasPixelGps);
    void render(double centerLat, double centerLon, const std::vector<Aircraft> &targets, bool wifiConnected, int rssi, const String &ipAddress, bool hasPixelGps = false, float phoneHeadingDeg = 0.0f, bool hasCompass = false);
    
    // Render Wi-Fi Scanner & Keyboard screens
    void renderWiFiScanScreen(const std::vector<String> &ssids, const std::vector<int> &rssis, const std::vector<bool> &encs);
    void renderKeyboardScreen(const String &targetSsid, const String &typedPass);
    
    // Process touch events based on active UI state
    void handleTouch(std::vector<Aircraft> &targets, RadarWiFiManager &wifiMgr, double &outLat, double &outLon);
    
    // Getters / Setters
    UiState getUiState() const { return m_uiState; }
    void setUiState(UiState st) { m_uiState = st; }
    float getRangeKm() const { return m_rangeKm; }
    FilterMode getFilterMode() const { return m_filterMode; }
    String getSelectedSsid() const { return m_selectedSsid; }
    String getTypedPassword() const { return m_typedPassword; }
    float getAlertDistKm() const { return m_alertDistKm; }
    float getAlertAltM() const { return m_alertAltM; }
    void setAlertConfig(float dist, float alt) { m_alertDistKm = dist; m_alertAltM = alt; }

private:
    void drawRadarScope(int centerX, int centerY, int radius, int totalTrafficCount);
    void drawSweepLine(int centerX, int centerY, int radius, float angleDeg);
    void drawTargets(int centerX, int centerY, int radius, const std::vector<Aircraft> &targets);
    void drawHeaderBar(bool wifiConnected, int rssi, int adsbCount, int ognCount, bool hasPixelGps = false, float phoneHeadingDeg = 0.0f, bool hasCompass = false);
    void drawBottomToolbar();
    void drawTargetDetailModal(const Aircraft &ac);
    
    TFT_eSPI m_tft;
    TFT_eSprite m_spr;
    XPT2046_Touchscreen m_touch;

    UiState m_uiState;
    float m_rangeKm;
    FilterMode m_filterMode;
    float m_alertDistKm;
    float m_alertAltM;
    float m_sweepAngleDeg;
    uint32_t m_lastRenderMs;
    
    int m_selectedTargetIndex;
    uint32_t m_selectedTargetExpireMs;
    uint32_t m_lastTouchMs;
    
    // On-screen Wi-Fi Scanner & Keyboard state
    String m_selectedSsid;
    String m_typedPassword;
    bool m_shiftActive;
    bool m_symbolsActive;
};

#endif // RADAR_DISPLAY_H
