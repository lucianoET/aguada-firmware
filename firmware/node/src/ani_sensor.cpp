#include "ani_sensor.h"
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <ScioSense_ENS160.h>
#include <esp_log.h>

static const char *TAG = "ani";

static Adafruit_AHTX0   s_aht;
static ScioSense_ENS160 s_ens(ENS160_I2CADDR_0);

static bool s_aht_ok = false;
static bool s_ens_ok = false;

static uint8_t s_sda = 6;
static uint8_t s_scl = 7;

bool ani_init(uint8_t sda_pin, uint8_t scl_pin) {
    s_sda = sda_pin;
    s_scl = scl_pin;

    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(50000);
    delay(50);

    // Full I2C scan — log every device found on the bus
    ESP_LOGI(TAG, "I2C scan (sda=%d scl=%d):", sda_pin, scl_pin);
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            ESP_LOGI(TAG, "  found 0x%02X", addr);
        }
    }

    // AHT21 — I2C address 0x38 (fixed)
    // Pass &Wire explicitly; Adafruit_AHTX0 still calls Wire.begin() internally
    s_aht_ok = s_aht.begin(&Wire);
    if (!s_aht_ok) {
        ESP_LOGW(TAG, "AHT21 not found (sda=%d scl=%d)", sda_pin, scl_pin);
    } else {
        ESP_LOGI(TAG, "AHT21 OK");
    }

    // ENS160 — I2C address 0x52 (ADDR pin low)
    s_ens.begin();
    if (!s_ens.available()) {
        ESP_LOGW(TAG, "ENS160 not found");
        s_ens_ok = false;
    } else {
        s_ens.setMode(ENS160_OPMODE_STD);
        s_ens_ok = true;
        ESP_LOGI(TAG, "ENS160 OK (fw %d.%d.%d)",
                 s_ens.getMajorRev(), s_ens.getMinorRev(), s_ens.getBuild());
#ifdef AIRQ_DIAG
        {   // DIAG temporário: acompanha DATA_STATUS por 6 s após entrar em STD
            auto w8 = [](uint8_t reg, uint8_t v) { Wire.beginTransmission(ENS160_I2CADDR_0); Wire.write(reg); Wire.write(v); return Wire.endTransmission(); };
            int e1 = w8(0x10, 0xF0); delay(100);   // RESET
            int e2 = w8(0x10, 0x01); delay(100);   // IDLE
            int e3 = w8(0x12, 0x00); delay(20);    // NOP
            int e4 = w8(0x12, 0xCC); delay(20);    // CLRGPR
            int e5 = w8(0x10, 0x02); delay(100);   // STD
            ESP_LOGI(TAG, "DIAG seq reset/idle/clr/std err=%d%d%d%d%d", e1, e2, e3, e4, e5);
            int last = -1; uint32_t t0 = millis();
            while (millis() - t0 < 6000) {
                Wire.beginTransmission(ENS160_I2CADDR_0); Wire.write(0x20);
                int st = (Wire.endTransmission(false) == 0 && Wire.requestFrom(ENS160_I2CADDR_0, 1) == 1) ? Wire.read() : -1;
                if (st != last) { ESP_LOGI(TAG, "DIAG t=%lu status=0x%02X", (unsigned long)(millis() - t0), st & 0xFF); last = st; }
                if (st > 0 && (st & 0x02)) {   // NEWDAT → lê eCO2 para limpar
                    uint8_t b[5]; Wire.beginTransmission(ENS160_I2CADDR_0); Wire.write(0x21); Wire.endTransmission(false);
                    Wire.requestFrom(ENS160_I2CADDR_0, 5); for (int i = 0; i < 5; i++) b[i] = Wire.read();
                    ESP_LOGI(TAG, "DIAG   aqi=%u tvoc=%u eco2=%u", b[0], b[1] | (b[2] << 8), b[3] | (b[4] << 8));
                    last = -1;
                }
                delay(50);
            }
        }
#endif
    }

    // Restore correct pins and clock after library Wire.begin() calls
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(50000);

    return s_aht_ok && s_ens_ok;
}

void ani_status(bool *aht_ok, bool *ens_ok) {
    if (aht_ok) *aht_ok = s_aht_ok;
    if (ens_ok) *ens_ok = s_ens_ok;
}

AniReading ani_read(void) {
    AniReading r = {};

    if (!s_aht_ok && !s_ens_ok) {
        ESP_LOGW(TAG, "Sensor(s) not available: aht=%d ens=%d", s_aht_ok, s_ens_ok);
        return r;
    }

    // Ensure clock didn't drift after other library inits
    Wire.setClock(50000);

    // 1. Read AHT21 (temp + hum)
    static float s_last_temp = 25.0f;
    static float s_last_hum  = 50.0f;
    sensors_event_t hum_evt, temp_evt;
    bool aht_ok = s_aht_ok && s_aht.getEvent(&hum_evt, &temp_evt);
    if (s_aht_ok && !aht_ok) {
        ESP_LOGW(TAG, "AHT21 read failed — retrying");
        Wire.begin(s_sda, s_scl);
        Wire.setClock(50000);
        aht_ok = s_aht.getEvent(&hum_evt, &temp_evt);
    }
    if (aht_ok) {
        r.temp_c  = temp_evt.temperature;
        r.hum_pct = hum_evt.relative_humidity;
        s_last_temp = r.temp_c;
        s_last_hum  = r.hum_pct;
        r.th_valid  = true;
        ESP_LOGI(TAG, "AHT21 T=%.1fC H=%.0f%%", r.temp_c, r.hum_pct);
    } else {
        // AHT21 unavailable — use last known values and continue reading ENS160
        r.temp_c  = s_last_temp;
        r.hum_pct = s_last_hum;
        ESP_LOGW(TAG, "AHT21 read failed after recovery — using last T=%.1fC H=%.0f%%",
                 r.temp_c, r.hum_pct);
    }

    if (!s_ens_ok) return r;   // só AHT21: T/H seguem (th_valid), sem VOC

    // 2. Feed T/H compensation to ENS160 (improves accuracy)
    s_ens.set_envdata(r.temp_c, r.hum_pct);

    // 3. Esperar dado novo do ENS160 (NEWDAT, 1 Hz em modo STD) com prazo.
    // NÃO usar measure(true): a lib fica em loop sem timeout até NEWDAT=1 —
    // se o ENS160 não sinalizar (ou o I2C falhar) o loop() trava para sempre.
    // available() da lib só diz "sensor presente", não "dado pronto".
    static uint8_t s_ens_miss = 0;
    uint32_t deadline = millis() + 1500UL;
    bool got = false;
    while (!(got = s_ens.measure(false)) && (int32_t)(millis() - deadline) < 0) {
        delay(20);
    }
    if (!got) {
        auto rd = [](uint8_t reg) -> int {
            Wire.beginTransmission(ENS160_I2CADDR_0);
            Wire.write(reg);
            if (Wire.endTransmission(false) != 0) return -1;
            if (Wire.requestFrom(ENS160_I2CADDR_0, 1) != 1) return -2;
            return Wire.read();
        };
        ESP_LOGW(TAG, "ENS160 sem dado novo em 1.5 s (status=0x%02X opmode=0x%02X, falhas=%u)",
                 rd(0x20) & 0xFF, rd(0x10) & 0xFF, (unsigned)(s_ens_miss + 1));
        if (++s_ens_miss >= 5) {                            // ~40 s sem dado → reinicia modo
            s_ens_miss = 0;
            s_ens.setMode(ENS160_OPMODE_STD);
            ESP_LOGW(TAG, "ENS160 reconfigurado (modo STD)");
        }
        return r;
    }
    s_ens_miss = 0;

    r.eco2  = s_ens.geteCO2();
    r.tvoc  = s_ens.getTVOC();
    r.aqi   = s_ens.getAQI();
    // eCO2 < 400 (ex.: 0) = ENS160 não está medindo de verdade (heater parado) —
    // não publicar como leitura válida.
    r.valid = r.eco2 >= 400;
    if (!r.valid) {
        ESP_LOGW(TAG, "ENS160 devolveu eCO2=%u (fora da faixa) — descartado", r.eco2);
        return r;
    }

    ESP_LOGI(TAG, "ANI eco2=%uppm tvoc=%uppb aqi=%u T=%.1fC H=%.0f%%",
             r.eco2, r.tvoc, r.aqi, r.temp_c, r.hum_pct);
    return r;
}
