// hardware.h
#pragma once

#include "adc.h"
#include "can.h"
#include "gpio.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/adc.h>

class Hardware
{
  public:
    int Hardware();
    // ADC Channels

    // Control signals
    GpioPin horn_signal;  // PC8
    GpioPin drive_enable; // PC9
    GpioPin air_ctrl;     // PA8

    CanBus can1;
    CanBus can2;

    int init();
    uint16_t getADCValue(uint8_t channel);

  private:
    const struct device *adc_dev_ = nullptr;
    const struct device *gpioe_ = nullptr;
    const struct device *gpioc_ = nullptr;
    const struct device *gpioa_ = nullptr;
    const struct device *can1_dev = nullptr;
    const struct device *can2_dev = nullptr;
    int initializeADCs();
    int initializeGPIOs();
    int initializeCANs();
};
