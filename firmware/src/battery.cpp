#include "battery.h"
#include "config.h"
#include <esp_adc_cal.h>

void batteryInit() {
    // ESP32-C3 ADC is noisy on battery rails. Use full 12-bit resolution and
    // 11dB attenuation so the divided lithium voltage stays inside range.
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

float readBatteryVoltage() {
    const int SAMPLES = 16;
    const int DISCARD = 2;
    int readings[SAMPLES];

    for (int i = 0; i < SAMPLES; i++) {
        readings[i] = analogRead(PIN_BAT_ADC);
        delayMicroseconds(100);
    }

    // Sort and trim outliers instead of trusting a single ADC sample. This keeps
    // displayed percentage stable enough to avoid unnecessary e-ink refreshes.
    for (int i = 0; i < SAMPLES - 1; i++)
        for (int j = i + 1; j < SAMPLES; j++)
            if (readings[i] > readings[j]) {
                int tmp = readings[i];
                readings[i] = readings[j];
                readings[j] = tmp;
            }

    long sum = 0;
    for (int i = DISCARD; i < SAMPLES - DISCARD; i++)
        sum += readings[i];

    float avgRaw = (float)sum / (SAMPLES - 2 * DISCARD);

    static esp_adc_cal_characteristics_t adcChars;
    static bool calibrated = false;
    if (!calibrated) {
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adcChars);
        calibrated = true;
    }

    uint32_t mv = esp_adc_cal_raw_to_voltage((uint32_t)avgRaw, &adcChars);
    return (mv / 1000.0f) * 2.0f;   // R1=10k, R2=10k, ratio = 2
}

int batteryPercent(float voltage) {
    // Piecewise lithium discharge curve. It is intentionally conservative at the
    // low end because sleep decisions use <=20%, <=10% and <=5% thresholds.
    if (voltage >= 4.20f) return 100;
    if (voltage >= 4.10f) return 95  + (int)((voltage - 4.10f) / 0.10f * 5);
    if (voltage >= 4.00f) return 82  + (int)((voltage - 4.00f) / 0.10f * 13);
    if (voltage >= 3.90f) return 68  + (int)((voltage - 3.90f) / 0.10f * 14);
    if (voltage >= 3.80f) return 50  + (int)((voltage - 3.80f) / 0.10f * 18);
    if (voltage >= 3.70f) return 30  + (int)((voltage - 3.70f) / 0.10f * 20);
    if (voltage >= 3.60f) return 15  + (int)((voltage - 3.60f) / 0.10f * 15);
    if (voltage >= 3.50f) return 5   + (int)((voltage - 3.50f) / 0.10f * 10);
    if (voltage >= 3.30f) return 0   + (int)((voltage - 3.30f) / 0.20f * 5);
    return 0;
}
