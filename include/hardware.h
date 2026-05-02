// hardware.h
#pragma once

#include "adc.h"
#include "gpio.h"
#include <zephyr/devicetree.h>
#include <zephyr/drivers/dac.h>

class Hardware
{
  public:
    /* Creates the hardware helper object used by main.cpp. */
    Hardware();

    /* Turns on the parts of the board needed for the current test program. */
    int init();
    /* Writes a 12-bit DAC code to PA4. */
    int setDAC1Value(uint16_t value);
    /* Writes a 12-bit DAC code to PA5. */
    int setDAC2Value(uint16_t value);
    /* Shared helper used by both DAC outputs. */
    int setDACValue(uint8_t channel, uint16_t value);
    /* Reads one ADC1 channel using the channel number printed in the AD7708 datasheet. */
    int readADC1Channel(uint8_t channel_number, int32_t *sample);

  private:
    /* This points at the STM32 DAC peripheral that drives PA4 and PA5. */
    const struct device *dac1_dev_ = nullptr;
    /* This object talks to the first external AD7708 ADC over SPI. */
    AD7708 adc1_;
    /* This GPIO must be driven low so ADC1 is allowed to run. */
    GpioPin adc1_disable;

    /* Turns on and configures the first external ADC. */
    int initializeADC1();
    /* Turns on and configures both DAC outputs. */
    int initializeDACs();
};
