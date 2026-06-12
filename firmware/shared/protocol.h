#pragma once
#include <stdint.h>
#include <assert.h>

// Protocol version
#define PROTO_VERSION       0x03
#ifndef ESPNOW_CHANNEL
#define ESPNOW_CHANNEL      1
#endif
#define FW_VERSION          "4.0.0"

// Defaults (used by both node and gateway)
#define DEFAULT_MEASURE_S       30
#define DEFAULT_SEND_S          120
#define DEFAULT_HEARTBEAT_S     60
#define DEFAULT_FILTER_WINDOW   5
#define DEFAULT_OUTLIER_CM      10
#define DEFAULT_THRESHOLD_CM    2
#define DEFAULT_TTL             8
#define VBAT_LOW_THRESHOLD      32
#define HC_MAX_RANGE_CM         400

// Packet types
#define PKT_SENSOR          0x01
#define PKT_HEARTBEAT       0x02
#define PKT_HELLO           0x03
#define PKT_AIR_QUALITY     0x04  // reserved for v2 extended env packet (22-byte)
#define PKT_CMD_CONFIG      0x10
#define PKT_CMD_RESTART     0x11
#define PKT_CMD_OTA_START   0x12
#define PKT_OTA_BLOCK       0x13
#define PKT_OTA_END         0x14
#define PKT_ACK             0x20

// Flags bitmask
#define FLAG_IS_RELAY        (1 << 0)
#define FLAG_OTA_PENDING     (1 << 1)
#define FLAG_SENSOR_ERROR    (1 << 2)
#define FLAG_LOW_BATTERY     (1 << 3)
#define FLAG_CONFIG_PENDING  (1 << 4)
#define FLAG_CFG_NUM_SENSORS (1 << 5)  // CMD_CONFIG carries num_sensors in distance_cm high byte
#define FLAG_BTN_HELLO       (1 << 6)  // HELLO sent by button press (not boot)
#define FLAG_IS_AIR_QUALITY  (1 << 7)  // PKT_SENSOR: distance_cm=eco2_ppm, reserved=aqi (ANI-01)

// Extended config transport (reuses PKT_CMD_CONFIG fields)
// sensor_id   -> target scope (0=global, 1/2=sensor-specific)
// seq         -> config transaction id
// distance_cm -> config value (uint16)
// vbat        -> config_group
// reserved    -> config_item
// flags       -> CFG_FLAG_*

typedef enum : uint8_t {
    CFG_GROUP_NONE      = 0x00,
    CFG_GROUP_GENERAL   = 0x01,
    CFG_GROUP_TIMING    = 0x02,
    CFG_GROUP_FILTER    = 0x03,
    CFG_GROUP_SENSOR_HW = 0x04,
    CFG_GROUP_VBAT      = 0x05,
    CFG_GROUP_COMMIT    = 0x06,
} config_group_t;

typedef enum : uint8_t {
    // CFG_GROUP_GENERAL
    CFG_ITEM_NUM_SENSORS        = 0x01,
    CFG_ITEM_LED_ENABLED        = 0x02,
    CFG_ITEM_DEEP_SLEEP_EN      = 0x03,
    CFG_ITEM_RESTART_DAILY_H    = 0x04,
    CFG_ITEM_ESPNOW_CHANNEL     = 0x05,
    CFG_ITEM_TTL_MAX            = 0x06,

    // CFG_GROUP_TIMING
    CFG_ITEM_INTERVAL_MEASURE_S = 0x11,
    CFG_ITEM_INTERVAL_SEND_S    = 0x12,
    CFG_ITEM_HEARTBEAT_S        = 0x13,

    // CFG_GROUP_FILTER
    CFG_ITEM_FILTER_WINDOW      = 0x21,
    CFG_ITEM_FILTER_OUTLIER_CM  = 0x22,
    CFG_ITEM_FILTER_THRESHOLD_CM = 0x23,

    // CFG_GROUP_SENSOR_HW (sensor_id = 1 or 2)
    CFG_ITEM_SENSOR_ENABLED     = 0x31,
    CFG_ITEM_SENSOR_TRIG_PIN    = 0x32,
    CFG_ITEM_SENSOR_ECHO_PIN    = 0x33,

    // CFG_GROUP_VBAT
    CFG_ITEM_VBAT_ENABLED       = 0x41,
    CFG_ITEM_VBAT_PIN           = 0x42,
    CFG_ITEM_VBAT_DIV           = 0x43,

    // CFG_GROUP_COMMIT
    CFG_ITEM_COMMIT_APPLY            = 0xF1,
    CFG_ITEM_COMMIT_SAVE_NVS         = 0xF2,
    CFG_ITEM_COMMIT_APPLY_AND_SAVE   = 0xF3,
    CFG_ITEM_COMMIT_SAVE_AND_RESTART = 0xF4,
} config_item_t;

typedef enum : uint8_t {
    CFG_ACK_OK                = 0x00,
    CFG_ACK_INVALID_GROUP     = 0x01,
    CFG_ACK_INVALID_ITEM      = 0x02,
    CFG_ACK_INVALID_SENSOR_ID = 0x03,
    CFG_ACK_INVALID_VALUE     = 0x04,
    CFG_ACK_RELAY_CONFLICT    = 0x05,
    CFG_ACK_SAVE_FAILED       = 0x06,
    CFG_ACK_APPLY_FAILED      = 0x07,
    CFG_ACK_RESTART_REQUIRED  = 0x08,
    CFG_ACK_UNSUPPORTED       = 0x09,
} config_ack_code_t;

#define CFG_FLAG_FIRST         (1 << 0)
#define CFG_FLAG_LAST          (1 << 1)
#define CFG_FLAG_REQUIRE_ACK   (1 << 2)
#define CFG_FLAG_ACK_ERROR     (1 << 5)
#define CFG_FLAG_ACK_REJECTED  (1 << 6)
#define CFG_FLAG_ACK_APPLIED   (1 << 7)

#define CFG_VALUE_U16(pkt)        ((uint16_t)((pkt)->distance_cm))
#define CFG_GROUP(pkt)            ((uint8_t)((pkt)->vbat))
#define CFG_ITEM(pkt)             ((uint8_t)((pkt)->reserved))
#define CFG_SENSOR_TARGET(pkt)    ((uint8_t)((pkt)->sensor_id))
#define CFG_TXN_SEQ(pkt)          ((uint16_t)((pkt)->seq))

// Special sensor_id values
#define SENSOR_ID_ENV        0xFE      // HEARTBEAT carrying relay I2C env data:
                                       //   distance_cm = (int)(temp_c*10)+1000
                                       //   reserved    = humidity % (0-100)

#define DISTANCE_ERROR      0xFFFF

typedef struct __attribute__((packed)) {
    uint8_t  version;       // 0x03
    uint8_t  type;          // PKT_*
    uint16_t node_id;       // 2 last MAC bytes
    uint8_t  sensor_id;     // 1 or 2 | 0=control/heartbeat
    uint8_t  ttl;           // decremented by relays
    uint16_t seq;           // per-node counter, wraps OK
    uint16_t distance_cm;   // filtered reading | 0xFFFF=error
    int8_t   rssi;          // filled by receiver
    int8_t   vbat;          // tenths of V (33=3.3V) | -1=n/a
    uint8_t  flags;
    uint8_t  reserved;
    uint16_t crc;           // CRC-16/CCITT over bytes 0..13
} espnow_packet_t;          // 16 bytes

static_assert(sizeof(espnow_packet_t) == 16, "Packet must be 16 bytes");
