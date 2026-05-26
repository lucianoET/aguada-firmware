#include "sensor_filter.h"
#include <string.h>
#include <stdlib.h>

void filter_init(sensor_filter_t *f, uint8_t window, uint8_t outlier_cm) {
    memset(f, 0, sizeof(*f));
    f->window      = window ? window : DEFAULT_FILTER_WINDOW;
    f->outlier_cm  = outlier_cm ? outlier_cm : DEFAULT_OUTLIER_CM;
    f->initialized = false;
}

// Returns filtered average, or 0 if sample was rejected
uint16_t filter_update(sensor_filter_t *f, uint16_t raw) {
    // Layer 1: reject invalid or out-of-range
    if (raw == 0 || raw > HC_MAX_RANGE_CM) return 0;

    // Check if outlier BEFORE updating buffer (but still update to track changes)
    bool is_outlier = false;
    bool extreme_outlier = false;
    if (f->initialized) {
        int delta = (int)raw - (int)f->moving_avg;
        if (delta < 0) delta = -delta;
        
        // Detect extreme outliers (likely real large changes or spikes)
        if (delta > f->outlier_cm * 3) {
            extreme_outlier = true;
            
            // Auto-reset: Se receber leituras consistentes em novo range,
            // aceitar como mudança real (não ruído)
            if (f->consecutive_rejects == 0) {
                // Primeira rejeição: guarda valor
                f->reject_value = raw;
                f->consecutive_rejects = 1;
            } else {
                // Verificar se nova leitura está no mesmo range (±10cm)
                int reject_delta = (int)raw - (int)f->reject_value;
                if (reject_delta < 0) reject_delta = -reject_delta;
                
                if (reject_delta <= f->outlier_cm) {
                    // Mesmo range: incrementar contador
                    f->consecutive_rejects++;
                    f->reject_value = ((uint32_t)f->reject_value + raw) / 2;  // média móvel
                    
                    // Se 5 leituras consecutivas no mesmo range → RESET baseline
                    if (f->consecutive_rejects >= 5) {
                        // Mudança real detectada: resetar filtro com novo valor
                        for (uint8_t i = 0; i < f->window; i++) {
                            f->buf[i] = raw;
                        }
                        f->buf_idx = f->window;
                        f->moving_avg = raw;
                        f->consecutive_rejects = 0;
                        return raw;  // Aceitar e enviar imediatamente
                    }
                } else {
                    // Range diferente: resetar contador (ruído inconsistente)
                    f->consecutive_rejects = 1;
                    f->reject_value = raw;
                }
            }
            return 0;  // Rejeitar por enquanto
        }
        
        // Leitura válida dentro do range: resetar contador de rejeições
        f->consecutive_rejects = 0;
        
        // Flag moderate outliers (will update avg but not send immediately)
        if (delta > f->outlier_cm) is_outlier = true;
    }

    // Layer 2: ALWAYS update moving average with valid readings
    // This allows filter to track real changes gradually
    f->buf[f->buf_idx % f->window] = raw;
    f->buf_idx++;
    f->initialized = true;

    uint8_t count = f->buf_idx < f->window ? f->buf_idx : f->window;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < count; i++) sum += f->buf[i];
    f->moving_avg = (uint16_t)((sum + count / 2) / count);  // rounded integer

    // Return 0 for moderate outliers (filters fluctuations, but avg is updated)
    if (is_outlier) return 0;
    return f->moving_avg;
}
