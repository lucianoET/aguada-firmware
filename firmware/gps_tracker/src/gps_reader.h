#pragma once
#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "gps_types.h"

#ifndef GPS_UART_NUM
#define GPS_UART_NUM 2
#endif

// GpsReader is the sole owner of the HardwareSerial UART instance and of the
// TinyGPSPlus parser instance. Nothing else in this firmware — not main.cpp,
// not fix_gate — is allowed to touch either directly; every other module
// reaches GPS state exclusively through the Fix snapshot returned by poll().
class GpsReader {
public:
    // Starts the UART with GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN on
    // GPS_UART_NUM and starts nothing else — transmits nothing to the GPS
    // module (D-02: no UBX/CASIC/PMTK, no baud probing).
    void begin();

    // Drains every available byte into the parser, echoes raw characters to
    // Serial when raw echo is on, and returns true only when a new location
    // fix has committed since the previous call — at which point `out` is
    // filled from a single consistent read of the parser.
    bool poll(Fix &out);

    uint32_t bytesRead() const { return bytesRead_; }
    uint32_t passedChecksum() const;
    uint32_t failedChecksum() const;

    // True while NMEA bytes have arrived recently, regardless of fix state.
    bool receiving() const;

    void setRawEcho(bool on) { rawEcho_ = on; }
    bool rawEcho() const { return rawEcho_; }

private:
    TinyGPSPlus    gps_;
    HardwareSerial serial_{GPS_UART_NUM};

    bool     rawEcho_ = true;
    uint32_t bytesRead_ = 0;
    uint32_t lastByteAtMs_ = 0;
};
