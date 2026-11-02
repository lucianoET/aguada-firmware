#pragma once
// Pinos e defaults do communicator_c3 (ESP32-C3 SuperMini). Todos sobrescrevíveis por -D.
// Pinagem e ligações: COMMUNICATOR.md

// ── I2S full-duplex (mic e saída dividem BCLK/WS) ──────────────────────────────
#ifndef COMM_PIN_I2S_BCLK
#define COMM_PIN_I2S_BCLK   4     // MAX98357A/PCM5102A BCLK + INMP441 SCK
#endif
#ifndef COMM_PIN_I2S_WS
#define COMM_PIN_I2S_WS     5     // LRC + INMP441 WS
#endif
#ifndef COMM_PIN_I2S_DOUT
#define COMM_PIN_I2S_DOUT   3     // MAX98357A DIN / PCM5102A DIN
#endif
#ifndef COMM_PIN_I2S_DIN
#define COMM_PIN_I2S_DIN    10    // INMP441 SD
#endif

// ── I2C (OLED) ─────────────────────────────────────────────────────────────────
#ifndef COMM_PIN_SDA
#define COMM_PIN_SDA        6
#endif
#ifndef COMM_PIN_SCL
#define COMM_PIN_SCL        7
#endif
#define COMM_OLED_ADDR      0x3C

// ── Botões (ao GND, pull-up interno) e saídas ──────────────────────────────────
#ifndef COMM_PIN_PTT
#define COMM_PIN_PTT        1
#endif
#ifndef COMM_PIN_ALARM
#define COMM_PIN_ALARM      0
#endif
#ifndef COMM_PIN_VOL
#define COMM_PIN_VOL        9     // botão BOOT da placa
#endif
#ifndef COMM_PIN_SIREN
#define COMM_PIN_SIREN      20    // MOSFET/relé da sirene 12 V (ativo alto)
#endif
#ifndef COMM_PIN_LED
#define COMM_PIN_LED        8     // LED da placa, ativo baixo
#endif

// ── Identidade / comportamento ─────────────────────────────────────────────────
#ifndef COMM_NAME
#define COMM_NAME           "COMM"
#endif
#ifndef COMM_ZONE
#define COMM_ZONE           0     // 0 = todas as zonas
#endif
#ifndef COMM_PRIO
#define COMM_PRIO           1     // 0..3; maior toma o canal de quem está falando
#endif
#ifndef COMM_PTT_CHIME
#define COMM_PTT_CHIME      1     // gongo de 2 tons antes da voz
#endif
#ifndef COMM_SIREN_MAX_S
#define COMM_SIREN_MAX_S    180   // sirene desliga sozinha (o alarme continua no OLED)
#endif
#ifndef COMM_VOLUME_DEFAULT
#define COMM_VOLUME_DEFAULT 75    // %
#endif
#ifndef COMM_MIC_MAX_GAIN_DB
#define COMM_MIC_MAX_GAIN_DB 40
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

// ── Áudio ──────────────────────────────────────────────────────────────────────
#define AUDIO_RATE_HZ       16000
#define AUDIO_FRAME         320   // 20 ms por pacote de voz
