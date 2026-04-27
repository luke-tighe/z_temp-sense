// zephyr_adc.cpp
#include "adc.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

/* Gives all LOG_INF/LOG_ERR messages in this file the module name "AD7708". */
LOG_MODULE_REGISTER(AD7708);

/* Constants and helper functions used only inside this adc.cpp file. */
namespace {
/* AD7708 register addresses from the datasheet. */
constexpr uint8_t AD7708_REG_STATUS = 0x00;
constexpr uint8_t AD7708_REG_MODE = 0x01;
constexpr uint8_t AD7708_REG_ADCCON = 0x02;
constexpr uint8_t AD7708_REG_FILTER = 0x03;
constexpr uint8_t AD7708_REG_DATA = 0x04;
constexpr uint8_t AD7708_REG_OFFSET = 0x05;
constexpr uint8_t AD7708_REG_GAIN = 0x06;
constexpr uint8_t AD7708_REG_IOCON = 0x07;

/* Bit 6 in the communications register selects a read operation. */
constexpr uint8_t AD7708_COMM_READ = BIT(6);
/* Maximum time to wait for one conversion before reporting a timeout. */
constexpr int32_t AD7708_DATA_READY_TIMEOUT_MS = 1000;
/* How often to check DRDY while waiting for a conversion. */
constexpr int32_t AD7708_DATA_READY_POLL_MS = 1;
/* The AD7708 channel field is 4 bits wide, so valid values are 0..15. */
constexpr uint8_t AD7708_MAX_CHANNEL = 15;
/* AD7708 has five offset registers and five gain registers. */
constexpr uint8_t AD7708_CALIBRATION_REGISTER_COUNT = 5;
/* Filter-word minimums depend on whether chop mode is enabled. */
constexpr uint8_t AD7708_MIN_FILTER_CHOP_ENABLED = 13;
constexpr uint8_t AD7708_MIN_FILTER_CHOP_DISABLED = 3;

/*
 * Builds the AD7708 communications byte.
 *
 * Every AD7708 SPI register access starts by writing this byte. It tells the
 * ADC which register to access and whether the next operation is a read/write.
 */
uint8_t make_comm(uint8_t reg, bool read)
{
    return (read ? AD7708_COMM_READ : 0U) | (reg & 0x0FU);
}
} // namespace

int AD7708::reset()
{
    /* The wrapper cannot touch GPIOs until it has a valid devicetree-backed config. */
    if (config_ == nullptr)
    {
        return -EINVAL;
    }

    /* Put RESET in its inactive state before driving it through the reset pulse. */
    int ret = gpio_pin_configure_dt(&config_->reset, GPIO_OUTPUT_INACTIVE);
    if (ret != 0)
    {
        LOG_ERR("Failed to configure AD7708 reset GPIO: %d", ret);
        return ret;
    }

    /* The devicetree flag is active-low, so logical 1 asserts the AD7708 reset input. */
    ret = gpio_pin_set_dt(&config_->reset, 1);
    if (ret != 0)
    {
        LOG_ERR("Failed to assert AD7708 reset: %d", ret);
        return ret;
    }

    /* Hold reset long enough for the ADC logic and digital filter to fully reset. */
    k_usleep(1000);

    /* Release RESET so the ADC can return to its default post-reset state. */
    ret = gpio_pin_set_dt(&config_->reset, 0);
    if (ret != 0)
    {
        LOG_ERR("Failed to deassert AD7708 reset: %d", ret);
        return ret;
    }

    /* Give the device a short recovery window before the next SPI transaction. */
    k_usleep(1000);

    LOG_DBG("AD7708 reset complete");
    return 0;
}

int AD7708::write_reg(uint8_t reg, uint8_t value){
    /* Make sure the object was constructed with a real config pointer. */
    if (config_ == nullptr) {
        return -EINVAL;
    }

    /*
     * Write transactions are two bytes:
     * 1. communications byte selecting the register
     * 2. register value
     */
    const uint8_t tx[] = {
        make_comm(reg, false),
        value,
    };
    /* spi_write_dt() expects a spi_buf_set, even for one contiguous buffer. */
    const struct spi_buf tx_buf = {const_cast<uint8_t *>(tx), sizeof(tx)};
    const struct spi_buf_set tx_set = {&tx_buf, 1};

    return spi_write_dt(&config_->spi, &tx_set);
}

int AD7708::write_reg16(uint8_t reg, uint16_t value){
    /* Make sure the object was constructed with a real config pointer. */
    if (config_ == nullptr) {
        return -EINVAL;
    }

    /*
     * AD7708 16-bit registers are transferred most-significant byte first.
     * This is used for data-like registers such as offset/gain coefficients.
     */
    const uint8_t tx[] = {
        make_comm(reg, false),
        static_cast<uint8_t>(value >> 8),
        static_cast<uint8_t>(value),
    };
    const struct spi_buf tx_buf = {const_cast<uint8_t *>(tx), sizeof(tx)};
    const struct spi_buf_set tx_set = {&tx_buf, 1};

    return spi_write_dt(&config_->spi, &tx_set);
}

int AD7708::read_reg(uint8_t reg, uint8_t *value){
    /* Make sure the object was constructed with a real config pointer. */
    if (config_ == nullptr) {
        return -EINVAL;
    }

    /* The caller must provide a place for the read result. */
    if (value == nullptr) {
        return -EINVAL;
    }

    /*
     * Read transactions send the communications byte first, then clock out
     * dummy bytes so the ADC can shift the register contents back on MISO.
     */
    uint8_t tx[] = {
        make_comm(reg, true),
        0xFF,
    };
    uint8_t rx[sizeof(tx)] = {};
    const struct spi_buf tx_buf = {tx, sizeof(tx)};
    const struct spi_buf rx_buf = {rx, sizeof(rx)};
    const struct spi_buf_set tx_set = {&tx_buf, 1};
    const struct spi_buf_set rx_set = {&rx_buf, 1};

    int ret = spi_transceive_dt(&config_->spi, &tx_set, &rx_set);
    if (ret != 0) {
        return ret;
    }

    /* rx[0] is received while the command byte is sent; rx[1] is the data byte. */
    *value = rx[1];
    return 0;
}

int AD7708::read_reg16(uint8_t reg, uint16_t *value){
    /* Make sure the object was constructed with a real config pointer. */
    if (config_ == nullptr) {
        return -EINVAL;
    }

    /* The caller must provide a place for the read result. */
    if (value == nullptr) {
        return -EINVAL;
    }

    /* Command byte plus two dummy bytes to clock out a 16-bit register. */
    uint8_t tx[] = {
        make_comm(reg, true),
        0xFF,
        0xFF,
    };
    uint8_t rx[sizeof(tx)] = {};
    const struct spi_buf tx_buf = {tx, sizeof(tx)};
    const struct spi_buf rx_buf = {rx, sizeof(rx)};
    const struct spi_buf_set tx_set = {&tx_buf, 1};
    const struct spi_buf_set rx_set = {&rx_buf, 1};

    int ret = spi_transceive_dt(&config_->spi, &tx_set, &rx_set);
    if (ret != 0) {
        return ret;
    }

    /* The AD7708 sends 16-bit register values most-significant byte first. */
    *value = (static_cast<uint16_t>(rx[1]) << 8) | rx[2];
    return 0;
}

int AD7708::wait_data_ready(){
    /* Make sure the object was constructed with a real config pointer. */
    if (config_ == nullptr) {
        return -EINVAL;
    }

    /* Convert the relative timeout into an absolute system uptime timestamp. */
    const int64_t timeout_at = k_uptime_get() + AD7708_DATA_READY_TIMEOUT_MS;

    /* Poll until DRDY becomes active or the timeout expires. */
    while (k_uptime_get() < timeout_at) {
        const int ready = gpio_pin_get_dt(&config_->drdy);
        if (ready < 0) {
            return ready;
        }

        /* gpio_pin_get_dt() returns the logical value, so active-low DRDY reads as 1 when ready. */
        if (ready != 0) {
            return 0;
        }

        k_sleep(K_MSEC(AD7708_DATA_READY_POLL_MS));
    }

    return -ETIMEDOUT;
}

int AD7708::write_settings_registers(const AD7708Settings &settings, AD7708Mode mode)
{
    /* The ADCCON channel field is 4 bits wide. */
    if (settings.active_channel > AD7708_MAX_CHANNEL) {
        return -EINVAL;
    }

    /*
     * The datasheet limits the filter word based on chop mode.
     * Reject invalid settings before writing anything to the device.
     */
    const uint8_t min_filter =
        settings.chop_disabled ? AD7708_MIN_FILTER_CHOP_DISABLED : AD7708_MIN_FILTER_CHOP_ENABLED;
    if (settings.filter_word < min_filter) {
        return -EINVAL;
    }

    /*
     * Put the ADC in idle before changing configuration/calibration registers.
     * This avoids updating important settings mid-conversion.
     */
    int ret = write_reg(AD7708_REG_MODE, static_cast<uint8_t>(AD7708Mode::Idle));
    if (ret != 0) {
        return ret;
    }

    /* Pack AD7708Settings fields into the MODE register bit layout. */
    const uint8_t mode_reg =
        (settings.chop_disabled ? BIT(7) : 0U) |
        (settings.aincom_buffer_enabled ? BIT(6) : 0U) |
        (static_cast<uint8_t>(settings.reference) << 5) |
        (static_cast<uint8_t>(settings.channel_configuration) << 4) |
        (settings.oscillator_power_down_in_standby ? BIT(3) : 0U) |
        static_cast<uint8_t>(mode);

    /* Pack selected channel, polarity, and input range into ADCCON. */
    const uint8_t adccon_reg =
        ((settings.active_channel & 0x0FU) << 4) |
        (static_cast<uint8_t>(settings.polarity) << 3) |
        static_cast<uint8_t>(settings.input_range);

    /* Pack the optional AD7708 P1/P2 digital I/O settings into IOCON. */
    const uint8_t iocon_reg =
        (static_cast<uint8_t>(settings.p2_direction) << 5) |
        (static_cast<uint8_t>(settings.p1_direction) << 4) |
        (settings.p2_output_value ? BIT(1) : 0U) |
        (settings.p1_output_value ? BIT(0) : 0U);

    /* Program conversion rate/filtering before starting conversions. */
    ret = write_reg(AD7708_REG_FILTER, settings.filter_word);
    if (ret != 0) {
        return ret;
    }

    /* Program the optional P1/P2 digital I/O pins. */
    ret = write_reg(AD7708_REG_IOCON, iocon_reg);
    if (ret != 0) {
        return ret;
    }

    /* Program the channel/range/polarity register. */
    ret = write_reg(AD7708_REG_ADCCON, adccon_reg);
    if (ret != 0) {
        return ret;
    }

    /*
     * Calibration coefficients are optional. If requested, write them while
     * the ADC is idle, then restore ADCCON afterward.
     */
    for (uint8_t i = 0; i < AD7708_CALIBRATION_REGISTER_COUNT; ++i) {
        if (settings.write_offset_calibration[i]) {
            /* Select which calibration register pair the offset write targets. */
            ret = write_reg(AD7708_REG_ADCCON, static_cast<uint8_t>((i & 0x0FU) << 4));
            if (ret != 0) {
                return ret;
            }

            ret = write_reg16(AD7708_REG_OFFSET, settings.offset_calibration[i]);
            if (ret != 0) {
                return ret;
            }
        }

        if (settings.write_gain_calibration[i]) {
            /* Select which calibration register pair the gain write targets. */
            ret = write_reg(AD7708_REG_ADCCON, static_cast<uint8_t>((i & 0x0FU) << 4));
            if (ret != 0) {
                return ret;
            }

            ret = write_reg16(AD7708_REG_GAIN, settings.gain_calibration[i]);
            if (ret != 0) {
                return ret;
            }
        }
    }

    /* Restore the caller's requested channel/range/polarity after calibration writes. */
    ret = write_reg(AD7708_REG_ADCCON, adccon_reg);
    if (ret != 0) {
        return ret;
    }

    /* Last step: write MODE to start the requested ADC mode. */
    return write_reg(AD7708_REG_MODE, mode_reg);
}

AD7708::AD7708(const AD7708Config *config) : config_(config), settings_(), configured_(false)
{
    /* Constructors cannot return hardware errors, so init() performs device checks. */
}

int AD7708::init(){
    LOG_INF("AD7708 init started");

    /* A missing config is a programming error by the caller. */
    if (config_ == nullptr) {
        return -EINVAL;
    }

    /* Verify the SPI controller referenced by the AD7708 devicetree node is ready. */
    if (!spi_is_ready_dt(&config_->spi)) {
        return -ENODEV;
    }

    /* DRDY must be available before reads can wait for completed conversions. */
    if (!gpio_is_ready_dt(&config_->drdy)) {
        return -ENODEV;
    }

    /* RESET must be available before this wrapper can place the ADC in a known state. */
    if (!gpio_is_ready_dt(&config_->reset)) {
        return -ENODEV;
    }

    /* Configure DRDY as an input; the AD7708 drives it low when data is ready. */
    int ret = gpio_pin_configure_dt(&config_->drdy, GPIO_INPUT);
    if (ret != 0) {
        LOG_ERR("Failed to configure AD7708 DRDY GPIO: %d", ret);
        return ret;
    }

    /* Finish initialization by forcing the ADC into a known post-reset state. */
    ret = reset();
    if (ret != 0) {
        return ret;
    }

    LOG_INF("AD7708 init complete");
    return 0;
}

int AD7708::configure(const AD7708Settings &settings){
    /* Write the caller's settings into the chip using the requested mode. */
    const int ret = write_settings_registers(settings, settings.mode);
    if (ret != 0) {
        return ret;
    }

    /* Remember the latest good settings so read_raw() can reuse them. */
    settings_ = settings;
    configured_ = true;
    return 0;
}

int AD7708::read_raw(uint8_t channel, int32_t *sample){
    /* The caller must provide storage for the returned raw ADC code. */
    if (sample == nullptr) {
        return -EINVAL;
    }

    /* The AD7708 channel field is 4 bits wide. */
    if (channel > AD7708_MAX_CHANNEL) {
        return -EINVAL;
    }

    /*
     * Use the existing configuration if one was applied. Otherwise use the
     * default AD7708Settings values from adc.h.
     */
    AD7708Settings read_settings = configured_ ? settings_ : AD7708Settings{};
    read_settings.active_channel = channel;

    /* Start a single conversion on the requested channel. */
    int ret = write_settings_registers(read_settings, AD7708Mode::SingleConversion);
    if (ret != 0) {
        return ret;
    }

    /* Wait until the ADC reports that the conversion result is ready. */
    ret = wait_data_ready();
    if (ret != 0) {
        return ret;
    }

    /* Read the 16-bit conversion result. */
    uint16_t raw = 0;
    ret = read_reg16(AD7708_REG_DATA, &raw);
    if (ret != 0) {
        return ret;
    }

    /* Return the raw code as int32_t so callers have room for later math. */
    *sample = raw;
    /* Keep the selected channel as part of the remembered settings. */
    settings_ = read_settings;
    configured_ = true;
    return 0;
}
