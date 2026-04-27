// hardware.h
#pragma once

#include "adc.h"
#include "can.h"
#include "gpio.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
/* Added for Zephyr's DAC API used by PA4/PA5 output support. */
#include <zephyr/drivers/dac.h>

class Hardware
{
  public:
    /* Added to match the existing implementation in hardware.cpp. */
    Hardware();
    /* Added to keep the existing CAN wrapper objects declared in the class. */
    CanBus can1;
    /* Added to keep the existing CAN wrapper objects declared in the class. */
    CanBus can2;

    int init();
    /* Added so application code can write a raw 12-bit value to PA4/DAC1_OUT1. */
    int setDAC1Value(uint16_t value);
    /* Added so application code can write a raw 12-bit value to PA5/DAC1_OUT2. */
    int setDAC2Value(uint16_t value);
    /* Added as a shared helper for selecting either DAC output by channel number. */
    int setDACValue(uint8_t channel, uint16_t value);

    /* Reads one raw sample from the external AD7708 ADCs. */
    uint16_t getADCValue(uint8_t channel);

  private:
    /* Added to store the DAC1 device used for PA4/PA5 analog outputs. */
    const struct device *dac1_dev_ = nullptr;
    const struct device *gpioe_ = nullptr;
    const struct device *gpioc_ = nullptr;
    const struct device *gpioa_ = nullptr;
    const struct device *can1_dev = nullptr;
    const struct device *can2_dev = nullptr;

    /* Added to hold the external AD7708 connected to the adc-spi1 alias. */
    AD7708 adc1_;
    /* Added to hold the external AD7708 connected to the adc-spi2 alias. */
    AD7708 adc2_;
    /* Added to hold the external AD7708 connected to the adc-spi3 alias. */
    AD7708 adc3_;
    /* Added so firmware can drive the first ADC's disable pin low to enable it. */
    GpioPin adc1_disable;
    /* Added so firmware can drive the second ADC's disable pin low to enable it. */
    GpioPin adc2_disable;
    /* Added so firmware can drive the third ADC's disable pin low to enable it. */
    GpioPin adc3_disable;

    /* Added to keep the existing VehicleState pointer declared in the class. */
    VehicleState *vehicle = nullptr;

    /* Initializes the external AD7708 ADCs declared in the board devicetree. */
    int initializeADCs();
    /* Added to initialize DAC1 channel 1 on PA4 and channel 2 on PA5. */
    int initializeDACs();
    int initializeGPIOs();
    int initializeCANs();
};
