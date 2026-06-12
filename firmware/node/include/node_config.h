#pragma once
#include <stdbool.h>
#include "protocol.h"   // shared: FW_VERSION, defaults, packet types

#define NVS_NAMESPACE           "aguada"
#define OTA_BLOCK_SIZE          200

typedef struct {
    uint8_t trig_pin;
    uint8_t echo_pin;
    bool    enabled;
} sensor_hw_t;

typedef enum : uint8_t {
    SENSOR_TYPE_ULTRASONIC  = 0,    // HC-SR04 (default)
    SENSOR_TYPE_AIR_QUALITY = 1,    // ENS160 + AHT21
} sensor_type_t;

typedef struct {
    uint16_t    node_id;                // 2 last MAC bytes
    uint8_t     num_sensors;            // 0=relay-only, 1 or 2=ultrasonic sensors
    sensor_hw_t sensor[2];             // [0]=sensor_id1, [1]=sensor_id2

    // Timing
    uint16_t    interval_measure_s;
    uint16_t    interval_send_s;
    uint16_t    heartbeat_s;

    // Filters
    uint8_t     filter_window;
    uint8_t     filter_outlier_cm;
    uint8_t     filter_threshold_cm;

    // Optional features
    bool        vbat_enabled;
    uint8_t     vbat_pin;
    uint8_t     vbat_div;           // voltage divider ratio: 1=direct, 2=equal resistors
    bool        deep_sleep_enabled;
    bool        led_enabled;

    // Network
    uint8_t     espnow_channel;
    uint8_t     ttl_max;

    // Maintenance
    uint8_t     restart_daily_h;        // 0=disabled
    char        fw_version[8];

    // ANI-01 / sensor type extension
    sensor_type_t sensor_type;          // selects sensor driver (default: ULTRASONIC)
    bool          relay_enabled;        // enable mesh relay independent of num_sensors
    uint8_t       led_rgb_pin;          // WS2812B data pin (ANI-01; 0=disabled)
    uint8_t       i2c_sda_pin;          // I2C SDA for ENS160+AHT21
    uint8_t       i2c_scl_pin;          // I2C SCL for ENS160+AHT21
} node_config_t;

// GPIO defaults — overridable via build flags
#if defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-C3 SuperMini
#  ifndef DEFAULT_I2C_SDA_PIN
#    define DEFAULT_I2C_SDA_PIN 6
#  endif
#  ifndef DEFAULT_I2C_SCL_PIN
#    define DEFAULT_I2C_SCL_PIN 7
#  endif
#  ifndef DEFAULT_I2C_ADDR_ENS160
#    define DEFAULT_I2C_ADDR_ENS160 0x52
#  endif
#  ifndef DEFAULT_TRIG1
#    define DEFAULT_TRIG1   1
#  endif
#  ifndef DEFAULT_ECHO1
#    define DEFAULT_ECHO1   0
#  endif
#  ifndef DEFAULT_TRIG2
#    define DEFAULT_TRIG2   3
#  endif
#  ifndef DEFAULT_ECHO2
#    define DEFAULT_ECHO2   2
#  endif
#  ifndef DEFAULT_LED_PIN
#    define DEFAULT_LED_PIN 8           // active-low on ESP32-C3 SuperMini
#  endif
#  ifndef DEFAULT_LED_ACTIVE_LOW
#    define DEFAULT_LED_ACTIVE_LOW 1
#  endif
#else
// Classic ESP32 DevKit
#  ifndef DEFAULT_I2C_SDA_PIN
#    define DEFAULT_I2C_SDA_PIN 21
#  endif
#  ifndef DEFAULT_I2C_SCL_PIN
#    define DEFAULT_I2C_SCL_PIN 22
#  endif
#  ifndef DEFAULT_I2C_ADDR_ENS160
#    define DEFAULT_I2C_ADDR_ENS160 0x52
#  endif
#  ifndef DEFAULT_TRIG1
#    define DEFAULT_TRIG1   5
#  endif
#  ifndef DEFAULT_ECHO1
#    define DEFAULT_ECHO1   18
#  endif
#  ifndef DEFAULT_TRIG2
#    define DEFAULT_TRIG2   19
#  endif
#  ifndef DEFAULT_ECHO2
#    define DEFAULT_ECHO2   21
#  endif
#  ifndef DEFAULT_LED_PIN
#    define DEFAULT_LED_PIN 2           // active-high on ESP32 DevKit
#  endif
#  ifndef DEFAULT_LED_ACTIVE_LOW
#    define DEFAULT_LED_ACTIVE_LOW 0
#  endif
#endif
