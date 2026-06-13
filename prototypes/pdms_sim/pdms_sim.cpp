#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#define ADC_MAX 4095

typedef struct {
    float pressure;
    float vibration;
    float temperature;
    float texture;
} TactileSample;

TactileSample simulate_pdms_sensor(int time_ms) {
    TactileSample s;
    float t = time_ms / 1000.0f;
    s.pressure = 0.3f + 0.2f * sinf(t * 0.5f);
    if (rand() % 100 < 5) {
        s.pressure += 0.4f + (rand() % 100) / 500.0f;
        if (s.pressure > 1.0f) s.pressure = 1.0f;
    }
    s.vibration = 10.0f + 20.0f * sinf(t * 2.0f) + (rand() % 100) / 5.0f;
    s.temperature = 2.0f * sinf(t * 0.1f) + (rand() % 100 - 50) / 200.0f;
    s.texture = s.pressure * 0.5f + 0.1f * sinf(t * 3.0f);
    return s;
}

int adc_quantize(float value, float vref) {
    int raw = (int)((value / vref) * ADC_MAX);
    if (raw < 0) raw = 0;
    if (raw > ADC_MAX) raw = ADC_MAX;
    return raw;
}

int main() {
    srand(time(NULL));
    printf("PDMS柔性皮肤传感器仿真\n");
    printf("时间(ms)  压力    振动(Hz) 温度(C)  纹理    ADC\n");
    printf("--------------------------------------------------\n");
    
    float pressure_avg = 0.0f;
    int sample_count = 0, touch_events = 0;
    
    for (int ms = 0; ms < 5000; ms += 10) {
        TactileSample s = simulate_pdms_sensor(ms);
        int adc_val = adc_quantize(s.pressure * 3.3f, 3.3f);
        pressure_avg = pressure_avg * 0.9f + s.pressure * 0.1f;
        sample_count++;
        if (s.pressure > 0.7f) touch_events++;
        if (ms % 100 == 0)
            printf("%6d   %.3f  %.1f    %+.2f   %.3f  %4d\n", ms, s.pressure, s.vibration, s.temperature, s.texture, adc_val);
    }
    printf("\n总采样: %d  |  触摸事件: %d  |  平均压力: %.3f\n", sample_count, touch_events, pressure_avg);
    return 0;
}
