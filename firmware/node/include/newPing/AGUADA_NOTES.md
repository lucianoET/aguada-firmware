# Biblioteca NewPing v1.9.7

## 📁 Localização
`/home/luc/Dev/espnow-ha/firmware/node/include/newPing/`

## 📚 Estrutura
```
newPing/
├── README.md
├── library.properties
├── keywords.txt
├── src/
│   ├── NewPing.h      ← Header principal
│   └── NewPing.cpp    ← Implementação
└── examples/
    ├── NewPingExample/          ← Exemplo básico
    ├── NewPing3Sensors/         ← 3 sensores simultâneos
    ├── NewPing15SensorsTimer/   ← 15 sensores com timer
    ├── NewPingEventTimer/       ← Event-driven
    ├── NewPingTimerMedian/      ← Mediana com timer (RECOMENDADO!)
    └── TimerExample/            ← Timer básico
```

## 🎯 Principais Funcionalidades

### 1. **Método ping_median() - BUILT-IN!** ⭐⭐⭐
```cpp
NewPing sonar(TRIG_PIN, ECHO_PIN, MAX_DISTANCE);
unsigned long cm = sonar.ping_median(5);  // 5 samples, retorna mediana em cm
```
- ✅ Descarta outliers automaticamente
- ✅ Retorna mediana de N samples (padrão: 5)
- ✅ Delay automático de 30ms entre samples
- ✅ Ignora leituras fora do range (NO_ECHO)

### 2. **Métodos básicos**
```cpp
// Construtor
NewPing sonar(trigger_pin, echo_pin, max_cm_distance);

// Pings
unsigned int uS = sonar.ping();              // Retorna tempo em µs
unsigned long cm = sonar.ping_cm();          // Retorna distância em cm
unsigned long in = sonar.ping_in();          // Retorna distância em polegadas

// Mediana (melhor opção!)
unsigned long cm = sonar.ping_median(5);     // 5 samples, retorna mediana
```

### 3. **Métodos estáticos de conversão**
```cpp
unsigned int cm = NewPing::convert_cm(echoTime);  // µs → cm
unsigned int in = NewPing::convert_in(echoTime);  // µs → polegadas
```

### 4. **Timer methods (avançado)**
```cpp
sonar.ping_timer(callback_function);  // Ping assíncrono
boolean ready = sonar.check_timer();   // Verifica se ping completou
unsigned long result = sonar.ping_result;  // Resultado em µs
```

## 📊 Constantes Importantes

```cpp
#define MAX_SENSOR_DISTANCE 500   // Distância máxima (cm)
#define US_ROUNDTRIP_CM 57        // µs para 1cm ida+volta
#define US_ROUNDTRIP_IN 146       // µs para 1" ida+volta
#define NO_ECHO 0                 // Valor quando não há eco
#define PING_MEDIAN_DELAY 30000   // 30ms entre pings na mediana
#define TRIGGER_WIDTH 12          // 12µs de pulso no trigger
```

## 💡 Vantagens sobre implementação manual

| Recurso | Manual (atual) | NewPing library |
|---------|---------------|-----------------|
| Mediana | ✅ Implementado | ✅ Built-in |
| Outlier rejection | ❌ Não | ✅ Automático |
| Timing preciso | ⚠️ Manual | ✅ Otimizado |
| Múltiplos sensores | ⚠️ Complexo | ✅ Simplificado |
| Timer assíncrono | ❌ Não | ✅ Sim (AVR) |
| Código | ~150 linhas | 3 linhas |

## 🚀 Uso no Aguada

### Opção 1: Usar ping_median() diretamente (SIMPLES)
```cpp
#include <NewPing.h>

NewPing sonar(TRIG_PIN, ECHO_PIN, 450);  // max 450cm

void sensor_tick(uint8_t idx) {
    // ... timer 60s ...
    
    unsigned long distance_cm = sonar.ping_median(5);  // 5 samples
    
    if (distance_cm > 0 && distance_cm <= 450) {
        uint8_t percent = calculate_percent(distance_cm, min_dist, max_dist);
        send_packet(PKT_SENSOR, sid, distance_cm, percent);
    }
}
```

### Opção 2: Manter us_dist_cm() mas usar NewPing internamente
```cpp
UltrasonicReading us_dist_cm(...) {
    NewPing sonar(trig_pin, echo_pin, max_distance);
    unsigned long raw_cm = sonar.ping_median(samples);
    
    // Calcular percentual
    UltrasonicReading result;
    result.distance_cm = raw_cm;
    result.percent = ...;
    result.valid = (raw_cm >= min_distance && raw_cm <= max_distance);
    return result;
}
```

## 📖 Exemplos Incluídos

### 1. NewPingExample.ino (básico)
Ping simples a cada 50ms, mostra distância em cm.

### 2. NewPingTimerMedian.ino (RECOMENDADO!)
- Usa timer assíncrono para não bloquear loop
- Calcula mediana de 5 samples
- 33ms entre samples
- Mostra mediana final

### 3. NewPing3Sensors.ino
Demonstra como gerenciar 3 sensores HC-SR04 simultaneamente.

## ⚠️ Compatibilidade ESP32

A biblioteca NewPing **funciona no ESP32**, mas:
- ✅ Métodos de ping padrão: **OK**
- ✅ `ping()`, `ping_cm()`, `ping_median()`: **OK** 
- ❌ Timer interrupts: **NÃO** (só AVR/Teensy/Particle)

Para ESP32, usar métodos síncronos (ping, ping_cm, ping_median) - **exatamente o que precisamos!**

## 🎓 Algoritmo da Mediana (NewPing)

```cpp
unsigned long NewPing::ping_median(uint8_t it, unsigned int max_cm_distance) {
    unsigned int uS[it], last;
    uint8_t j, i = 0;
    uS[0] = NO_ECHO;
    
    while (i < it) {
        last = ping(max_cm_distance);           // Faz ping
        if (last != NO_ECHO) {                  // Ping válido?
            if (i > 0) {                        // Insertion sort
                for (j = i; j > 0 && uS[j - 1] < last; j--)
                    uS[j] = uS[j - 1];
            } else j = 0;
            uS[j] = last;
        } else it--;                            // Ping inválido, descarta
        i++;
        if (i < it) delay(PING_MEDIAN_DELAY / 1000);  // 30ms entre pings
    }
    return (convert_cm(uS[it >> 1]));          // Retorna mediana convertida
}
```

## 📝 Notas

1. **Delay entre pings**: NewPing usa 30ms (vs nosso 100ms manual)
2. **Outlier rejection**: Automático via NO_ECHO
3. **Insertion sort**: Mais eficiente que bubble sort para pequenos arrays
4. **Conversão**: Usa constante US_ROUNDTRIP_CM=57 (vs nossa conta manual)

## 🔗 Links

- Wiki oficial: https://bitbucket.org/teckel12/arduino-new-ping/wiki/Home
- Issues: https://bitbucket.org/teckel12/arduino-new-ping/issues

---

**Recomendação**: Considerar migrar para `ping_median()` nativo da NewPing para simplificar código e ter melhor performance.
