#include "adsb_client.h"

AdsbClient::AdsbClient() : m_lastUpdateMs(0), m_fetchedCount(0) {}

bool AdsbClient::updateTraffic(double centerLat, double centerLon, float radiusKm, std::vector<Aircraft> &targetList) {
    if (WiFi.status() != WL_CONNECTED) return false;
    
    // Convert km to nautical miles for API query (min 15 nmi to ensure airliner coverage)
    int radiusNmi = (int)(radiusKm * 0.539957f);
    if (radiusNmi < 15) radiusNmi = 15;
    if (radiusNmi > 150) radiusNmi = 150;
    
    WiFiClientSecure client;
    client.setInsecure(); // Skip SSL certificate verification for high-speed ESP32 fetch
    
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.4f/%.4f/%d", centerLat, centerLon, radiusNmi);
    
    http.begin(client, url);
    http.setTimeout(1500);
    http.setUserAgent("ESP32-CYD-Radar/1.0");
    
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        http.end();
        // Fallback to airplanes.live API
        snprintf(url, sizeof(url), "https://api.airplanes.live/v2/point/%.4f/%.4f/%d", centerLat, centerLon, radiusNmi);
        http.begin(client, url);
        http.setTimeout(1500);
        http.setUserAgent("ESP32-CYD-Radar/1.0");
        httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            http.end();
            return false;
        }
    }
    
    // Filter to only extract necessary fields and discard 85% of JSON payload from memory
    JsonDocument filter;
    filter["ac"][0]["hex"] = true;
    filter["ac"][0]["flight"] = true;
    filter["ac"][0]["lat"] = true;
    filter["ac"][0]["lon"] = true;
    filter["ac"][0]["alt_baro"] = true;
    filter["ac"][0]["alt_geom"] = true;
    filter["ac"][0]["gs"] = true;
    filter["ac"][0]["track"] = true;
    filter["ac"][0]["baro_rate"] = true;
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    
    if (err) {
        return false;
    }
    
    JsonArray acArray = doc["ac"].as<JsonArray>();
    uint32_t now = millis();
    m_fetchedCount = 0;
    
    for (JsonObject ac : acArray) {
        String hex = ac["hex"] | "";
        if (hex.length() == 0) continue;
        
        double lat = ac["lat"] | 0.0;
        double lon = ac["lon"] | 0.0;
        if (lat == 0.0 || lon == 0.0) continue;
        
        float distKm = calculateDistanceKm(centerLat, centerLon, lat, lon);
        if (distKm > radiusKm) continue; // Out of current range ring
        
        float bearingDeg = calculateBearingDeg(centerLat, centerLon, lat, lon);
        
        String callsign = ac["flight"] | hex;
        callsign.trim();
        
        float altFt = 0.0f;
        if (ac["alt_baro"].is<float>()) {
            altFt = ac["alt_baro"].as<float>();
        } else if (ac["alt_geom"].is<float>()) {
            altFt = ac["alt_geom"].as<float>();
        }
        
        float gs = ac["gs"] | 0.0f;
        float track = ac["track"] | 0.0f;
        float baroRate = ac["baro_rate"] | 0.0f;
        
        // Find existing target in list or create new entry
        bool found = false;
        for (auto &tgt : targetList) {
            if (tgt.id == hex && tgt.source == SOURCE_ADSB) {
                tgt.callsign = callsign;
                tgt.lat = lat;
                tgt.lon = lon;
                tgt.altitudeFt = altFt;
                tgt.speedKts = gs;
                tgt.headingDeg = track;
                tgt.verticalSpeedFpm = baroRate;
                tgt.lastSeenMs = now;
                tgt.distanceKm = distKm;
                tgt.bearingDeg = bearingDeg;
                found = true;
                break;
            }
        }
        
        if (!found && targetList.size() < 60) {
            Aircraft newAc;
            newAc.id = hex;
            newAc.callsign = callsign;
            newAc.lat = lat;
            newAc.lon = lon;
            newAc.altitudeFt = altFt;
            newAc.speedKts = gs;
            newAc.headingDeg = track;
            newAc.verticalSpeedFpm = baroRate;
            newAc.source = SOURCE_ADSB;
            newAc.lastSeenMs = now;
            newAc.distanceKm = distKm;
            newAc.bearingDeg = bearingDeg;
            newAc.isAlert = false;
            targetList.push_back(newAc);
        }
        
        m_fetchedCount++;
    }
    
    m_lastUpdateMs = now;
    return true;
}
