// hardware.h
#pragma once

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>
#include "adc.h"
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

    int init();
    int updateAllAnalogChannels(); 
    int updateAnalogChannel(ZephyrAdcChannel& chan,float& destination);

private:
    VehicleState vehicle; 
    const struct device* adc_dev_ = nullptr;
    int initializeADCs();  
};
