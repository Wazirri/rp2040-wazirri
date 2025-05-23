#ifndef	voltage_h
#define	voltage_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include	<stdint.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"




int get_ADC1();

float convert_adc_to_voltage(uint16_t adc_value, uint16_t adc_range, float vref);

void checkFluctuation();
int voltageInterval(float voltage, double abs_min, double abs_min_cam ,double abs_max, 
double abs_high_min, double abs_high_min_cam, double low_min, double low_high_th, double high_min, double safety_extra);

void manageRKRegulator(int current_voltage_interval);
void manageCamRegulator(int current_voltage_interval);

// Regülatör durum değişkenleri
extern int regulatorIsOff;
extern int camregulatorIsOff;

#endif