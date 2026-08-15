#pragma once
// firmware/gps_tracker/include/gps_config.h
//
// Tunable GPS acquisition / gating / cadence constants. Every value here is
// #ifndef-guarded so platformio.ini `build_flags -D...` can override it
// without touching source — same shape as firmware/node/include/node_config.h.
//
// IMPORTANT: GPS_BENCH_SIM must be set to 0 before any networked build
// (Phase 3 onward, once WiFi/MQTT enter scope). It exists only to mark this
// phase as bench-only/offline and must never ship enabled once the tracker
// talks to a network.

// --- gps_reader (Task 1) -------------------------------------------------

#ifndef DEFAULT_GPS_FRESH_MS
#define DEFAULT_GPS_FRESH_MS 2500   // location.age() at/above this => stale (fix_gate REJECT_STALE)
#endif

#ifndef DEFAULT_GPS_MIN_UTC_YEAR
#define DEFAULT_GPS_MIN_UTC_YEAR 2024   // reject pre-lock default GPS dates when deriving utc_unix
#endif

#ifndef GPS_ROLLOVER_DAYS
#define GPS_ROLLOVER_DAYS 7168   // 1024 semanas — um epoch de week number (gps_reader.cpp)
#endif

#ifndef DEFAULT_GPS_HEALTH_PERIOD_MS
#define DEFAULT_GPS_HEALTH_PERIOD_MS 5000   // [HEALTH] line print period
#endif

#ifndef GPS_BENCH_SIM
#define GPS_BENCH_SIM 1
#endif

// --- fix_gate (Task 2) ---------------------------------------------------
// These four are research starting points (Assumptions A1, A2, A4 in
// 01-RESEARCH.md), not final values — tune against field data.

#ifndef DEFAULT_GPS_HDOP_MAX
#define DEFAULT_GPS_HDOP_MAX 5.0f   // reject above this HDOP (A1)
#endif

#ifndef DEFAULT_GPS_SATS_MIN
#define DEFAULT_GPS_SATS_MIN 4   // hard minimum satellites used (A2)
#endif

#ifndef DEFAULT_GPS_WARMUP_FIXES
#define DEFAULT_GPS_WARMUP_FIXES 3   // consecutive gate-passing fixes required post-cold-start (A4)
#endif

#ifndef DEFAULT_GPS_MAX_PLAUSIBLE_KMH
#define DEFAULT_GPS_MAX_PLAUSIBLE_KMH 200.0f   // implied-speed outlier gate vs last accepted fix
#endif

// --- cadence (Plan 02) ----------------------------------------------------
// These five are research starting points (Assumptions A3, A4 in
// 01-RESEARCH.md), not final values — pending a multi-hour stationary test
// in the real mounting location. Any field override belongs in
// platformio.ini build_flags, never by editing this header.

#ifndef DEFAULT_GPS_MOVING_KMH
#define DEFAULT_GPS_MOVING_KMH 5.0f   // upper hysteresis edge: at/above this is a "moving" vote (A3) —
                                       // stays clear of the bench-observed 2.6 km/h stationary jitter
#endif

#ifndef DEFAULT_GPS_STATIONARY_KMH
#define DEFAULT_GPS_STATIONARY_KMH 3.0f   // lower hysteresis edge: at/below this is a "stationary" vote (A3) —
                                            // the gap to DEFAULT_GPS_MOVING_KMH is what prevents flapping.
                                            // Second consumer (Fase 01.1, D-08): also the speed threshold
                                            // below which the OLED heading arbiter prefers the compass over
                                            // GPS course — do not add a second, competing threshold constant.
#endif

#ifndef DEFAULT_GPS_DEBOUNCE_FIXES
#define DEFAULT_GPS_DEBOUNCE_FIXES 3   // consecutive confirming accepted fixes required before a state flip (A4)
#endif

#ifndef DEFAULT_GPS_CADENCE_MOVING_S
#define DEFAULT_GPS_CADENCE_MOVING_S 8   // emit interval while MOVING, seconds — mid-point of GPS-03's 5-10s range
#endif

#ifndef DEFAULT_GPS_STATIONARY_HEARTBEAT_S
#define DEFAULT_GPS_STATIONARY_HEARTBEAT_S 0   // 0 disables the stationary heartbeat (Phase 1 default: GPS-03
                                                 // only requires acceptance to pause when parked). The constant
                                                 // exists so Phase 2's flash logger can turn on a low-rate
                                                 // "still parked" record later without touching cadence logic.
#endif

// --- perifericos I2C (Fase 01.1) ------------------------------------------
// Shared bus + per-module config for the 4 bench peripherals (OLED SSD1306,
// MPU6050 accel, HMC5883L/QMC5883L compass, HTU21D temp/hum). All planos
// 01/02/03/04 desta fase consomem exclusivamente daqui — nao adicionar mais
// nenhuma constante DEFAULT_* fora deste bloco para esta fase.

// Barramento -----------------------------------------------------------

#ifndef DEFAULT_I2C_SDA_PIN
#define DEFAULT_I2C_SDA_PIN 6   // convencao do repo (mesma pinagem do node AirQ) — esp32-devkit sobrescreve para 21
#endif

#ifndef DEFAULT_I2C_SCL_PIN
#define DEFAULT_I2C_SCL_PIN 7   // convencao do repo (mesma pinagem do node AirQ) — esp32-devkit sobrescreve para 22
#endif

#ifndef DEFAULT_I2C_CLOCK_HZ
#define DEFAULT_I2C_CLOCK_HZ 400000   // research A3; 100000 e o fallback documentado se o barramento com os
                                       // 4 modulos em paralelo se mostrar instavel na bancada
#endif

#ifndef DEFAULT_I2C_ERROR_THRESHOLD
#define DEFAULT_I2C_ERROR_THRESHOLD 3   // erros consecutivos antes de marcar um modulo offline (research A4)
#endif

#ifndef DEFAULT_I2C_RETRY_INTERVAL_MS
#define DEFAULT_I2C_RETRY_INTERVAL_MS 10000   // D-13: ~10s entre tentativas de re-init de um modulo offline
#endif

// Enderecos --------------------------------------------------------------

#ifndef DEFAULT_OLED_ADDR
#define DEFAULT_OLED_ADDR 0x3C
#endif

#ifndef DEFAULT_OLED_ADDR_ALT
#define DEFAULT_OLED_ADDR_ALT 0x3D
#endif

#ifndef DEFAULT_ACCEL_ADDR
#define DEFAULT_ACCEL_ADDR 0x68
#endif

#ifndef DEFAULT_MAG_ADDR_HMC
#define DEFAULT_MAG_ADDR_HMC 0x1E
#endif

#ifndef DEFAULT_MAG_ADDR_QMC
#define DEFAULT_MAG_ADDR_QMC 0x0D   // muitos GY-271 vem com QMC5883L em vez do HMC5883L anunciado (D-11)
#endif

#ifndef DEFAULT_MAG_ADDR_VCM
#define DEFAULT_MAG_ADDR_VCM 0x0C
#endif

#ifndef DEFAULT_ENV_ADDR
#define DEFAULT_ENV_ADDR 0x40
#endif

// Accel (D-06/D-07) --------------------------------------------------------
// research starting points (A1/A2), pendente de soak test em veiculo.

#ifndef DEFAULT_ACCEL_POLL_MS
#define DEFAULT_ACCEL_POLL_MS 100   // research starting point (A2), pendente de soak test em veiculo
#endif

#ifndef DEFAULT_ACCEL_WAKE_THRESHOLD_G
#define DEFAULT_ACCEL_WAKE_THRESHOLD_G 0.20f   // desvio absoluto em relacao ao baseline de 1g; research
                                                 // starting point (A1), pendente de soak test em veiculo
#endif

#ifndef DEFAULT_ACCEL_WAKE_DEBOUNCE_SAMPLES
#define DEFAULT_ACCEL_WAKE_DEBOUNCE_SAMPLES 3   // research starting point (A1), pendente de soak test em veiculo
#endif

// Bussola (D-09/D-10) -------------------------------------------------------

#ifndef DEFAULT_MAG_POLL_MS
#define DEFAULT_MAG_POLL_MS 250
#endif

#ifndef DEFAULT_MAG_DECLINATION
#define DEFAULT_MAG_DECLINATION 0.0f   // graus, D-10 — sem tilt compensation nesta fase (montagem plana assumida)
#endif

#ifndef DEFAULT_MAG_CAL_TIMEOUT_S
#define DEFAULT_MAG_CAL_TIMEOUT_S 120   // auto-encerramento da captura de calibracao "cal" (D-09)
#endif

// Ambiente (D-14) -----------------------------------------------------------

#ifndef DEFAULT_HTU21_POLL_MS
#define DEFAULT_HTU21_POLL_MS 4000   // a leitura da lib Adafruit bloqueia ~100ms somados (Pitfall 6 do
                                      // RESEARCH.md); este intervalo mantem o custo longe da janela de
                                      // ~266ms do buffer UART do GPS
#endif

// Display (D-02/D-04) --------------------------------------------------------

#ifndef DEFAULT_DISPLAY_REFRESH_MS
#define DEFAULT_DISPLAY_REFRESH_MS 1000   // 1 Hz, alinhado ao fix do GPS (D-02)
#endif

#ifndef DEFAULT_SPEED_UNIT
#define DEFAULT_SPEED_UNIT 0   // 0 = km/h, 1 = nos/kt (D-04)
#endif
