#ifndef AIRCRAFT_H
#define AIRCRAFT_H

#include <Arduino.h>
#include <math.h>

enum SourceType {
    SOURCE_ADSB = 0,
    SOURCE_OGN_FLARM = 1
};

struct Aircraft {
    String id;               // ICAO Hex or OGN FLARM ID
    String callsign;         // Flight ID / Registration / Tail number
    double lat;
    double lon;
    float altitudeFt;        // Altitude in Feet MSL
    float speedKts;          // Groundspeed in Knots
    float headingDeg;        // Track in degrees (0..360)
    float verticalSpeedFpm;  // Climb/Sink rate in feet per min (+/-)
    SourceType source;
    uint32_t lastSeenMs;     // Timestamp millis() when updated
    
    // Computed radar metrics relative to observer
    float distanceKm;        // Distance in km
    float bearingDeg;        // True bearing from observer (0..360)
    bool isAlert;            // Highlight if dangerously close
};

// ==========================================
// Navigational & Geodesic Calculations
// ==========================================

// Calculate Great-Circle distance in km using Haversine formula
inline float calculateDistanceKm(double lat1, double lon1, double lat2, double lon2) {
    const float R = 6371.0f; // Earth radius in km
    float dLat = (float)(lat2 - lat1) * DEG_TO_RAD;
    float dLon = (float)(lon2 - lon1) * DEG_TO_RAD;
    
    float a = sinf(dLat / 2.0f) * sinf(dLat / 2.0f) +
              cosf((float)lat1 * DEG_TO_RAD) * cosf((float)lat2 * DEG_TO_RAD) *
              sinf(dLon / 2.0f) * sinf(dLon / 2.0f);
              
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return R * c;
}

// Calculate initial bearing in degrees (0..360) from observer to target
inline float calculateBearingDeg(double lat1, double lon1, double lat2, double lon2) {
    float phi1 = (float)lat1 * DEG_TO_RAD;
    float phi2 = (float)lat2 * DEG_TO_RAD;
    float dLon = (float)(lon2 - lon1) * DEG_TO_RAD;
    
    float y = sinf(dLon) * cosf(phi2);
    float x = cosf(phi1) * sinf(phi2) - sinf(phi1) * cosf(phi2) * cosf(dLon);
    
    float bearing = atan2f(y, x) * RAD_TO_DEG;
    if (bearing < 0.0f) {
        bearing += 360.0f;
    }
    return bearing;
}

#endif // AIRCRAFT_H
