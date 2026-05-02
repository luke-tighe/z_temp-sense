#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "hardware.h"

LOG_MODULE_REGISTER(main);

namespace {
/* The STM32 DAC uses 12-bit values from 0 to 4095. */
constexpr uint16_t kDacFullScaleCode = 4095;
/* This test assumes the DAC output supply is 3.3 V. */
constexpr int32_t kDacFullScaleMillivolts = 3300;
/* This is the target test voltage for DAC1 on PA4. */
constexpr int32_t kDac1TargetMillivolts = 1000;
/* This is the target test voltage for DAC2 on PA5. */
constexpr int32_t kDac2TargetMillivolts = 2000;

/* This turns a voltage in millivolts into a DAC code for a 3.3 V DAC output. */
uint16_t millivoltsToDacCode(int32_t millivolts)
{
    if (millivolts < 0)
    {
        millivolts = 0;
    }

    if (millivolts > kDacFullScaleMillivolts)
    {
        millivolts = kDacFullScaleMillivolts;
    }

    return static_cast<uint16_t>((millivolts * kDacFullScaleCode) / kDacFullScaleMillivolts);
}
} // namespace

int main(void)
{
    /* This object owns the board hardware used by the DAC test. */
    static Hardware hardware;

    /* This message shows that the firmware has started running. */
    LOG_INF("Starting fixed DAC output test");

    /* The program stops immediately if the needed hardware cannot be started. */
    if (hardware.init() != 0)
    {
        LOG_ERR("Hardware init failed");
        return -1;
    }

    /* This converts the desired 1 V output into the 12-bit code the DAC expects. */
    const uint16_t dac1_code = millivoltsToDacCode(kDac1TargetMillivolts);
    /* This converts the desired 2 V output into the 12-bit code the DAC expects. */
    const uint16_t dac2_code = millivoltsToDacCode(kDac2TargetMillivolts);

    /* This writes about 1 V to PA4. */
    if (hardware.setDAC1Value(dac1_code) != 0)
    {
        LOG_ERR("Failed to write DAC1");
        return -2;
    }

    /* This writes about 2 V to PA5. */
    if (hardware.setDAC2Value(dac2_code) != 0)
    {
        LOG_ERR("Failed to write DAC2");
        return -3;
    }

    /* This log line shows the voltages and raw DAC codes used for the test. */
    LOG_INF("DAC1 set to %d mV with code %u, DAC2 set to %d mV with code %u",
            static_cast<int>(kDac1TargetMillivolts),
            static_cast<unsigned int>(dac1_code),
            static_cast<int>(kDac2TargetMillivolts),
            static_cast<unsigned int>(dac2_code));

    while (1)
    {
        /* The loop just keeps the firmware alive so the analog outputs stay active. */
        k_sleep(K_MSEC(1000));
    }
}
