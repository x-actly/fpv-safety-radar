#include "ogn_client.h"
#include <WiFi.h>

const char* OGN_SERVER = "aprs.glidernet.org";
const uint16_t OGN_PORT = 14580;

OgnClient::OgnClient() 
    : m_lastReconnectMs(0), m_lastKeepAliveMs(0), m_fetchedCount(0),
      m_currentLat(0.0), m_currentLon(0.0), m_currentRadius(0.0) {}

OgnClient::~OgnClient() {
    if (m_tcpClient.connected()) {
        m_tcpClient.stop();
    }
}

bool OgnClient::isConnected() {
    return m_tcpClient.connected();
}

bool OgnClient::connectServer(double centerLat, double centerLon, float radiusKm) {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    if (m_tcpClient.connected()) {
        m_tcpClient.stop();
    }
    
    if (!m_tcpClient.connect(OGN_SERVER, OGN_PORT)) {
        return false;
    }
    
    // Format APRS IS login with geographic radius filter
    char login[128];
    snprintf(login, sizeof(login), 
             "user N0CALL pass -1 vers ESP32CYDRadar 1.0 filter r/%.4f/%.4f/%d\r\n", 
             centerLat, centerLon, (int)radiusKm);
             
    m_tcpClient.print(login);
    
    m_currentLat = centerLat;
    m_currentLon = centerLon;
    m_currentRadius = radiusKm;
    m_lastKeepAliveMs = millis();
    m_rxBuffer = "";
    
    return true;
}

void OgnClient::process(double centerLat, double centerLon, float radiusKm, std::vector<Aircraft> &targetList) {
    uint32_t now = millis();
    
    // Check if re-connection needed (or location/radius changed significantly)
    bool radiusChanged = fabs(radiusKm - m_currentRadius) > 2.0f;
    bool locChanged = (calculateDistanceKm(centerLat, centerLon, m_currentLat, m_currentLon) > 5.0f);
    
    if (!m_tcpClient.connected() || radiusChanged || locChanged) {
        if (now - m_lastReconnectMs > 5000) { // Throttle reconnects to 5s
            m_lastReconnectMs = now;
            connectServer(centerLat, centerLon, radiusKm);
        }
        return;
    }
    
    // Send keep-alive every 60 seconds
    if (now - m_lastKeepAliveMs > 60000) {
        m_tcpClient.print("# keepalive\r\n");
        m_lastKeepAliveMs = now;
    }
    
    // Read incoming stream non-blocking
    while (m_tcpClient.available() > 0) {
        char c = m_tcpClient.read();
        if (c == '\n') {
            m_rxBuffer.trim();
            if (m_rxBuffer.length() > 0 && !m_rxBuffer.startsWith("#")) {
                parseAprsPacket(m_rxBuffer, centerLat, centerLon, radiusKm, targetList);
            }
            m_rxBuffer = "";
        } else if (c != '\r') {
            m_rxBuffer += c;
            if (m_rxBuffer.length() > 256) { // Safety length truncation
                m_rxBuffer = "";
            }
        }
    }
}

void OgnClient::parseAprsPacket(const String &packet, double centerLat, double centerLon, float radiusKm, std::vector<Aircraft> &targetList) {
    // Format: CALLSIGN>APRS,TCPIP*,qAS,...:>HHMMSShDDMM.MMN/DDDMM.MME'DDD/SSS/A=AAAAAA idXXYYYYYY ...
    int gtIdx = packet.indexOf('>');
    if (gtIdx <= 0) return;
    
    String callsign = packet.substring(0, gtIdx);
    callsign.trim();
    
    int posMarker = packet.indexOf('h', gtIdx);
    if (posMarker < 0 || posMarker + 18 >= (int)packet.length()) return;
    
    // Extract Latitude: e.g. 5112.34N
    String latStr = packet.substring(posMarker + 1, posMarker + 9); // 5112.34N
    if (latStr.length() < 8) return;
    float latDeg = latStr.substring(0, 2).toFloat();
    float latMin = latStr.substring(2, 7).toFloat();
    double lat = latDeg + (latMin / 60.0f);
    if (latStr.endsWith("S")) lat = -lat;
    
    // Extract Longitude: e.g. 00627.12E
    String lonStr = packet.substring(posMarker + 10, posMarker + 19); // 00627.12E
    if (lonStr.length() < 9) return;
    float lonDeg = lonStr.substring(0, 3).toFloat();
    float lonMin = lonStr.substring(3, 8).toFloat();
    double lon = lonDeg + (lonMin / 60.0f);
    if (lonStr.endsWith("W")) lon = -lon;
    
    float distKm = calculateDistanceKm(centerLat, centerLon, lat, lon);
    if (distKm > radiusKm) return;
    
    float bearingDeg = calculateBearingDeg(centerLat, centerLon, lat, lon);
    
    // Altitude: A=001250
    float altFt = 0.0f;
    int altIdx = packet.indexOf("/A=");
    if (altIdx > 0 && altIdx + 9 <= (int)packet.length()) {
        altFt = packet.substring(altIdx + 3, altIdx + 9).toFloat();
    }
    
    // Course and Speed: '045/032
    float track = 0.0f;
    float speedKts = 0.0f;
    int speedIdx = packet.indexOf('/', posMarker + 19);
    if (speedIdx > 0 && speedIdx - 3 >= 0 && speedIdx + 4 <= (int)packet.length()) {
        track = packet.substring(speedIdx - 3, speedIdx).toFloat();
        speedKts = packet.substring(speedIdx + 1, speedIdx + 4).toFloat();
    }
    
    // Climb rate: e.g. +020fpm
    float fpm = 0.0f;
    int fpmIdx = packet.indexOf("fpm");
    if (fpmIdx >= 4) {
        fpm = packet.substring(fpmIdx - 4, fpmIdx).toFloat();
    }
    
    uint32_t now = millis();
    
    // Check if target already exists in list
    bool found = false;
    for (auto &tgt : targetList) {
        if (tgt.id == callsign && tgt.source == SOURCE_OGN_FLARM) {
            tgt.lat = lat;
            tgt.lon = lon;
            tgt.altitudeFt = altFt;
            tgt.speedKts = speedKts;
            tgt.headingDeg = track;
            tgt.verticalSpeedFpm = fpm;
            tgt.lastSeenMs = now;
            tgt.distanceKm = distKm;
            tgt.bearingDeg = bearingDeg;
            found = true;
            break;
        }
    }
    
    if (!found && targetList.size() < 60) {
        Aircraft newAc;
        newAc.id = callsign;
        newAc.callsign = callsign;
        newAc.lat = lat;
        newAc.lon = lon;
        newAc.altitudeFt = altFt;
        newAc.speedKts = speedKts;
        newAc.headingDeg = track;
        newAc.verticalSpeedFpm = fpm;
        newAc.source = SOURCE_OGN_FLARM;
        newAc.lastSeenMs = now;
        newAc.distanceKm = distKm;
        newAc.bearingDeg = bearingDeg;
        newAc.isAlert = false;
        targetList.push_back(newAc);
        m_fetchedCount++;
    }
}
