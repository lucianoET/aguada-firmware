/*
 * Firmware de Teste I2C — ESP32 DevKit
 *
 * Pinos: SDA=GPIO21 (D21), SCL=GPIO22 (D22)
 * Sensores testados:
 *   HTU21  → 0x40  (temperatura + umidade)
 *   AHT20  → 0x38  (temperatura + umidade)
 *   OLED   → 0x3C  (SSD1306 128×64) — exibe leituras em tempo real
 *
 * Ao iniciar, faz scan completo do barramento I2C e imprime no Serial.
 * Loop: lê os dois sensores e atualiza o display a cada 2 s.
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_HTU21DF.h>
#include <Adafruit_AHTX0.h>

// ── Pinos I2C ─────────────────────────────────────────────────────────────────
#define I2C_SDA_PIN  21
#define I2C_SCL_PIN  22
#define I2C_FREQ_HZ  100000UL

// ── OLED SSD1306 ──────────────────────────────────────────────────────────────
#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_ADDR    0x3C   // 0x3D para alguns displays com jumper A0
#define OLED_RESET    -1    // sem pino de reset

// ── Objetos ───────────────────────────────────────────────────────────────────
static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
static Adafruit_HTU21DF htu21;
static Adafruit_AHTX0   aht20;

static bool g_oled_ok  = false;
static bool g_htu21_ok = false;
static bool g_aht20_ok = false;

// ── Utilitários ───────────────────────────────────────────────────────────────

static const char *sensor_tag(uint8_t addr) {
    switch (addr) {
        case 0x3C: case 0x3D: return "OLED SSD1306";
        case 0x38:            return "AHT10/AHT20";
        case 0x40:            return "HTU21/SHT21/HD21D";
        case 0x44: case 0x45: return "SHT3x";
        case 0x48:            return "ADS1115/TMP102";
        case 0x68: case 0x69: return "MPU-6050/DS3231";
        case 0x76: case 0x77: return "BME280/BMP280";
        default:              return "";
    }
}

// Varre todo o barramento e imprime os endereços encontrados
static void i2c_scan(void) {
    Serial.println("\n╔══════════════════════════════╗");
    Serial.println( "║    SCAN DO BARRAMENTO I2C    ║");
    Serial.println( "╚══════════════════════════════╝");

    // Suprimir logs de erro do driver I2C (ESP-IDF 5.x loga cada NACK em ERROR)
    esp_log_level_set("i2c.master", ESP_LOG_NONE);

    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            const char *tag = sensor_tag(addr);
            if (tag[0])
                Serial.printf("  [0x%02X] OK  -> %s\n", addr, tag);
            else
                Serial.printf("  [0x%02X] OK\n", addr);
            found++;
        }
    }

    if (found == 0)
        Serial.println("  Nenhum dispositivo encontrado!");
    else
        Serial.printf("\n  Total: %d dispositivo(s)\n", found);
    Serial.println("══════════════════════════════\n");
}

// Mostra linha de status no Serial
static void print_init_status(const char *name, bool ok) {
    Serial.printf("  %-10s : %s\n", name, ok ? "OK" : "FALHA — não detectado");
}

// ── Tela OLED — helpers ───────────────────────────────────────────────────────

static void oled_header(void) {
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0, 0);
    oled.println("=== I2C TEST ESP32 ===");
}

// Formata float com 1 casa ou "---" se NAN
static const char *fmt_f(char *buf, float v, const char *unit) {
    if (isnan(v))
        snprintf(buf, 16, "---");
    else
        snprintf(buf, 16, "%.1f%s", v, unit);
    return buf;
}

// ── Setup ─────────────────────────────────────────────────────────────────────

void setup(void) {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n\n===== FIRMWARE DE TESTE I2C =====");
    Serial.printf("SDA = GPIO%d   SCL = GPIO%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
    Serial.printf("Frequência : %lu Hz\n", I2C_FREQ_HZ);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQ_HZ);
    delay(50);

    // Scan do barramento antes de inicializar qualquer sensor
    i2c_scan();

    // ── OLED SSD1306 ──────────────────────────────────────────────────────────
    Serial.println("Inicializando sensores...");
    g_oled_ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    print_init_status("OLED", g_oled_ok);
    if (g_oled_ok) {
        oled.clearDisplay();
        oled_header();
        oled.setCursor(0, 16);
        oled.println("Inicializando...");
        oled.display();
    }

    // ── HTU21 ─────────────────────────────────────────────────────────────────
    g_htu21_ok = htu21.begin();
    print_init_status("HTU21", g_htu21_ok);

    // ── AHT20 ─────────────────────────────────────────────────────────────────
    g_aht20_ok = aht20.begin();
    print_init_status("AHT20", g_aht20_ok);

    // Resumo no OLED
    if (g_oled_ok) {
        oled.clearDisplay();
        oled_header();
        oled.setCursor(0, 16);
        oled.printf("HTU21 : %s\n", g_htu21_ok ? "OK" : "FALHA");
        oled.printf("AHT20 : %s\n", g_aht20_ok ? "OK" : "FALHA");
        oled.println();
        oled.println("Iniciando leituras...");
        oled.display();
        delay(2000);
    }

    Serial.println("\n===== LOOP INICIADO =====");
}

// ── Loop ──────────────────────────────────────────────────────────────────────

void loop(void) {
    float htu_temp = NAN, htu_hum = NAN;
    float aht_temp = NAN, aht_hum = NAN;

    // ── Leitura HTU21 ─────────────────────────────────────────────────────────
    if (g_htu21_ok) {
        htu_temp = htu21.readTemperature();
        htu_hum  = htu21.readHumidity();
        // Validação básica: descartar leituras fora do range físico
        if (htu_temp < -40.0f || htu_temp > 125.0f) htu_temp = NAN;
        if (htu_hum  <   0.0f || htu_hum  > 100.0f) htu_hum  = NAN;
        Serial.printf("[HTU21]  T = %.2f °C   RH = %.2f %%\n", htu_temp, htu_hum);
    } else {
        Serial.println("[HTU21]  não detectado");
    }

    // ── Leitura AHT20 ─────────────────────────────────────────────────────────
    if (g_aht20_ok) {
        sensors_event_t h_evt, t_evt;
        aht20.getEvent(&h_evt, &t_evt);
        aht_temp = t_evt.temperature;
        aht_hum  = h_evt.relative_humidity;
        if (aht_temp < -40.0f || aht_temp > 85.0f) aht_temp = NAN;
        if (aht_hum  <   0.0f || aht_hum  > 100.0f) aht_hum  = NAN;
        Serial.printf("[AHT20]  T = %.2f °C   RH = %.2f %%\n", aht_temp, aht_hum);
    } else {
        Serial.println("[AHT20]  não detectado");
    }

    // ── Diferença entre sensores (quando ambos OK) ────────────────────────────
    if (!isnan(htu_temp) && !isnan(aht_temp)) {
        Serial.printf("[DIFF ]  ΔT = %+.2f °C   ΔRH = %+.2f %%\n",
                      htu_temp - aht_temp, htu_hum - aht_hum);
    }

    Serial.printf("[UPTIME] %lu s\n\n", millis() / 1000);

    // ── Atualiza OLED ─────────────────────────────────────────────────────────
    if (g_oled_ok) {
        char buf[16];
        oled.clearDisplay();
        oled_header();

        // HTU21 — linha 2 e 3
        oled.setCursor(0, 10);
        oled.setTextSize(1);
        if (g_htu21_ok) {
            oled.printf("HTU21  T:%s\n", fmt_f(buf, htu_temp, "C"));
            oled.printf("       H:%s\n", fmt_f(buf, htu_hum,  "%"));
        } else {
            oled.println("HTU21 : nao detec.");
        }

        // Separador
        oled.drawFastHLine(0, 34, OLED_WIDTH, SSD1306_WHITE);

        // AHT20 — linha 5 e 6
        oled.setCursor(0, 37);
        if (g_aht20_ok) {
            oled.printf("AHT20  T:%s\n", fmt_f(buf, aht_temp, "C"));
            oled.printf("       H:%s\n", fmt_f(buf, aht_hum,  "%"));
        } else {
            oled.println("AHT20 : nao detec.");
        }

        // Uptime na última linha
        oled.setCursor(0, 57);
        oled.printf("up:%lus", millis() / 1000);

        oled.display();
    }

    delay(2000);
}
