#include "wifi_manager.h"

const uint16_t GPS_UDP_PORT = 10110;

RadarWiFiManager::RadarWiFiManager() 
    : m_server(80), m_lat(51.1657), m_lon(10.4515), m_headingDeg(0.0f), m_hasCompass(false),
      m_hasPixelGps(false), m_lastGpsUpdateMs(0),
      m_alertDistKm(5.0f), m_alertAltM(200.0f) {}

bool RadarWiFiManager::init(double &currentLat, double &currentLon) {
    m_prefs.begin("radar_cfg", false);
    m_lat = m_prefs.getDouble("lat", 51.1657);
    m_lon = m_prefs.getDouble("lon", 10.4515);
    m_alertDistKm = m_prefs.getFloat("alrt_dist", 5.0f);
    m_alertAltM = m_prefs.getFloat("alrt_alt", 200.0f);
    m_prefs.end();
    
    bool connected = connectStoredWiFi();
    
    if (connected) {
        if (m_lat == 51.1657) {
            double geoLat, geoLon;
            if (fetchIpGeolocation(geoLat, geoLon)) {
                m_lat = geoLat;
                m_lon = geoLon;
                m_prefs.begin("radar_cfg", false);
                m_prefs.putDouble("lat", m_lat);
                m_prefs.putDouble("lon", m_lon);
                m_prefs.end();
            }
        }
        setupGpsServices();
    }
    
    currentLat = m_lat;
    currentLon = m_lon;
    return connected;
}

void RadarWiFiManager::setAlertConfig(float dist, float alt) {
    m_alertDistKm = dist;
    m_alertAltM = alt;
    m_prefs.begin("radar_cfg", false);
    m_prefs.putFloat("alrt_dist", m_alertDistKm);
    m_prefs.putFloat("alrt_alt", m_alertAltM);
    m_prefs.end();
}

void RadarWiFiManager::handleClient() {
    if (WiFi.status() == WL_CONNECTED) {
        m_server.handleClient();
        processUdpGps();
    }
}

void RadarWiFiManager::setupGpsServices() {
    m_udp.begin(GPS_UDP_PORT);
    m_server.enableCORS(true);
    
    m_server.on("/setalert", [this]() {
        m_server.sendHeader("Access-Control-Allow-Origin", "*");
        if (m_server.hasArg("dist") && m_server.hasArg("alt")) {
            m_alertDistKm = m_server.arg("dist").toFloat();
            m_alertAltM = m_server.arg("alt").toFloat();
            
            m_prefs.begin("radar_cfg", false);
            m_prefs.putFloat("alrt_dist", m_alertDistKm);
            m_prefs.putFloat("alrt_alt", m_alertAltM);
            m_prefs.end();
            
            String res = "Alert Config Updated: Dist " + String(m_alertDistKm, 1) + "km | Alt " + String(m_alertAltM, 0) + "m";
            m_server.send(200, "text/plain", res);
            return;
        }
        m_server.send(400, "text/plain", "Usage: /setalert?dist=5.0&alt=200");
    });

    m_server.on("/setgps", [this]() {
        m_server.sendHeader("Access-Control-Allow-Origin", "*");
        if (m_server.hasArg("lat") && m_server.hasArg("lon")) {
            m_lat = m_server.arg("lat").toDouble();
            m_lon = m_server.arg("lon").toDouble();
            m_hasPixelGps = true;
            m_lastGpsUpdateMs = millis();
            
            if (m_server.hasArg("hdg")) {
                m_headingDeg = m_server.arg("hdg").toFloat();
                m_hasCompass = true;
            }
            
            // NOTE: Flash writing removed here to prevent CPU stalls, display stuttering, and NVS crash!
            m_server.send(200, "text/plain", "OK");
            return;
        }
        m_server.send(400, "text/plain", "Usage: /setgps?lat=50.7689&lon=7.1647&hdg=180");
    });
    
    m_server.on("/api/location", HTTP_POST, [this]() {
        m_server.sendHeader("Access-Control-Allow-Origin", "*");
        if (m_server.hasArg("plain")) {
            String body = m_server.arg("plain");
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);
            if (!err) {
                m_lat = doc["lat"];
                m_lon = doc["lon"];
                m_hasPixelGps = true;
                m_lastGpsUpdateMs = millis();
                
                if (doc["hdg"].is<float>()) {
                    m_headingDeg = doc["hdg"].as<float>();
                    m_hasCompass = true;
                }
                
                // NOTE: Flash writing removed here to prevent CPU stalls, display stuttering, and NVS crash!
                m_server.send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
        }
        m_server.send(400, "application/json", "{\"error\":\"bad_request\"}");
    });
    
    m_server.on("/gps", [this]() {
        m_server.sendHeader("Access-Control-Allow-Origin", "*");
        String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Pixel 10 GPS & Compass Streamer</title>
    <style>
        body { font-family: sans-serif; background: #0a0e14; color: #00ffcc; text-align: center; padding: 20px; }
        .card { background: #121820; border: 2px solid #00ffcc; border-radius: 12px; padding: 20px; max-width: 400px; margin: auto; }
        .status { font-size: 1.2em; font-weight: bold; margin: 15px 0; color: #ffcc00; }
        .coords { font-size: 1.1em; color: #ffffff; }
        .compass { font-size: 1.1em; color: #00ff66; margin-top: 8px; font-weight: bold; }
        .count { font-size: 0.9em; color: #8899a6; margin-top: 10px; }
        .hint { font-size: 0.8em; color: #8899a6; margin-top: 15px; }
    </style>
</head>
<body>
    <div class="card">
        <h2>Pixel 10 GPS & Compass Streamer</h2>
        <p>Streaming high-precision GPS & Smartphone Compass to ESP32...</p>
        <div id="status" class="status">Initializing sensors...</div>
        <div id="coords" class="coords">Lat: --, Lon: --</div>
        <div id="compass" class="compass">Kompass: --° 🧭</div>
        <div id="count" class="count">Sent updates: 0</div>
    </div>
    <script>
        let sentCount = 0;
        let currentHdg = -1;
        let currentLat = null;
        let currentLon = null;
        let currentAlt = 0;
        let isSending = false;

        function onOrientation(e) {
            let hdg = null;
            if (e.webkitCompassHeading !== undefined && e.webkitCompassHeading !== null) {
                hdg = e.webkitCompassHeading;
            } else if (e.alpha !== null) {
                hdg = (360 - e.alpha) % 360;
            }
            if (hdg !== null && !isNaN(hdg)) {
                currentHdg = Math.round(hdg);
                document.getElementById("compass").innerText = "Kompass: " + currentHdg + "° 🧭";
            }
        }

        if (window.DeviceOrientationEvent) {
            window.addEventListener('deviceorientationabsolute', onOrientation, true);
            window.addEventListener('deviceorientation', onOrientation, true);
        }

        if ("geolocation" in navigator) {
            navigator.geolocation.watchPosition(function(pos) {
                currentLat = pos.coords.latitude;
                currentLon = pos.coords.longitude;
                currentAlt = pos.coords.altitude || 0;
                
                document.getElementById("status").innerText = "LIVE GPS & COMPASS ACTIVE 📡";
                document.getElementById("status").style.color = "#00ff66";
                document.getElementById("coords").innerText = "Lat: " + currentLat.toFixed(6) + " | Lon: " + currentLon.toFixed(6);
            }, function(err) {
                document.getElementById("status").innerText = "GPS Error: " + err.message;
                document.getElementById("status").style.color = "#ff3366";
            }, { enableHighAccuracy: true, maximumAge: 1000, timeout: 5000 });
        } else {
            document.getElementById("status").innerText = "Geolocation not supported!";
        }

        // Send at most 1 update per second (1 Hz) to avoid flooding ESP32 WebServer
        setInterval(function() {
            if (currentLat === null || currentLon === null || isSending) return;
            
            isSending = true;
            let url = '/setgps?lat=' + currentLat + '&lon=' + currentLon + '&alt=' + currentAlt;
            if (currentHdg >= 0) {
                url += '&hdg=' + currentHdg;
            }
            
            fetch(url).then(function() {
                sentCount++;
                document.getElementById("count").innerText = "Sent updates: " + sentCount;
                isSending = false;
            }).catch(function() {
                isSending = false;
            });
        }, 1000);
    </script>
</body>
</html>
)rawhtml";
        m_server.send(200, "text/html", html);
    });
    
    m_server.begin();
}

void RadarWiFiManager::processUdpGps() {
    int packetSize = m_udp.parsePacket();
    if (packetSize > 0) {
        char packetBuffer[256];
        int len = m_udp.read(packetBuffer, sizeof(packetBuffer) - 1);
        if (len > 0) {
            packetBuffer[len] = 0;
            String msg = String(packetBuffer);
            msg.trim();
            
            if (msg.startsWith("{")) {
                JsonDocument doc;
                if (!deserializeJson(doc, msg)) {
                    m_lat = doc["lat"];
                    m_lon = doc["lon"];
                    m_hasPixelGps = true;
                    m_lastGpsUpdateMs = millis();
                }
            } else if (msg.startsWith("$GPRMC") || msg.startsWith("$GPGGA")) {
                parseNmeaSentence(msg);
            }
        }
    }
}

void RadarWiFiManager::parseNmeaSentence(const String &nmea) {
    if (nmea.startsWith("$GPRMC")) {
        int idx1 = nmea.indexOf(','); if (idx1 < 0) return;
        int idx2 = nmea.indexOf(',', idx1 + 1); if (idx2 < 0) return;
        int idx3 = nmea.indexOf(',', idx2 + 1); if (idx3 < 0) return;
        int idx4 = nmea.indexOf(',', idx3 + 1); if (idx4 < 0) return;
        int idx5 = nmea.indexOf(',', idx4 + 1); if (idx5 < 0) return;
        int idx6 = nmea.indexOf(',', idx5 + 1); if (idx6 < 0) return;
        
        String status = nmea.substring(idx1 + 1, idx2);
        if (status != "A") return;
        
        String latRaw = nmea.substring(idx2 + 1, idx3);
        String latNs  = nmea.substring(idx3 + 1, idx4);
        String lonRaw = nmea.substring(idx4 + 1, idx5);
        String lonEw  = nmea.substring(idx5 + 1, idx6);
        
        if (latRaw.length() >= 4 && lonRaw.length() >= 5) {
            float latDeg = latRaw.substring(0, 2).toFloat();
            float latMin = latRaw.substring(2).toFloat();
            double lat = latDeg + (latMin / 60.0f);
            if (latNs == "S") lat = -lat;
            
            float lonDeg = lonRaw.substring(0, 3).toFloat();
            float lonMin = lonRaw.substring(3).toFloat();
            double lon = lonDeg + (lonMin / 60.0f);
            if (lonEw == "W") lon = -lon;
            
            m_lat = lat;
            m_lon = lon;
            m_hasPixelGps = true;
            m_lastGpsUpdateMs = millis();
        }
    }
}

bool RadarWiFiManager::connectStoredWiFi() {
    String savedSsid = getSavedSSID();
    String savedPass = getSavedPassword();
    
    if (savedSsid.length() == 0) return false;
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
        delay(200);
    }
    
    return (WiFi.status() == WL_CONNECTED);
}

bool RadarWiFiManager::connectWithCredentials(const String &ssid, const String &pass) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < 12000) {
        delay(200);
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        saveCredentials(ssid, pass);
        setupGpsServices();
        return true;
    }
    return false;
}

int RadarWiFiManager::scanNetworks(std::vector<String> &ssidList, std::vector<int> &rssiList, std::vector<bool> &encryptedList) {
    ssidList.clear();
    rssiList.clear();
    encryptedList.clear();
    
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(150);
    
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n && i < 15; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.length() > 0) {
            ssidList.push_back(ssid);
            rssiList.push_back(WiFi.RSSI(i));
            encryptedList.push_back(WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        }
    }
    WiFi.scanDelete();
    return (int)ssidList.size();
}

void RadarWiFiManager::saveCredentials(const String &ssid, const String &pass) {
    m_prefs.begin("radar_cfg", false);
    m_prefs.putString("ssid", ssid);
    m_prefs.putString("pass", pass);
    m_prefs.end();
}

String RadarWiFiManager::getSavedSSID() {
    m_prefs.begin("radar_cfg", true);
    String val = m_prefs.getString("ssid", "");
    m_prefs.end();
    return val;
}

String RadarWiFiManager::getSavedPassword() {
    m_prefs.begin("radar_cfg", true);
    String val = m_prefs.getString("pass", "");
    m_prefs.end();
    return val;
}

bool RadarWiFiManager::fetchIpGeolocation(double &lat, double &lon) {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    HTTPClient http;
    http.begin("http://ip-api.com/json/?fields=status,lat,lon");
    http.setTimeout(4000);
    
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (!err && doc["status"] == "success") {
            lat = doc["lat"];
            lon = doc["lon"];
            http.end();
            return true;
        }
    }
    http.end();
    return false;
}

bool RadarWiFiManager::isConnected() {
    return WiFi.status() == WL_CONNECTED;
}

int RadarWiFiManager::getRSSI() {
    return WiFi.RSSI();
}

String RadarWiFiManager::getIPAddress() {
    return WiFi.localIP().toString();
}

String RadarWiFiManager::getSSID() {
    return WiFi.SSID();
}
