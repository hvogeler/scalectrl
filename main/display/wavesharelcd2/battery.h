#pragma once

void battery_init(void);
void battery_get_voltage(float *voltage, uint16_t *adc_value);
uint8_t mapBatVolt2Pct(float v);
float voltage_to_percentage_sigmoid(float voltage);
