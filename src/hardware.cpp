// hardware.cpp
#include "hardware.h"

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hardware);

namespace {
/* This tells Zephyr how the AD7708 is wired on the SPI bus. */
constexpr uint32_t kAd7708SpiOperation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB;

/* This connects the first ADC object to the ADC1 devicetree alias. */
static const AD7708Config kAdc1Config = {
    SPI_DT_SPEC_GET(DT_ALIAS(adc_spi1), kAd7708SpiOperation),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi1), drdy_gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(adc_spi1), reset_gpios),
};

/* The AD7708 produces a 16-bit code. */
constexpr int32_t kAdcFullScaleCode = 65535;
/* This test program assumes ADC1 is measuring up to 2.56 V in unipolar mode. */
constexpr int32_t kAdcFullScaleMillivolts = 2560;
/* The STM32 DAC outputs a 12-bit code. */
constexpr uint16_t kDacFullScaleCode = 4095;
/* This test program assumes the STM32 DAC runs from a 3.3 V analog supply. */
constexpr int32_t kDacFullScaleMillivolts = 3300;
} // namespace

int Hardware::init()
{
    /* This message makes it easy to see when board bring-up starts. */
    LOG_INF("Initializing the hardware needed for the ADC-to-DAC test");

    /* Start only the first external ADC because that is all the test needs. */
    if (initializeADC1() != 0)
    {
        LOG_ERR("Failed to initialize ADC1");
        return -1;
    }

    /* Start the two DAC outputs that mirror the highest ADC input. */
    if (initializeDACs() != 0)
    {
        LOG_ERR("Failed to initialize DACs");
        return -2;
    }

    /* Reaching this line means the board is ready to read and write analog values. */
    LOG_INF("Hardware initialization finished");
    return 0;
}

Hardware::Hardware()
    /* The ADC object must be connected to its devicetree wiring at construction time. */
    : adc1_(&kAdc1Config)
{
}

int Hardware::initializeADC1()
{
    /* This gets the STM32 GPIO port used by the ADC1 enable pin. */
    const struct device *gpiod_dev = DEVICE_DT_GET(DT_NODELABEL(gpiod));

    /* The board keeps ADC1 disabled until firmware drives this line low. */
    if (adc1_disable.init(gpiod_dev, 2, GPIO_OUTPUT_INACTIVE) != 0)
    {
        LOG_ERR("Failed to initialize ADC1 disable pin");
        return -1;
    }

    /* This resets the AD7708 and checks that its SPI and GPIO wiring are ready. */
    if (adc1_.init() != 0)
    {
        LOG_ERR("Failed to initialize ADC1");
        return -2;
    }

    /*
     * These settings tell the ADC to measure one channel at a time, use a
     * straight 0-to-full-scale code range, and expect signals up to about 2.56 V.
     */
    AD7708Settings settings;
    settings.mode = AD7708Mode::Idle;
    settings.polarity = AD7708Polarity::Unipolar;
    settings.input_range = AD7708InputRange::Range2V56;

    /* This writes the basic operating mode into ADC1 before the read loop starts. */
    if (adc1_.configure(settings) != 0)
    {
        LOG_ERR("Failed to configure ADC1");
        return -3;
    }

    /* This log line confirms that ADC1 is ready for channel reads. */
    LOG_INF("ADC1 is ready");
    return 0;
}

int Hardware::initializeDACs()
{
    /* This finds the STM32 DAC peripheral from the board devicetree. */
    dac1_dev_ = DEVICE_DT_GET(DT_NODELABEL(dac1));
    if (!device_is_ready(dac1_dev_))
    {
        LOG_ERR("DAC device dac1 not ready");
        return -1;
    }

    /* This sets up DAC channel 1, which is the analog output on PA4. */
    static const struct dac_channel_cfg dac1_ch1_cfg = {1, 12, true, false};

    /* This sets up DAC channel 2, which is the analog output on PA5. */
    static const struct dac_channel_cfg dac1_ch2_cfg = {2, 12, true, false};

    /* The DAC channel must be configured before any values can be written to it. */
    if (dac_channel_setup(dac1_dev_, &dac1_ch1_cfg) != 0)
    {
        LOG_ERR("Failed to setup DAC1 channel 1 (PA4)");
        return -2;
    }

    /* The second DAC output must also be configured before use. */
    if (dac_channel_setup(dac1_dev_, &dac1_ch2_cfg) != 0)
    {
        LOG_ERR("Failed to setup DAC1 channel 2 (PA5)");
        return -3;
    }

    /* Starting both outputs at zero makes startup behavior predictable. */
    if (dac_write_value(dac1_dev_, 1, 0) != 0 || dac_write_value(dac1_dev_, 2, 0) != 0)
    {
        LOG_ERR("Failed to write initial DAC values");
        return -4;
    }

    LOG_INF("DACs initialized");
    return 0;
}

int Hardware::setDACValue(uint8_t channel, uint16_t value)
{
    /* This prevents DAC writes before the DAC hardware has been initialized. */
    if (dac1_dev_ == nullptr || !device_is_ready(dac1_dev_))
    {
        return -ENODEV;
    }

    /* Only channels 1 and 2 exist on this DAC peripheral. */
    if (channel < 1 || channel > 2)
    {
        return -EINVAL;
    }

    /* The STM32 DAC accepts values from 0 to 4095 because it is a 12-bit DAC. */
    if (value > 0x0FFF)
    {
        value = 0x0FFF;
    }

    /* This writes the final value into the selected DAC output register. */
    return dac_write_value(dac1_dev_, channel, value);
}

int Hardware::setDAC1Value(uint16_t value)
{
    /* This keeps the PA4 write path short and easy to read from main.cpp. */
    return setDACValue(1, value);
}

int Hardware::setDAC2Value(uint16_t value)
{
    /* This keeps the PA5 write path short and easy to read from main.cpp. */
    return setDACValue(2, value);
}

int Hardware::readADC1Channel(uint8_t channel_number, int32_t *sample)
{
    /* The caller must give us somewhere to store the ADC result. */
    if (sample == nullptr)
    {
        return -EINVAL;
    }

    /*
     * This function now expects the same channel numbers that appear in the
     * AD7708 datasheet, such as AIN1, AIN2, AIN3, and so on.
     */
    if (channel_number < 1 || channel_number > 8)
    {
        return -EINVAL;
    }

    /* This asks ADC1 for one fresh sample from the exact datasheet channel number. */
    return adc1_.read_raw(channel_number, sample);
}
