// ENC28J60 SPI wiring diagnostic v2 — CS proven on D10; now permute
// {D11,D12,D13} across (SI, SO, SCK) to find the actual wiring.
// Prints EREVID for each permutation @9600. rev=0x6 → that row is the truth.
#include <Arduino.h>

struct Perm { uint8_t mosi, miso, sck; };
static const Perm perms[] = {
    {11, 12, 13}, {11, 13, 12},
    {12, 11, 13}, {12, 13, 11},
    {13, 11, 12}, {13, 12, 11},
};
static const uint8_t CS_PIN = 10;

static uint8_t xfer(const Perm& p, uint8_t out) {
    uint8_t in = 0;
    for (int8_t b = 7; b >= 0; b--) {
        digitalWrite(p.mosi, (out >> b) & 1);
        delayMicroseconds(5);
        digitalWrite(p.sck, HIGH);
        delayMicroseconds(5);
        in = (in << 1) | digitalRead(p.miso);
        digitalWrite(p.sck, LOW);
    }
    return in;
}

static uint8_t probeRevid(const Perm& p) {
    pinMode(p.mosi, OUTPUT);
    pinMode(p.miso, INPUT);
    pinMode(p.sck, OUTPUT);
    pinMode(CS_PIN, OUTPUT);
    digitalWrite(p.sck, LOW);
    digitalWrite(CS_PIN, HIGH);
    delayMicroseconds(100);

    digitalWrite(CS_PIN, LOW);  xfer(p, 0xFF);                 digitalWrite(CS_PIN, HIGH); // soft reset
    delay(2);
    digitalWrite(CS_PIN, LOW);  xfer(p, 0x40 | 0x1F); xfer(p, 0x03); digitalWrite(CS_PIN, HIGH); // ECON1=bank3
    delayMicroseconds(50);
    digitalWrite(CS_PIN, LOW);  xfer(p, 0x12); uint8_t rev = xfer(p, 0x00); digitalWrite(CS_PIN, HIGH); // RCR EREVID
    return rev;
}

void setup() {
    Serial.begin(9600);
    Serial.println(F("\n=== ENC28J60 diag v2: permutando SI/SO/SCK, CS=D10 ==="));
}

void loop() {
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t rev = probeRevid(perms[i]);
        Serial.print(F("SI=D")); Serial.print(perms[i].mosi);
        Serial.print(F(" SO=D")); Serial.print(perms[i].miso);
        Serial.print(F(" SCK=D")); Serial.print(perms[i].sck);
        Serial.print(F("  rev=0x")); Serial.print(rev, HEX);
        if (rev != 0x00 && rev != 0xFF) Serial.print(F("  <<< ACHOU! fiacao real e esta"));
        Serial.println();
    }
    Serial.println(F("---"));
    delay(3000);
}
