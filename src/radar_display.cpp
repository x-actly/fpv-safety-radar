#include "radar_display.h"

SPIClass touchSpi(VSPI);

RadarDisplay::RadarDisplay() 
    : m_tft(), m_spr(&m_tft), m_touch(CYD_TOUCH_CS, 255),
      m_uiState(STATE_MAIN_MENU), m_rangeKm(10.0f), m_filterMode(FILTER_ALL),
      m_alertDistKm(5.0f), m_alertAltM(200.0f),
      m_sweepAngleDeg(0.0f), m_lastRenderMs(0), m_selectedTargetIndex(-1),
      m_selectedTargetExpireMs(0), m_lastTouchMs(0), m_shiftActive(false),
      m_symbolsActive(false) {}

void RadarDisplay::init() {
    pinMode(CYD_TFT_BL, OUTPUT);
    digitalWrite(CYD_TFT_BL, HIGH);
    
    pinMode(CYD_TOUCH_CS, OUTPUT);
    digitalWrite(CYD_TOUCH_CS, HIGH);
    
    m_tft.init();
    m_tft.setRotation(1); // 320x240 Landscape
    m_tft.invertDisplay(true); // Invert display colors for CYD (Fixes white background & purple lines)
    m_tft.fillScreen(TFT_BLACK);
    
    m_spr.setColorDepth(8);
    m_spr.createSprite(320, 240);
    
    // Dedicated Touch SPI bus (VSPI: CLK=25, MISO=39, MOSI=32, CS=33)
    touchSpi.begin(CYD_TOUCH_CLK, CYD_TOUCH_MISO, CYD_TOUCH_MOSI, CYD_TOUCH_CS);
    touchSpi.setFrequency(2500000);
    m_touch.begin(touchSpi);
    m_touch.setRotation(1);
    
    cydLedOff();
}

void RadarDisplay::drawBootScreen(const String &statusMsg) {
    m_tft.fillScreen(TFT_BLACK);
    m_tft.drawRect(5, 5, 310, 230, 0x03E0);
    m_tft.drawRect(7, 7, 306, 226, 0x01E0);
    
    m_tft.setTextColor(TFT_GREEN, TFT_BLACK);
    m_tft.setTextDatum(MC_DATUM);
    m_tft.drawString("FPV SAFETY RADAR", 160, 50, 4);
    
    m_tft.setTextColor(TFT_CYAN, TFT_BLACK);
    m_tft.drawString("ADS-B & OGN / FLARM LIVE", 160, 85, 2);
    
    m_tft.drawRoundRect(20, 130, 280, 50, 6, 0x05E0);
    m_tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    m_tft.drawString(statusMsg, 160, 155, 2);
    
    m_tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    m_tft.drawString("ESP32-2432S028R CYD Edition", 160, 215, 1);
}

void RadarDisplay::render(double centerLat, double centerLon, const std::vector<Aircraft> &targets, bool wifiConnected, int rssi, const String &ipAddress, bool hasPixelGps, float phoneHeadingDeg, bool hasCompass) {
    if (m_uiState != STATE_RADAR) return;
    
    uint32_t now = millis();
    float dt = (now - m_lastRenderMs) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    m_lastRenderMs = now;
    
    m_sweepAngleDeg += 60.0f * dt;
    if (m_sweepAngleDeg >= 360.0f) m_sweepAngleDeg -= 360.0f;
    
    m_spr.fillSprite(TFT_BLACK);
    
    int centerX = 160;
    int centerY = 116;
    int radius = 94; // Perfect centered fit between header (y=16) and toolbar (y=218)
    
    int adsbCount = 0;
    int ognCount = 0;
    int visibleCount = 0;
    for (const auto &ac : targets) {
        if (m_filterMode == FILTER_ADSB_ONLY && ac.source != SOURCE_ADSB) continue;
        if (m_filterMode == FILTER_OGN_ONLY && ac.source != SOURCE_OGN_FLARM) continue;
        if (ac.distanceKm <= m_rangeKm) visibleCount++;
        
        if (ac.source == SOURCE_ADSB) adsbCount++;
        else ognCount++;
    }
    
    drawRadarScope(centerX, centerY, radius, visibleCount);
    drawSweepLine(centerX, centerY, radius, m_sweepAngleDeg);
    drawTargets(centerX, centerY, radius, targets);
    drawHeaderBar(wifiConnected, rssi, adsbCount, ognCount, hasPixelGps, phoneHeadingDeg, hasCompass);
    
    // Draw alternating IP address / GPS status in header bar
    if (wifiConnected && ipAddress.length() > 0) {
        bool showIp = ((millis() / 3000) % 2) == 1;
        if (showIp && !hasPixelGps) {
            m_spr.fillRect(0, 0, 110, 16, TFT_BLACK);
            m_spr.setTextColor(0x07E0, TFT_BLACK);
            m_spr.setTextDatum(TL_DATUM);
            m_spr.drawString(ipAddress, 5, 2, 1);
        }
    }
    
    drawBottomToolbar();
    
    if (m_selectedTargetIndex >= 0 && m_selectedTargetIndex < (int)targets.size()) {
        if (now < m_selectedTargetExpireMs) {
            drawTargetDetailModal(targets[m_selectedTargetIndex]);
        } else {
            m_selectedTargetIndex = -1;
        }
    }
    
    m_spr.pushSprite(0, 0);
}

void RadarDisplay::renderMainMenuScreen(bool wifiConnected, const String &ipAddress, double lat, double lon, bool hasPixelGps) {
    m_spr.fillSprite(TFT_BLACK);
    
    // Outer Decorative Frame
    m_spr.drawRoundRect(4, 4, 312, 232, 6, 0x03E0);
    m_spr.drawRoundRect(6, 6, 308, 228, 5, 0x01E0);
    
    // Header Title (x: 20..300, y: 10..44, height: 34px - EXACT SAME SIZE AS BUTTONS)
    m_spr.drawRoundRect(20, 10, 280, 34, 6, 0x07E0);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    m_spr.setTextDatum(TC_DATUM);
    m_spr.drawString("FPV SAFETY RADAR", 160, 17, 4); // Exact equal 7px top and 7px bottom padding inside 34px container
    
    // Button 1: START RADAR (x: 20..300, y: 50..84, height: 34px, Black BG, Green Border & Text)
    m_spr.drawRoundRect(20, 50, 280, 34, 6, 0x07E0);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    m_spr.setTextDatum(MC_DATUM);
    m_spr.drawString("START RADAR", 160, 67, 2);
    
    // Button 2: WI-FI SETTINGS (x: 20..300, y: 90..124, height: 34px, Center Y: 107)
    m_spr.drawRoundRect(20, 90, 280, 34, 6, TFT_CYAN);
    m_spr.setTextColor(TFT_CYAN, TFT_BLACK);
    m_spr.setTextDatum(MC_DATUM);
    m_spr.drawString("WI-FI SETTINGS", 160, 107, 2);
    
    // Button 3: ALERT LIMITS (x: 20..300, y: 130..164, height: 34px, Center Y: 147)
    m_spr.drawRoundRect(20, 130, 280, 34, 6, (m_alertDistKm > 0.0f) ? TFT_YELLOW : 0x0460);
    m_spr.setTextColor((m_alertDistKm > 0.0f) ? TFT_YELLOW : 0x07E0, TFT_BLACK);
    m_spr.setTextDatum(MC_DATUM);
    char alrtBuf[48];
    if (m_alertDistKm == 50.0f && m_alertAltM >= 10000.0f) {
        snprintf(alrtBuf, sizeof(alrtBuf), "ALERT LIMIT: TEST (50km / 10km)");
    } else if (m_alertDistKm > 0.0f) {
        if (m_alertAltM >= 1000.0f) {
            snprintf(alrtBuf, sizeof(alrtBuf), "ALERT LIMIT: %.0fkm / %.0fkm", m_alertDistKm, m_alertAltM / 1000.0f);
        } else {
            snprintf(alrtBuf, sizeof(alrtBuf), "ALERT LIMIT: %.0fkm / %.0fm", m_alertDistKm, m_alertAltM);
        }
    } else {
        snprintf(alrtBuf, sizeof(alrtBuf), "ALERT LIMIT: DISABLED");
    }
    m_spr.drawString(alrtBuf, 160, 147, 2);
    
    // Status Info Panel at Bottom (x: 20..300, y: 170..232, height: 62px)
    m_spr.drawRoundRect(20, 170, 280, 62, 6, 0x0300);
    m_spr.setTextDatum(TL_DATUM);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    
    char line1[48];
    if (wifiConnected) {
        snprintf(line1, sizeof(line1), "WIFI: CONNECTED (IP %s)", ipAddress.c_str());
    } else {
        snprintf(line1, sizeof(line1), "WIFI: NOT CONNECTED");
    }
    m_spr.drawString(line1, 28, 176, 1);
    
    char line2[48];
    if (hasPixelGps) {
        snprintf(line2, sizeof(line2), "GPS: SMARTPHONE STREAM ACTIVE");
    } else {
        snprintf(line2, sizeof(line2), "POS: %.4f, %.4f", lat, lon);
    }
    m_spr.drawString(line2, 28, 191, 1);
    
    m_spr.setTextColor(TFT_CYAN, TFT_BLACK);
    String companionUrl = wifiConnected ? ("http://" + ipAddress + "/gps") : "CONNECT WI-FI FOR GPS STREAM";
    m_spr.drawString("COMPANION: " + companionUrl, 28, 206, 1);
    
    m_spr.pushSprite(0, 0);
}

void RadarDisplay::renderWiFiScanScreen(const std::vector<String> &ssids, const std::vector<int> &rssis, const std::vector<bool> &encs) {
    m_spr.fillSprite(TFT_BLACK);
    
    // Header
    m_spr.fillRect(0, 0, 320, 26, 0x01E0);
    m_spr.setTextColor(TFT_BLACK, 0x01E0);
    m_spr.setTextDatum(MC_DATUM);
    m_spr.drawString("SELECT WI-FI NETWORK", 160, 13, 2);
    
    int startY = 32;
    int itemH = 26;
    
    for (size_t i = 0; i < ssids.size() && i < 6; i++) {
        int y = startY + (i * (itemH + 4));
        m_spr.drawRoundRect(10, y, 300, itemH, 4, 0x03E0);
        m_spr.fillRect(11, y + 1, 298, itemH - 2, 0x0821);
        
        m_spr.setTextColor(TFT_WHITE, 0x0821);
        m_spr.setTextDatum(ML_DATUM);
        m_spr.drawString(ssids[i].substring(0, 22), 20, y + (itemH / 2), 2);
        
        // Signal strength RSSI indicator
        m_spr.setTextDatum(MR_DATUM);
        m_spr.setTextColor(TFT_YELLOW, 0x0821);
        char rssiBuf[16];
        snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm %s", rssis[i], encs[i] ? "*" : "");
        m_spr.drawString(rssiBuf, 300, y + (itemH / 2), 1);
    }
    
    // Bottom Action Buttons
    m_spr.drawRoundRect(10, 210, 140, 26, 4, TFT_CYAN);
    m_spr.setTextColor(TFT_CYAN, TFT_BLACK);
    m_spr.setTextDatum(MC_DATUM);
    m_spr.drawString("[RESCAN]", 80, 223, 2);
    
    m_spr.drawRoundRect(170, 210, 140, 26, 4, TFT_RED);
    m_spr.setTextColor(TFT_RED, TFT_BLACK);
    m_spr.drawString("[CANCEL]", 240, 223, 2);
    
    m_spr.pushSprite(0, 0);
}

void RadarDisplay::renderKeyboardScreen(const String &targetSsid, const String &typedPass) {
    m_spr.fillSprite(TFT_BLACK);
    
    // Title & Selected SSID
    m_spr.setTextColor(TFT_CYAN, TFT_BLACK);
    m_spr.setTextDatum(TC_DATUM);
    m_spr.drawString("CONNECT TO: " + targetSsid.substring(0, 18), 160, 4, 2);
    
    // Password Input Box
    m_spr.drawRoundRect(10, 25, 300, 28, 4, TFT_YELLOW);
    m_spr.fillRect(11, 26, 298, 26, 0x0821);
    m_spr.setTextColor(TFT_GREEN, 0x0821);
    m_spr.setTextDatum(ML_DATUM);
    
    String maskedPass = typedPass + "_";
    m_spr.drawString(maskedPass, 18, 39, 2);
    
    // Touch Keyboard Layout Definition
    const char* row1 = m_symbolsActive ? "1234567890" : (m_shiftActive ? "QWERTYUIOP" : "qwertyuiop");
    const char* row2 = m_symbolsActive ? "!@#$%^&*()" : (m_shiftActive ? "ASDFGHJKL" : "asdfghjkl");
    const char* row3 = m_symbolsActive ? "-_=+[{]};:" : (m_shiftActive ? "ZXCVBNM" : "zxcvbnm");
    
    // Draw Row 1 (10 keys)
    int keyW = 28;
    int keyH = 32;
    int startY = 60;
    
    for (int i = 0; i < 10; i++) {
        int x = 5 + (i * 31);
        m_spr.drawRoundRect(x, startY, keyW, keyH, 4, 0x03E0);
        m_spr.setTextColor(TFT_WHITE, TFT_BLACK);
        m_spr.setTextDatum(MC_DATUM);
        char c[2] = {row1[i], '\0'};
        m_spr.drawString(c, x + (keyW / 2), startY + (keyH / 2), 2);
    }
    
    // Draw Row 2 (9 keys)
    startY += 36;
    for (int i = 0; i < 9; i++) {
        int x = 20 + (i * 31);
        m_spr.drawRoundRect(x, startY, keyW, keyH, 4, 0x03E0);
        m_spr.setTextColor(TFT_WHITE, TFT_BLACK);
        m_spr.setTextDatum(MC_DATUM);
        char c[2] = {row2[i], '\0'};
        m_spr.drawString(c, x + (keyW / 2), startY + (keyH / 2), 2);
    }
    
    // Draw Row 3 (Shift/Sym + 7 keys + Backspace)
    startY += 36;
    // Shift Button
    m_spr.drawRoundRect(5, startY, 36, keyH, 4, m_shiftActive ? TFT_YELLOW : 0x03E0);
    m_spr.setTextColor(m_shiftActive ? TFT_YELLOW : TFT_WHITE, TFT_BLACK);
    m_spr.drawString(m_symbolsActive ? "ABC" : "SHF", 23, startY + (keyH / 2), 1);
    
    for (int i = 0; i < 7; i++) {
        int x = 45 + (i * 31);
        m_spr.drawRoundRect(x, startY, keyW, keyH, 4, 0x03E0);
        m_spr.setTextColor(TFT_WHITE, TFT_BLACK);
        char c[2] = {row3[i], '\0'};
        m_spr.drawString(c, x + (keyW / 2), startY + (keyH / 2), 2);
    }
    
    // Backspace Button
    m_spr.drawRoundRect(266, startY, 48, keyH, 4, TFT_RED);
    m_spr.setTextColor(TFT_RED, TFT_BLACK);
    m_spr.drawString("DEL", 290, startY + (keyH / 2), 1);
    
    // Draw Row 4 (Space + Clear + Connect)
    startY += 36;
    // 123 / Symbols toggle
    m_spr.drawRoundRect(5, startY, 45, keyH, 4, 0x03E0);
    m_spr.setTextColor(TFT_CYAN, TFT_BLACK);
    m_spr.drawString(m_symbolsActive ? "abc" : "123", 27, startY + (keyH / 2), 1);
    
    // Space bar
    m_spr.drawRoundRect(55, startY, 135, keyH, 4, 0x03E0);
    m_spr.setTextColor(TFT_WHITE, TFT_BLACK);
    m_spr.drawString("SPACE", 122, startY + (keyH / 2), 1);
    
    // Connect Button
    m_spr.drawRoundRect(195, startY, 120, keyH, 4, TFT_GREEN);
    m_spr.fillRect(196, startY + 1, 118, keyH - 2, 0x03E0);
    m_spr.setTextColor(TFT_BLACK, 0x03E0);
    m_spr.drawString("CONNECT", 255, startY + (keyH / 2), 2);
    
    m_spr.pushSprite(0, 0);
}

void RadarDisplay::drawRadarScope(int centerX, int centerY, int radius, int totalTrafficCount) {
    // Outer Scope Ring (double-line phosphor green)
    m_spr.drawCircle(centerX, centerY, radius, 0x07E0);
    m_spr.drawCircle(centerX, centerY, radius - 1, 0x07E0);
    
    // Concentric Inner Range Rings
    m_spr.drawCircle(centerX, centerY, (int)(radius * 0.68f), 0x0460);
    m_spr.drawCircle(centerX, centerY, (int)(radius * 0.35f), 0x0300);
    
    // Full Crosshair Lines
    m_spr.drawFastVLine(centerX, centerY - radius, radius * 2, 0x0460);
    m_spr.drawFastHLine(centerX - radius, centerY, radius * 2, 0x0460);
    
    // Center Radar Marker
    m_spr.fillRect(centerX - 2, centerY - 2, 5, 5, 0x07E0);
    
    // Top Title: FPV SAFETY RADAR (positioned inside top of scope arc)
    m_spr.setTextDatum(TC_DATUM);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    m_spr.drawString("FPV SAFETY RADAR", centerX, centerY - radius + 4, 2);
    
    // Bottom Traffic Count: TRAFFIC: X (positioned inside bottom of scope arc)
    m_spr.setTextDatum(BC_DATUM);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    char trfStr[24];
    snprintf(trfStr, sizeof(trfStr), "TRAFFIC: %d", totalTrafficCount);
    m_spr.drawString(trfStr, centerX, centerY + radius - 4, 2);
}

void RadarDisplay::drawSweepLine(int centerX, int centerY, int radius, float angleDeg) {
    float rad = angleDeg * DEG_TO_RAD;
    int endX = centerX + (int)(sinf(rad) * radius);
    int endY = centerY - (int)(cosf(rad) * radius);
    
    m_spr.drawLine(centerX, centerY, endX, endY, 0x07E0);
    
    for (int i = 1; i <= 5; i++) {
        float trailRad = (angleDeg - (i * 2.5f)) * DEG_TO_RAD;
        int tX = centerX + (int)(sinf(trailRad) * radius);
        int tY = centerY - (int)(cosf(trailRad) * radius);
        uint16_t trailColor = (i == 1) ? 0x05E0 : ((i == 2) ? 0x0460 : ((i == 3) ? 0x0300 : 0x01E0));
        m_spr.drawLine(centerX, centerY, tX, tY, trailColor);
    }
}

void RadarDisplay::drawTargets(int centerX, int centerY, int radius, const std::vector<Aircraft> &targets) {
    bool hasCollisionAlert = false;
    String alertMsg = "";
    
    for (size_t i = 0; i < targets.size(); i++) {
        const auto &ac = targets[i];
        
        if (m_filterMode == FILTER_ADSB_ONLY && ac.source != SOURCE_ADSB) continue;
        if (m_filterMode == FILTER_OGN_ONLY && ac.source != SOURCE_OGN_FLARM) continue;
        if (ac.distanceKm > m_rangeKm) continue;
        
        float normalizedDist = ac.distanceKm / m_rangeKm;
        float bearingRad = ac.bearingDeg * DEG_TO_RAD;
        
        int tgtX = centerX + (int)(sinf(bearingRad) * normalizedDist * radius);
        int tgtY = centerY - (int)(cosf(bearingRad) * normalizedDist * radius);
        
        uint16_t color = 0x07E0; // Vintage Phosphor Green
        
        float altM = ac.altitudeFt * 0.3048f;
        
        // Dynamic Proximity & Altitude Collision Alert Check
        if (m_alertDistKm > 0.0f && ac.distanceKm <= m_alertDistKm && fabsf(altM) <= m_alertAltM) {
            color = TFT_RED;
            hasCollisionAlert = true;
            char buf[36];
            snprintf(buf, sizeof(buf), "ALARM: %s (%.1fkm %.0fm)", ac.callsign.substring(0, 6).c_str(), ac.distanceKm, altM);
            alertMsg = String(buf);
        }
        
        if (m_selectedTargetIndex == (int)i) {
            color = TFT_YELLOW;
            m_spr.drawCircle(tgtX, tgtY, 6, TFT_YELLOW);
            m_spr.drawCircle(tgtX, tgtY, 7, TFT_WHITE);
        }
        
        // Target blip dot (solid green or red if alert)
        m_spr.fillCircle(tgtX, tgtY, 3, color);
        
        // Heading vector
        float hdgRad = ac.headingDeg * DEG_TO_RAD;
        int vecX = tgtX + (int)(sinf(hdgRad) * 8.0f);
        int vecY = tgtY - (int)(cosf(hdgRad) * 8.0f);
        m_spr.drawLine(tgtX, tgtY, vecX, vecY, color);
        
        // Target Label: Line 1 = Callsign (SAS1740), Line 2 = Altitude in meters (2073m)
        m_spr.setTextColor(color, TFT_BLACK);
        m_spr.setTextDatum(TL_DATUM);
        
        String cs = ac.callsign.substring(0, 7);
        if (cs.length() == 0) cs = "TGT";
        
        char altStr[16];
        snprintf(altStr, sizeof(altStr), "%.0fm", altM);
        
        m_spr.drawString(cs, tgtX + 5, tgtY - 8, 1);
        m_spr.drawString(altStr, tgtX + 5, tgtY + 1, 1);
    }
    
    if (hasCollisionAlert) {
        cydLedSet(true, false, false);
        cydBeep(2600, 70); // Acoustic warning beep
        
        // Display Warning Banner on top of scope
        m_spr.fillRoundRect(30, 28, 260, 22, 4, TFT_RED);
        m_spr.setTextColor(TFT_WHITE, TFT_RED);
        m_spr.setTextDatum(MC_DATUM);
        m_spr.drawString(alertMsg, 160, 39, 2);
    } else {
        cydLedSet(false, true, false);
    }
}

void RadarDisplay::drawHeaderBar(bool wifiConnected, int rssi, int adsbCount, int ognCount, bool hasPixelGps, float phoneHeadingDeg, bool hasCompass) {
    m_spr.setTextDatum(TL_DATUM);
    if (wifiConnected) {
        if (hasPixelGps) {
            m_spr.setTextColor(0x07E0, TFT_BLACK); // Phosphor Green matching retro theme
            char gpsStr[24];
            if (hasCompass) {
                snprintf(gpsStr, sizeof(gpsStr), "GPS:OK %.0f\xB0", phoneHeadingDeg);
            } else {
                snprintf(gpsStr, sizeof(gpsStr), "GPS:OK");
            }
            m_spr.drawString(gpsStr, 5, 2, 1);
        } else {
            m_spr.setTextColor(0x07E0, TFT_BLACK);
            char wifiStr[16];
            snprintf(wifiStr, sizeof(wifiStr), "WIFI %ddBm", rssi);
            m_spr.drawString(wifiStr, 5, 2, 1);
        }
    } else {
        m_spr.setTextColor(TFT_RED, TFT_BLACK);
        m_spr.drawString("NO WIFI", 5, 2, 1);
    }
    
    m_spr.setTextDatum(TR_DATUM);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    char cntStr[32];
    snprintf(cntStr, sizeof(cntStr), "A:%d FLARM:%d", adsbCount, ognCount);
    m_spr.drawString(cntStr, 315, 2, 1);
}

void RadarDisplay::drawBottomToolbar() {
    // Retro Green CRT Touch Buttons at bottom (4 buttons)
    // 1. RNG (5..75)
    m_spr.drawRoundRect(4, 218, 70, 20, 3, 0x0460);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    m_spr.setTextDatum(MC_DATUM);
    char rngBtn[16];
    snprintf(rngBtn, sizeof(rngBtn), "RNG:%dkm", (int)m_rangeKm);
    m_spr.drawString(rngBtn, 39, 228, 1);
    
    // 2. FLT (78..148)
    m_spr.drawRoundRect(78, 218, 70, 20, 3, 0x0460);
    const char* fltStr = (m_filterMode == FILTER_ALL) ? "FLT: ALL" : 
                         ((m_filterMode == FILTER_ADSB_ONLY) ? "FLT: ADSB" : "FLT: OGN");
    m_spr.drawString(fltStr, 113, 228, 1);
    
    // 3. WARN Config (152..245)
    m_spr.drawRoundRect(152, 218, 93, 20, 3, (m_alertDistKm > 0.0f) ? 0x07E0 : 0x0460);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    char alrtBtn[24];
    if (m_alertDistKm == 50.0f && m_alertAltM >= 10000.0f) {
        snprintf(alrtBtn, sizeof(alrtBtn), "TEST:50k/10k");
    } else if (m_alertDistKm > 0.0f) {
        if (m_alertAltM >= 1000.0f) {
            snprintf(alrtBtn, sizeof(alrtBtn), "ALRT:%.0fk/%.0fk", m_alertDistKm, m_alertAltM / 1000.0f);
        } else {
            snprintf(alrtBtn, sizeof(alrtBtn), "ALRT:%.0fk/%.0fm", m_alertDistKm, m_alertAltM);
        }
    } else {
        snprintf(alrtBtn, sizeof(alrtBtn), "ALRT: OFF");
    }
    m_spr.drawString(alrtBtn, 198, 228, 1);
    
    // 4. MENU Button (249..315)
    m_spr.drawRoundRect(249, 218, 67, 20, 3, 0x0460);
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    m_spr.drawString("MENU", 282, 228, 1);
}

void RadarDisplay::drawTargetDetailModal(const Aircraft &ac) {
    m_spr.fillRoundRect(170, 25, 145, 95, 6, 0x0000);
    m_spr.drawRoundRect(170, 25, 145, 95, 6, 0x07E0);
    
    m_spr.setTextColor(0x07E0, TFT_BLACK);
    m_spr.setTextDatum(TL_DATUM);
    m_spr.drawString(ac.callsign, 175, 30, 2);
    
    m_spr.setTextColor(TFT_WHITE, TFT_BLACK);
    float altM = ac.altitudeFt * 0.3048f;
    float speedKmh = ac.speedKts * 1.852f;
    char line1[32], line2[32], line3[32], line4[32];
    snprintf(line1, sizeof(line1), "SRC: %s", (ac.source == SOURCE_ADSB) ? "ADS-B" : "OGN/FLARM");
    snprintf(line2, sizeof(line2), "ALT: %.0fm", altM);
    snprintf(line3, sizeof(line3), "SPD: %.0fkm/h HDG: %.0f\xB0", speedKmh, ac.headingDeg);
    snprintf(line4, sizeof(line4), "DST: %.1fkm BRG: %.0f\xB0", ac.distanceKm, ac.bearingDeg);
    
    m_spr.drawString(line1, 175, 48, 1);
    m_spr.drawString(line2, 175, 60, 1);
    m_spr.drawString(line3, 175, 72, 1);
    m_spr.drawString(line4, 175, 84, 1);
}

void RadarDisplay::handleTouch(std::vector<Aircraft> &targets, RadarWiFiManager &wifiMgr, double &outLat, double &outLon) {
    uint32_t now = millis();
    if (now - m_lastTouchMs < 200) return;
    
    if (m_touch.touched()) {
        TS_Point p = m_touch.getPoint();
        
        // Ignore noise / floating SPI reads
        if (p.x < 100 || p.y < 100 || p.z < 50 || p.z > 3800) {
            return;
        }
        
        m_lastTouchMs = now;
        
        int touchX = map(p.x, CYD_TOUCH_MIN_X, CYD_TOUCH_MAX_X, 0, 320);
        int touchY = map(p.y, CYD_TOUCH_MIN_Y, CYD_TOUCH_MAX_Y, 0, 240);
        
        touchX = constrain(touchX, 0, 319);
        touchY = constrain(touchY, 0, 239);
        
        Serial.printf("[VALID TOUCH!] Raw X:%d, Y:%d, Z:%d -> Mapped X:%d, Y:%d (State: %d)\n", p.x, p.y, p.z, touchX, touchY, (int)m_uiState);
        
        cydBeep(1800, 30);
        
        // ----------------------------------------------------
        // State 0: MAIN MENU SCREEN
        // ----------------------------------------------------
        if (m_uiState == STATE_MAIN_MENU) {
            if (touchY >= 48 && touchY <= 82) { // START RADAR
                m_uiState = STATE_RADAR;
            } else if (touchY >= 88 && touchY <= 122) { // WI-FI SETTINGS
                drawBootScreen("Scanning Wi-Fi...");
                std::vector<String> ssids;
                std::vector<int> rssis;
                std::vector<bool> encs;
                wifiMgr.scanNetworks(ssids, rssis, encs);
                renderWiFiScanScreen(ssids, rssis, encs);
                m_uiState = STATE_WIFI_SCAN;
            } else if (touchY >= 128 && touchY <= 162) { // ALERT CONFIG CYCLING
                if (m_alertDistKm == 5.0f && m_alertAltM == 200.0f) {
                    m_alertDistKm = 50.0f; m_alertAltM = 10000.0f; // TEST MODE: 50km / 10km altitude!
                } else if (m_alertDistKm == 50.0f && m_alertAltM >= 10000.0f) {
                    m_alertDistKm = 10.0f; m_alertAltM = 500.0f;
                } else if (m_alertDistKm == 10.0f && m_alertAltM == 500.0f) {
                    m_alertDistKm = 2.0f; m_alertAltM = 100.0f;
                } else if (m_alertDistKm == 2.0f && m_alertAltM == 100.0f) {
                    m_alertDistKm = 0.0f; m_alertAltM = 0.0f; // OFF
                } else if (m_alertDistKm == 0.0f) {
                    m_alertDistKm = 1.0f; m_alertAltM = 100.0f;
                } else {
                    m_alertDistKm = 5.0f; m_alertAltM = 200.0f;
                }
                wifiMgr.setAlertConfig(m_alertDistKm, m_alertAltM);
            }
            return;
        }
        
        // ----------------------------------------------------
        // State 1: RADAR SCOPE SCREEN
        // ----------------------------------------------------
        else if (m_uiState == STATE_RADAR) {
            // Check Bottom Toolbar
            if (touchY >= 210) {
                if (touchX >= 4 && touchX <= 75) { // RANGE
                    if (m_rangeKm == 5.0f) m_rangeKm = 10.0f;
                    else if (m_rangeKm == 10.0f) m_rangeKm = 25.0f;
                    else if (m_rangeKm == 25.0f) m_rangeKm = 50.0f;
                    else m_rangeKm = 5.0f;
                } else if (touchX >= 78 && touchX <= 148) { // FILTER
                    if (m_filterMode == FILTER_ALL) m_filterMode = FILTER_ADSB_ONLY;
                    else if (m_filterMode == FILTER_ADSB_ONLY) m_filterMode = FILTER_OGN_ONLY;
                    else m_filterMode = FILTER_ALL;
                } else if (touchX >= 152 && touchX <= 245) { // ALERT CONFIG CYCLING
                    if (m_alertDistKm == 5.0f && m_alertAltM == 200.0f) {
                        m_alertDistKm = 50.0f; m_alertAltM = 10000.0f; // TEST MODE: 50km / 10km altitude!
                    } else if (m_alertDistKm == 50.0f && m_alertAltM >= 10000.0f) {
                        m_alertDistKm = 10.0f; m_alertAltM = 500.0f;
                    } else if (m_alertDistKm == 10.0f && m_alertAltM == 500.0f) {
                        m_alertDistKm = 2.0f; m_alertAltM = 100.0f;
                    } else if (m_alertDistKm == 2.0f && m_alertAltM == 100.0f) {
                        m_alertDistKm = 0.0f; m_alertAltM = 0.0f;
                    } else if (m_alertDistKm == 0.0f) {
                        m_alertDistKm = 1.0f; m_alertAltM = 100.0f;
                    } else {
                        m_alertDistKm = 5.0f; m_alertAltM = 200.0f;
                    }
                    wifiMgr.setAlertConfig(m_alertDistKm, m_alertAltM);
                } else if (touchX >= 249 && touchX <= 315) { // RETURN TO MAIN MENU
                    m_uiState = STATE_MAIN_MENU;
                }
                return;
            }
            
            // Check Target Selection
            int centerX = 160;
            int centerY = 116;
            int radius = 94;
            
            for (size_t i = 0; i < targets.size(); i++) {
                const auto &ac = targets[i];
                if (ac.distanceKm > m_rangeKm) continue;
                
                float normalizedDist = ac.distanceKm / m_rangeKm;
                float bearingRad = ac.bearingDeg * DEG_TO_RAD;
                
                int tgtX = centerX + (int)(sinf(bearingRad) * normalizedDist * radius);
                int tgtY = centerY - (int)(cosf(bearingRad) * normalizedDist * radius);
                
                int dx = touchX - tgtX;
                int dy = touchY - tgtY;
                if ((dx * dx + dy * dy) <= 225) {
                    m_selectedTargetIndex = (int)i;
                    m_selectedTargetExpireMs = now + 8000;
                    return;
                }
            }
        }
        
        // ----------------------------------------------------
        // State 2: WI-FI SCANNER NETWORK SELECTION
        // ----------------------------------------------------
        else if (m_uiState == STATE_WIFI_SCAN) {
            if (touchY >= 210) {
                if (touchX >= 10 && touchX <= 150) { // RESCAN
                    drawBootScreen("Scanning Wi-Fi...");
                    std::vector<String> ssids;
                    std::vector<int> rssis;
                    std::vector<bool> encs;
                    wifiMgr.scanNetworks(ssids, rssis, encs);
                    renderWiFiScanScreen(ssids, rssis, encs);
                } else if (touchX >= 170 && touchX <= 310) { // CANCEL -> RETURN TO MENU
                    m_uiState = STATE_MAIN_MENU;
                }
                return;
            }
            
            // Tap network item (6 items vertically)
            int startY = 32;
            int itemH = 26;
            for (int i = 0; i < 6; i++) {
                int y = startY + (i * (itemH + 4));
                if (touchY >= y && touchY <= y + itemH) {
                    // Re-scan briefly to get tapped SSID
                    std::vector<String> ssids;
                    std::vector<int> rssis;
                    std::vector<bool> encs;
                    wifiMgr.scanNetworks(ssids, rssis, encs);
                    if (i < (int)ssids.size()) {
                        m_selectedSsid = ssids[i];
                        m_typedPassword = "";
                        renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                        m_uiState = STATE_WIFI_KEYBOARD;
                    }
                    return;
                }
            }
        }
        
        // ----------------------------------------------------
        // State 3: TOUCH KEYBOARD PASSWORD ENTRY
        // ----------------------------------------------------
        else if (m_uiState == STATE_WIFI_KEYBOARD) {
            const char* row1 = m_symbolsActive ? "1234567890" : (m_shiftActive ? "QWERTYUIOP" : "qwertyuiop");
            const char* row2 = m_symbolsActive ? "!@#$%^&*()" : (m_shiftActive ? "ASDFGHJKL" : "asdfghjkl");
            const char* row3 = m_symbolsActive ? "-_=+[{]};:" : (m_shiftActive ? "ZXCVBNM" : "zxcvbnm");
            
            // Row 1 (y: 60..92)
            if (touchY >= 60 && touchY <= 92) {
                int col = (touchX - 5) / 31;
                if (col >= 0 && col < 10) {
                    m_typedPassword += row1[col];
                    renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                }
            }
            // Row 2 (y: 96..128)
            else if (touchY >= 96 && touchY <= 128) {
                int col = (touchX - 20) / 31;
                if (col >= 0 && col < 9) {
                    m_typedPassword += row2[col];
                    renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                }
            }
            // Row 3 (y: 132..164)
            else if (touchY >= 132 && touchY <= 164) {
                if (touchX >= 5 && touchX <= 41) { // Shift toggle
                    m_shiftActive = !m_shiftActive;
                    renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                } else if (touchX >= 266 && touchX <= 314) { // Backspace
                    if (m_typedPassword.length() > 0) {
                        m_typedPassword.remove(m_typedPassword.length() - 1);
                        renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                    }
                } else {
                    int col = (touchX - 45) / 31;
                    if (col >= 0 && col < 7) {
                        m_typedPassword += row3[col];
                        renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                    }
                }
            }
            // Row 4 (y: 168..200)
            else if (touchY >= 168 && touchY <= 200) {
                if (touchX >= 5 && touchX <= 50) { // Symbols toggle
                    m_symbolsActive = !m_symbolsActive;
                    renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                } else if (touchX >= 55 && touchX <= 190) { // Space
                    m_typedPassword += ' ';
                    renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                } else if (touchX >= 195 && touchX <= 315) { // CONNECT BUTTON
                    drawBootScreen("Connecting to " + m_selectedSsid.substring(0, 12));
                    if (wifiMgr.connectWithCredentials(m_selectedSsid, m_typedPassword)) {
                        cydBeep(2400, 150);
                        double geoLat, geoLon;
                        if (wifiMgr.fetchIpGeolocation(geoLat, geoLon)) {
                            outLat = geoLat;
                            outLon = geoLon;
                        }
                        m_uiState = STATE_RADAR;
                    } else {
                        cydBeep(500, 300);
                        drawBootScreen("Failed to Connect!");
                        delay(2000);
                        renderKeyboardScreen(m_selectedSsid, m_typedPassword);
                    }
                }
            }
        }
    }
}
