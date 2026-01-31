// hardware.h
#pragma once

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include "adc.h"
#include "gpio.h"
#include "vehicle_state.h"

class Hardware {
public:
    Hardware(VehicleState& state); 
    // ADC Channels (8 total) - referenced as chan0..chan7
    ZephyrAdcChannel adc_chan0; // PA0  -> ADC1_INP16
    ZephyrAdcChannel adc_chan1; // PA1  -> ADC1_INP17
    ZephyrAdcChannel adc_chan2; // PA2  -> ADC1_INP14
    ZephyrAdcChannel adc_chan3; // PA3  -> ADC1_INP15
    ZephyrAdcChannel adc_chan4; // PA4  -> ADC1_INP18
    ZephyrAdcChannel adc_chan5; // PA5  -> ADC1_INP19
    ZephyrAdcChannel adc_chan6; // PA6  -> ADC1_INP3
    ZephyrAdcChannel adc_chan7; // PA7  -> ADC1_INP7

    // Error LEDs (PE2-PE6)
    ZephyrGpioPin led_yellow;   // PE2 - led0
    ZephyrGpioPin led_orange;   // PE3 - led1
    ZephyrGpioPin led_red;      // PE4 - led2
    ZephyrGpioPin led_blue;     // PE5 - led3
    ZephyrGpioPin led_green;    // PE6 - led4
    
    // Control signals
    ZephyrGpioPin horn_signal;     // PC8
    ZephyrGpioPin drive_enable;    // PC9
    ZephyrGpioPin air_ctrl;        // PA8

    int init();
    int updateAllAnalogChannels(); 
    int updateAnalogChannel(ZephyrAdcChannel& chan,float& destination);

private:
    VehicleState vehicle; 
    const struct device* adc_dev_ = nullptr;
    const struct device* gpioe_ = nullptr;
    const struct device* gpioc_ = nullptr;
    const struct device* gpioa_ = nullptr;
    int initializeADCs();  
    int initializeGPIOs();

};
