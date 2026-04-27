// hardware.cpp
#include "hardware.h"
#include "adc.h"
#include <errno.h>
#include "zephyr/drivers/can.h"
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hardware);

namespace {
/* Added to define the SPI bus settings used by each external AD7708 transaction. */
constexpr uint32_t kAd7708SpiOperation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB;

/* Added to build the first external ADC's wiring description from devicetree. */
static const AD7708Config kAdc1Config = {
    SPI_DT_SPEC_GET(DT_ALIAS(adc_spi1), kAd7708SpiOperation),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi1), drdy_gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi1), reset_gpios),
};

/* Added to build the second external ADC's wiring description from devicetree. */
static const AD7708Config kAdc2Config = {
    SPI_DT_SPEC_GET(DT_ALIAS(adc_spi2), kAd7708SpiOperation),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi2), drdy_gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi2), reset_gpios),
};

/* Added to build the third external ADC's wiring description from devicetree. */
static const AD7708Config kAdc3Config = {
    SPI_DT_SPEC_GET(DT_ALIAS(adc_spi3), kAd7708SpiOperation),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi3), drdy_gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi3), reset_gpios),
};

/* Added to fetch the first ADC's board-level disable GPIO from devicetree. */
static const struct gpio_dt_spec kAdc1DisableSpec = GPIO_DT_SPEC_GET(DT_ALIAS(adc1_disable), gpios);
/* Added to fetch the second ADC's board-level disable GPIO from devicetree. */
static const struct gpio_dt_spec kAdc2DisableSpec = GPIO_DT_SPEC_GET(DT_ALIAS(adc2_disable), gpios);
/* Added to fetch the third ADC's board-level disable GPIO from devicetree. */
static const struct gpio_dt_spec kAdc3DisableSpec = GPIO_DT_SPEC_GET(DT_ALIAS(adc3_disable), gpios);
} // namespace

int Hardware::init()
{
    LOG_INF("Initializing hardware...");

    if (initializeADCs() != 0)
    {
        LOG_ERR("Failed to initialize ADCs");
        return -1;
    }

    /* Added to configure DAC1 so PA4 and PA5 can be driven as analog outputs. */
    if (initializeDACs() != 0)
    {
        LOG_ERR("Failed to initialize DACs");
        return -4;
    }

    if (initializeGPIOs() != 0)
    {
        LOG_ERR("Failed to initialize GPIOs");
        return -2;
    }

    if (initializeCANs() != 0)
    {
        LOG_ERR("Failed to initialize CANs");
        return -3;
    }

    LOG_INF("Hardware initialized successfully");
    return 0;
}

Hardware::Hardware()
    
{
}

int Hardware::initializeADCs()
{
    /* Added to drive the ADC disable pins low because the board pull-ups leave them disabled by default. */
    if (adc1_disable.init(&kAdc1DisableSpec, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to initialize ADC1 disable pin");
        return -1;
    }
    /* Added to drive the ADC disable pins low because the board pull-ups leave them disabled by default. */
    if (adc2_disable.init(&kAdc2DisableSpec, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to initialize ADC2 disable pin");
        return -2;
    }
    /* Added to drive the ADC disable pins low because the board pull-ups leave them disabled by default. */
    if (adc3_disable.init(&kAdc3DisableSpec, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to initialize ADC3 disable pin");
        return -3;
    }

    /* Added to initialize the first external AD7708 from the adc-spi1 devicetree alias. */
    if (adc1_.init() != 0)
    {
        LOG_ERR("Failed to initialize external ADC1");
        return -10;
    }
    /* Added to initialize the second external AD7708 from the adc-spi2 devicetree alias. */
    if (adc2_.init() != 0)
    {
        LOG_ERR("Failed to initialize external ADC2");
        return -11;
    }
    /* Added to initialize the third external AD7708 from the adc-spi3 devicetree alias. */
    if (adc3_.init() != 0)
    {
        LOG_ERR("Failed to initialize external ADC3");
        return -12;
    }

    LOG_INF("External ADCs initialized");
    return 0;
}

/* Added to configure DAC1 channel 1 on PA4 and channel 2 on PA5 as 12-bit outputs. */
int Hardware::initializeDACs()
{
    /* Added to fetch the DAC1 device enabled by the board DTS. */
    dac1_dev_ = DEVICE_DT_GET(DT_NODELABEL(dac1));
    if (!device_is_ready(dac1_dev_))
    {
        LOG_ERR("DAC device dac1 not ready");
        return -1;
    }

    /* Added to describe the PA4 DAC output channel configuration. */
    static const struct dac_channel_cfg dac1_ch1_cfg = {1, 12, true, false};

    /* Added to describe the PA5 DAC output channel configuration. */
    static const struct dac_channel_cfg dac1_ch2_cfg = {2, 12, true, false};

    /* Added to enable DAC1_OUT1 on PA4 before writes are attempted. */
    if (dac_channel_setup(dac1_dev_, &dac1_ch1_cfg) != 0)
    {
        LOG_ERR("Failed to setup DAC1 channel 1 (PA4)");
        return -2;
    }

    /* Added to enable DAC1_OUT2 on PA5 before writes are attempted. */
    if (dac_channel_setup(dac1_dev_, &dac1_ch2_cfg) != 0)
    {
        LOG_ERR("Failed to setup DAC1 channel 2 (PA5)");
        return -3;
    }

    /* Added to start both outputs at 0 V equivalent code after setup. */
    if (dac_write_value(dac1_dev_, 1, 0) != 0 || dac_write_value(dac1_dev_, 2, 0) != 0)
    {
        LOG_ERR("Failed to write initial DAC values");
        return -4;
    }

    LOG_INF("DACs initialized");
    return 0;
}

/* Added to provide a common path for writing either DAC output by channel number. */
int Hardware::setDACValue(uint8_t channel, uint16_t value)
{
    /* Added to reject DAC writes before Hardware::init() completes successfully. */
    if (dac1_dev_ == nullptr || !device_is_ready(dac1_dev_))
    {
        return -ENODEV;
    }

    /* Added to enforce the STM32 DAC channel numbers supported by this board. */
    if (channel < 1 || channel > 2)
    {
        return -EINVAL;
    }

    /* Added to clamp callers to the 12-bit range expected by the configured DAC. */
    if (value > 0x0FFF)
    {
        value = 0x0FFF;
    }

    /* Added to send the raw 12-bit code to the selected DAC output. */
    return dac_write_value(dac1_dev_, channel, value);
}

/* Added as the dedicated helper for writing DAC1_OUT1 on PA4. */
int Hardware::setDAC1Value(uint16_t value)
{
    /* Added to route PA4 writes through the shared DAC helper. */
    return setDACValue(1, value);
}

/* Added as the dedicated helper for writing DAC1_OUT2 on PA5. */
int Hardware::setDAC2Value(uint16_t value)
{
    /* Added to route PA5 writes through the shared DAC helper. */
    return setDACValue(2, value);
}

uint16_t Hardware::getADCValue(uint8_t channel)
{
    /* Added to support three 8-channel AD7708s using a flat channel numbering scheme. */
    AD7708 *adc = nullptr;
    /* Added to convert the global channel index into the per-chip AD7708 input number. */
    uint8_t local_channel = channel;

    /* Added so channels 0-7 map to the first external ADC. */
    if (channel < 8)
    {
        adc = &adc1_;
    }
    /* Added so channels 8-15 map to the second external ADC. */
    else if (channel < 16)
    {
        adc = &adc2_;
        local_channel = static_cast<uint8_t>(channel - 8);
    }
    /* Added so channels 16-23 map to the third external ADC. */
    else if (channel < 24)
    {
        adc = &adc3_;
        local_channel = static_cast<uint8_t>(channel - 16);
    }
    else
    {
        return -1;
    }

    /* Added to store the AD7708's signed sample before converting it to the existing raw API type. */
    int32_t sample = 0;
    /* Added to surface read failures through the existing uint16_t-returning API. */
    if (adc->read_raw(local_channel, &sample) != 0)
    {
        return static_cast<uint16_t>(-1);
    }

    /* Added to preserve the previous Hardware API shape expected by APPS and other callers. */
    return static_cast<uint16_t>(sample & 0xFFFF);
}

int Hardware::initializeGPIOs()
{
    // Get GPIO ports
    gpioe_ = DEVICE_DT_GET(DT_NODELABEL(gpioe));
    gpioc_ = DEVICE_DT_GET(DT_NODELABEL(gpioc));
    gpioa_ = DEVICE_DT_GET(DT_NODELABEL(gpioa));

    if (!gpioe_ || !gpioc_ || !gpioa_)
    {
        LOG_ERR("Failed to get GPIO ports");
        return -1;
    }

    // Initialize LEDs (PE2-PE6)
    if (led_yellow.init(gpioe_, 2, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init led_yellow");
        return -10;
    }
    if (led_orange.init(gpioe_, 3, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init led_orange");
        return -11;
    }
    if (led_red.init(gpioe_, 4, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init led_red");
        return -12;
    }
    if (led_blue.init(gpioe_, 5, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init led_blue");
        return -13;
    }
    if (led_green.init(gpioe_, 6, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init led_green");
        return -14;
    }

    // Initialize control signals
    if (horn_signal.init(gpioc_, 8, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init horn_signal");
        return -20;
    }
    if (drive_enable.init(gpioc_, 9, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init drive_enable");
        return -21;
    }
    if (air_ctrl.init(gpioa_, 8, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to init air_ctrl");
        return -22;
    }

    LOG_INF("GPIOs initialized");
    return 0;
}

int Hardware::initializeCANs()
{
    // Get CAN devices
    can1_dev = DEVICE_DT_GET(DT_NODELABEL(fdcan1));
    can2_dev = DEVICE_DT_GET(DT_NODELABEL(fdcan2));

    if (!can1_dev || !can2_dev)
    {
        LOG_ERR("Failed to get CAN devices");
        return -1;
    }

    // Initialize CAN1 (1 Mbps)
    if (can1.init(can1_dev, 1000000, 875) != 0)
    {
        LOG_ERR("Failed to init CAN1");
        return -10;
    }

    if (can1.start() != 0)
    {
        LOG_ERR("Failed to start CAN1");
        return -11;
    }

    // Initialize CAN2 (1 Mbps)
    if (can2.init(can2_dev, 1000000, 875) != 0)
    {
        LOG_ERR("Failed to init CAN2");
        return -20;
    }

    if (can2.start() != 0)
    {
        LOG_ERR("Failed to start CAN2");
        return -21;
    }

    LOG_INF("CANs initialized");
    return 0;
}
