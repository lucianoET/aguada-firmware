#pragma once
#include <stdint.h>

// Rosto do Yeti no ST7789 240×240: olhos animados num sprite de 16 bit (115 KB em RAM
// interna) empurrado inteiro por DMA a cada quadro.

enum Mood : uint8_t {
    MOOD_IDLE,      // olha em volta e pisca
    MOOD_HAPPY,     // olhos ^ ^ (carinho na cabeça)
    MOOD_SLEEPY,    // escuro: pálpebras baixas, zzz
    MOOD_LISTEN,    // tocando anúncio de outro node
    MOOD_TALK,      // no ar (PTT): boca segue o mic
    MOOD_ALARM,     // vermelho piscando, sobrancelhas bravas
    MOOD_DIZZY,     // chacoalhado: espirais
    MOOD_COLD,      // azul tremendo
    MOOD_HOT,       // laranja com gota de suor
};

void face_begin(void);
void face_set_mood(Mood m);
Mood face_mood(void);
void face_set_gaze(float x, float y);       // -1..1; NAN = olhar sozinho
void face_set_mouth(float level);           // 0..1 (voz)
void face_set_brightness(uint8_t v);        // 0..255
void face_set_net(bool ok);                 // ponto de status no canto
void face_show_info(uint32_t ms);           // tela de temperatura/umidade/luz
void face_toast(const char *msg);           // texto curto no rodapé por 2 s
void face_draw(void);                       // um quadro (chamar a ~YETI_FPS)
