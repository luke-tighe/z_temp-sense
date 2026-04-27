// zephyr_adc.h
#pragma once

/* Fixed-width integer types like uint8_t, uint16_t, and int32_t. */
#include <stdint.h>
/* Zephyr SPI API and spi_dt_spec helper type. */
#include <zephyr/drivers/spi.h>
/* Zephyr GPIO API and gpio_dt_spec helper type. */
#include <zephyr/drivers/gpio.h>
/* Zephyr base device type. */
#include <zephyr/device.h>

/*
 * The AD7708 MODE register selects what the ADC is doing right now.
 * These values are written into the MODE register's MD2..MD0 bits.
 */
enum class AD7708Mode : uint8_t {
    PowerDown = 0,
    Idle = 1,
    SingleConversion = 2,
    ContinuousConversion = 3,
    InternalZeroScaleCalibration = 4,
    InternalFullScaleCalibration = 5,
    SystemZeroScaleCalibration = 6,
    SystemFullScaleCalibration = 7,
};

/*
 * The AD7708 can expose either 8 or 10 pseudo-differential inputs depending
 * on whether pins AIN9/AIN10 are used as extra analog inputs or as REF2.
 */
enum class AD7708ChannelConfiguration : uint8_t {
    EightPseudoDifferentialOrFourDifferential = 0,
    TenPseudoDifferentialOrFiveDifferential = 1,
};

/* Selects which external reference input pair the ADC uses. */
enum class AD7708Reference : uint8_t {
    RefIn1 = 0,
    RefIn2 = 1,
};

/*
 * Programmable input ranges supported by the AD7708 PGA.
 * The correct range depends on the largest signal you expect to measure.
 */
enum class AD7708InputRange : uint8_t {
    Range20mV = 0,
    Range40mV = 1,
    Range80mV = 2,
    Range160mV = 3,
    Range320mV = 4,
    Range640mV = 5,
    Range1V28 = 6,
    Range2V56 = 7,
};

/*
 * Bipolar mode returns codes around mid-scale for zero input.
 * Unipolar mode returns straight-binary codes starting at zero input.
 */
enum class AD7708Polarity : uint8_t {
    Bipolar = 0,
    Unipolar = 1,
};

/* The AD7708 has two optional digital I/O pins, P1 and P2. */
enum class AD7708GpioDirection : uint8_t {
    Input = 0,
    Output = 1,
};

/*
 * Board wiring for one physical AD7708 chip.
 *
 * This should come from devicetree:
 * - spi: which SPI bus and chip-select settings talk to this ADC
 * - drdy: the ADC data-ready pin
 * - reset: the ADC hardware reset pin
 */
struct AD7708Config {
    struct spi_dt_spec spi;
    struct gpio_dt_spec drdy;
    struct gpio_dt_spec reset;
};

/*
 * Runtime settings that control how the AD7708 performs conversions.
 *
 * This is intentionally separate from AD7708Config:
 * - AD7708Config describes board wiring.
 * - AD7708Settings describes ADC behavior.
 */
struct AD7708Settings {
    /* MODE register fields. */
    /* The operating mode to put the ADC in when configure() is called. */
    AD7708Mode mode = AD7708Mode::Idle;
    /* false keeps chop enabled for better analog performance; true improves throughput. */
    bool chop_disabled = false;
    /* Enables the internal AINCOM input buffer. */
    bool aincom_buffer_enabled = false;
    /* Selects REF1 or REF2. */
    AD7708Reference reference = AD7708Reference::RefIn1;
    /* Selects whether the part is used in 8-channel or 10-channel mode. */
    AD7708ChannelConfiguration channel_configuration =
        AD7708ChannelConfiguration::EightPseudoDifferentialOrFourDifferential;
    /* Allows the oscillator to power down while the ADC is in standby/power-down. */
    bool oscillator_power_down_in_standby = false;

    /* ADCCON register fields. */
    /* The AD7708 input channel selected for conversion. */
    uint8_t active_channel = 0;
    /* Selects bipolar or unipolar output coding. */
    AD7708Polarity polarity = AD7708Polarity::Bipolar;
    /* Selects the PGA input range. */
    AD7708InputRange input_range = AD7708InputRange::Range2V56;

    /* FILTER register field. */
    /*
     * Controls output update rate and filtering. The reset default is 0x45.
     * Smaller values are faster; larger values are slower and usually quieter.
     */
    uint8_t filter_word = 0x45;

    /* IOCON register fields for the AD7708 P1/P2 digital I/O pins. */
    /* Direction of AD7708 pin P1. */
    AD7708GpioDirection p1_direction = AD7708GpioDirection::Input;
    /* Direction of AD7708 pin P2. */
    AD7708GpioDirection p2_direction = AD7708GpioDirection::Input;
    /* Output value driven on P1 if P1 is configured as an output. */
    bool p1_output_value = false;
    /* Output value driven on P2 if P2 is configured as an output. */
    bool p2_output_value = false;

    /*
     * Optional user-provided calibration coefficients. The AD7708 has five
     * 16-bit offset registers and five 16-bit gain registers. They should only
     * be written when the ADC is inactive.
     */
    bool write_offset_calibration[5] = {};
    uint16_t offset_calibration[5] = {};
    bool write_gain_calibration[5] = {};
    uint16_t gain_calibration[5] = {};
};

/*
 * Small application-level driver for one AD7708.
 *
 * This is not a full Zephyr ADC subsystem driver. It talks to the chip directly
 * over SPI and returns raw 16-bit conversion codes.
 */
class AD7708
{
  private:
    /* Pulses the hardware reset pin and returns the chip to defaults. */
    int reset();
    /* Writes one 8-bit AD7708 register. */
    int write_reg(uint8_t reg, uint8_t value);
    /* Writes one 16-bit AD7708 register, such as offset/gain calibration. */
    int write_reg16(uint8_t reg, uint16_t value);
    /* Reads one 8-bit AD7708 register. */
    int read_reg(uint8_t reg, uint8_t *value);
    /* Reads one 16-bit AD7708 register, such as the conversion result. */
    int read_reg16(uint8_t reg, uint16_t *value);
    /* Waits until the AD7708 asserts its DRDY pin. */
    int wait_data_ready();
    /* Packs AD7708Settings into register values and writes them to the chip. */
    int write_settings_registers(const AD7708Settings &settings, AD7708Mode mode);

    /* Pointer to the board wiring/configuration for this AD7708 instance. */
    const AD7708Config *config_;
    /* Last settings successfully written to the chip. */
    AD7708Settings settings_;
    /* Tracks whether configure() or read_raw() has written settings yet. */
    bool configured_;

  public:
    /* Store the config pointer. Call init() before trying to use the ADC. */
    explicit AD7708(const AD7708Config *config);
    /* Check that SPI/GPIO devices are ready and reset the ADC. */
    int init();
    /* Apply ADC operating settings. */
    int configure(const AD7708Settings &settings);
    /* Read one raw 16-bit conversion from the requested channel. */
    int read_raw(uint8_t channel, int32_t *sample);

};
