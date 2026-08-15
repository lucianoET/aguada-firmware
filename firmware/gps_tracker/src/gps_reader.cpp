// GpsReader uses the GPS module read-only by design (D-02): begin()/poll()
// never write or print to the GPS UART object — no UBX, no CASIC, no PMTK,
// no baud probing. Both target modules (ATGM336H, NEO-6M) must work from
// this same binary at plain NMEA 9600 baud (D-03).

#include "gps_reader.h"
#include "gps_config.h"
#include <cstdlib>

// Howard Hinnant's days-from-civil algorithm (public domain), used instead
// of mktime()/an external time library because we only ever need pure UTC
// seconds-since-epoch with no timezone handling.
// http://howardhinnant.github.io/date_algorithms.html
static int32_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    uint32_t yoe = static_cast<uint32_t>(y - era * 400);                    // [0, 399]
    uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;           // [0, 365]
    uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                    // [0, 146096]
    return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

// Registers the GSV field-3 ("total satellites in view") custom elements
// against gps_ -- the only place in the firmware allowed to instantiate
// TinyGPSCustom (see gps_reader.h's class-level ownership comment). GPGSV
// covers u-blox NEO-6M; GNGSV is the aggregated multi-GNSS form; GLGSV
// (GLONASS) and BDGSV (BeiDou) are the per-constellation forms. BDGSV is
// not hypothetical: the GY-GPS6MV2 boards on this bench carry a remarked
// AT6558-class chip, not a real NEO-6M, and emit GPGSV + BDGSV with no
// aggregated GNGSV at all -- without it every BeiDou satellite is invisible.
GpsReader::GpsReader()
    : gpgsvSatsInView_(gps_, "GPGSV", 3),
      glgsvSatsInView_(gps_, "GLGSV", 3),
      bdgsvSatsInView_(gps_, "BDGSV", 3),
      gngsvSatsInView_(gps_, "GNGSV", 3) {
}

void GpsReader::begin() {
    serial_.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

uint32_t GpsReader::passedChecksum() const {
    return gps_.passedChecksum();
}

uint32_t GpsReader::failedChecksum() const {
    return gps_.failedChecksum();
}

bool GpsReader::receiving() const {
    return bytesRead_ > 0 && (millis() - lastByteAtMs_) < 2000;
}

uint8_t GpsReader::satsInView() const {
    // GNGSV is already the combined multi-GNSS total, so when it is fresh it
    // is the answer -- adding the per-constellation talkers on top of it
    // would count the same satellites twice.
    if (gngsvSatsInView_.age() < DEFAULT_GPS_FRESH_MS) {
        return static_cast<uint8_t>(atoi(gngsvSatsInView_.value()));
    }

    // No aggregate: each talker reports only its own constellation, so the
    // total is their sum. Taking the max here instead would silently drop
    // every constellation but the largest.
    uint16_t total = 0;
    if (gpgsvSatsInView_.age() < DEFAULT_GPS_FRESH_MS) {
        total += static_cast<uint16_t>(atoi(gpgsvSatsInView_.value()));
    }
    if (glgsvSatsInView_.age() < DEFAULT_GPS_FRESH_MS) {
        total += static_cast<uint16_t>(atoi(glgsvSatsInView_.value()));
    }
    if (bdgsvSatsInView_.age() < DEFAULT_GPS_FRESH_MS) {
        total += static_cast<uint16_t>(atoi(bdgsvSatsInView_.value()));
    }

    // Accumulate in uint16_t and clamp: three constellations in view can
    // plausibly exceed 255 only under a garbled field, but the return type
    // must not wrap on one.
    return total > 255 ? 255 : static_cast<uint8_t>(total);
}

bool GpsReader::poll(Fix &out) {
    while (serial_.available()) {
        char c = serial_.read();
        bytesRead_++;
        lastByteAtMs_ = millis();
        if (rawEcho_) Serial.write(c);
        gps_.encode(c);
    }

    // Mandatory ordering: check the location isUpdated() flag exactly once,
    // BEFORE calling any accessor (e.g. FixQuality(), lat()/lng()) that
    // consumes it as a side effect. Checking isUpdated() first is what
    // prevents a new-fix edge from being swallowed before this poll()
    // observes it.
    if (!gps_.location.isUpdated()) {
        return false;
    }

    out.lat        = gps_.location.isValid() ? gps_.location.lat() : 0.0;
    out.lon        = gps_.location.isValid() ? gps_.location.lng() : 0.0;
    out.speed_kmh  = gps_.speed.isValid() ? static_cast<float>(gps_.speed.kmph()) : 0.0f;
    out.course_deg = gps_.course.isValid() ? static_cast<float>(gps_.course.deg()) : 0.0f;
    out.alt_m      = gps_.altitude.isValid() ? static_cast<float>(gps_.altitude.meters()) : 0.0f;
    // Sentinel above any plausible HDOP so an absent value is rejected by
    // fix_gate rather than silently accepted.
    out.hdop        = gps_.hdop.isValid() ? static_cast<float>(gps_.hdop.hdop()) : 99.9f;
    out.sats        = gps_.satellites.isValid() ? static_cast<uint8_t>(gps_.satellites.value()) : 0;
    // TinyGPSLocation::Quality's enumerators are ASCII digit characters
    // ('0'..'8'), not small integers — normalize before storing so
    // fix_quality carries the documented 0-8 range (gps_types.h).
    out.fix_quality = gps_.location.isValid()
        ? static_cast<uint8_t>(gps_.location.FixQuality() - '0')
        : 0;
    out.age_ms      = gps_.location.age();
    out.mono_ms     = millis();

    // GPS week rollover: receivers with pre-2016 firmware wrap the 10-bit
    // week number every 1024 weeks, so the time-of-day arrives correct but
    // the date lands 7168 days in the past (bench, ATGM336H: 2026-08-14
    // reported as 2006-12-29). Adding one epoch back and re-testing the
    // DEFAULT_GPS_MIN_UTC_YEAR floor recovers the real date without ever
    // promoting a pre-lock date: TinyGPSPlus's year-2000 default lands at
    // 2019 after the shift and is still rejected. One epoch carries this to
    // ~2045; the floor is what makes a second one a code change, not a
    // silent wrong timestamp.
    out.utc_unix = 0;
    if (gps_.date.isValid() && gps_.time.isValid()) {
        const int32_t minDays = daysFromCivil(DEFAULT_GPS_MIN_UTC_YEAR, 1, 1);
        int32_t days = daysFromCivil(gps_.date.year(), gps_.date.month(), gps_.date.day());
        if (days < minDays) {
            days += GPS_ROLLOVER_DAYS;
        }
        if (days >= minDays) {
            out.utc_unix = static_cast<uint32_t>(days) * 86400UL
                         + static_cast<uint32_t>(gps_.time.hour()) * 3600UL
                         + static_cast<uint32_t>(gps_.time.minute()) * 60UL
                         + static_cast<uint32_t>(gps_.time.second());
        }
    }

    return true;
}
