// ============================================================
// EmotionOS - NFC PNN Edge Node
// 神经形态感知末梢神经网络
// 目标平台: ESP32 (Arduino框架)
// ============================================================

#include <Arduino.h>

#define SENSOR_PIN      34
#define PWM_OUTPUT_PIN  2
#define LED_INDICATOR   16
#define BTN_EVENT       4

#define ADC_RESOLUTION  4096
#define PWM_RESOLUTION  256
#define PWM_FREQ        5000
#define PWM_CHANNEL     0

#define WHITE_MATTER_SIZE  64
#define GRAY_MATTER_SIZE   32
#define NEURAL_LAYER_SIZE     8
#define SPIKE_THRESHOLD       0.3f
#define REFRACTORY_PERIOD_MS  5

struct NFCState {
    float white_matter[WHITE_MATTER_SIZE];
    uint8_t white_idx;
    uint8_t gray_matter[GRAY_MATTER_SIZE];
    uint8_t gray_idx;
    float neural_layer[NEURAL_LAYER_SIZE];
    uint32_t last_spike_time[NEURAL_LAYER_SIZE];
    float subconscious_bias;
    float heart_rhythm_phase;
    uint32_t spike_count;
    uint32_t total_steps;
};

NFCState nfc;

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println(" EmotionOS NFC PNN Edge Node");
    Serial.println("========================================");
    
    analogReadResolution(12);
    ledcSetup(PWM_CHANNEL, PWM_FREQ, 8);
    ledcAttachPin(PWM_OUTPUT_PIN, PWM_CHANNEL);
    
    pinMode(LED_INDICATOR, OUTPUT);
    pinMode(BTN_EVENT, INPUT_PULLUP);
    
    memset(&nfc, 0, sizeof(nfc));
    nfc.subconscious_bias = 0.5f;
    
    Serial.println("Ready.");
}

void process_peripheral_network(float sensor_value) {
    float normalized = sensor_value / (float)ADC_RESOLUTION;
    
    nfc.white_matter[nfc.white_idx] = normalized;
    nfc.white_idx = (nfc.white_idx + 1) % WHITE_MATTER_SIZE;
    
    nfc.heart_rhythm_phase += 0.005f;
    if (nfc.heart_rhythm_phase > 6.2832f) nfc.heart_rhythm_phase -= 6.2832f;
    float rhythm = sinf(nfc.heart_rhythm_phase) * 0.3f + 0.5f;
    
    nfc.subconscious_bias += (random(-100, 100) / 100000.0f);
    nfc.subconscious_bias = constrain(nfc.subconscious_bias, 0.2f, 0.8f);
    
    float input = normalized * 0.5f + rhythm * 0.3f + nfc.subconscious_bias * 0.2f;
    
    for (int i = 0; i < NEURAL_LAYER_SIZE; i++) {
        float activation = input * (1.0f + 0.1f * i);
        uint32_t now = millis();
        
        if (now - nfc.last_spike_time[i] < REFRACTORY_PERIOD_MS) continue;
        
        if (activation > SPIKE_THRESHOLD) {
            nfc.neural_layer[i] = activation;
            nfc.last_spike_time[i] = now;
            nfc.spike_count++;
        } else {
            nfc.neural_layer[i] *= 0.95f;
        }
    }
    
    float gray_activity = 0.0f;
    for (int i = 0; i < NEURAL_LAYER_SIZE; i++) {
        gray_activity += nfc.neural_layer[i];
    }
    gray_activity /= NEURAL_LAYER_SIZE;
    
    uint8_t pwm_out = (uint8_t)constrain(gray_activity * 255.0f, 0.0f, 255.0f);
    
    nfc.gray_matter[nfc.gray_idx] = pwm_out;
    nfc.gray_idx = (nfc.gray_idx + 1) % GRAY_MATTER_SIZE;
    
    ledcWrite(PWM_CHANNEL, pwm_out);
    digitalWrite(LED_INDICATOR, gray_activity > 0.5f ? HIGH : LOW);
    
    nfc.total_steps++;
}

void loop() {
    int sensor_raw = analogRead(SENSOR_PIN);
    process_peripheral_network((float)sensor_raw);
    
    static bool last_btn_state = HIGH;
    bool btn_state = digitalRead(BTN_EVENT);
    if (last_btn_state == HIGH && btn_state == LOW) {
        Serial.println("[EVENT] Manual trigger!");
        for (int i = 0; i < NEURAL_LAYER_SIZE; i++) {
            nfc.neural_layer[i] = 0.8f + (random(0, 100) / 500.0f);
        }
    }
    last_btn_state = btn_state;
    
    static uint32_t last_debug = 0;
    if (millis() - last_debug > 2000) {
        float sensor_voltage = (sensor_raw / (float)ADC_RESOLUTION) * 3.3f;
        uint8_t latest_pwm = nfc.gray_matter[(nfc.gray_idx == 0) ? GRAY_MATTER_SIZE - 1 : nfc.gray_idx - 1];
        Serial.print(sensor_voltage, 3);
        Serial.print(" ");
        Serial.print(latest_pwm);
        Serial.print(" ");
        Serial.println(nfc.subconscious_bias, 4);
        last_debug = millis();
    }
    
    delay(10);
}
