/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CanInitializer.h"
#include <stdio.h>
#include <sys/_intsup.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS 1000

#define DEBUGGING 0

// Placeholder values while we do not have read numbers (most likely same as 24 car)
const int ocThreshold = 1; // Open Circuit threshold
const int scThreshold = 2;  // Short Circuit threshold

// Class for the pedals contains how to read pedal positions and check Open Circuit and Short Circuit faults
struct pedalSensor {
    const int inputPin;
    const int zeroPos;
    const int fullPos;
    const int onLedPin;
    const int ocLedPin;
    const int scLedPin;

    public:
        float pedalPercentage() const {
            float percent = ((float) analogRead(inputPin) - zeroPos) / (fullPos - zeroPos);
            if (percent < 0.0f) {
                percent = 0.0f;
            } else if (percent > 1.0f) {
                percent = 1.0f;
            }
            return percent;
        }

        // Checks for a Short Circuit by reading the value of the sensor and comparing it to the threshold defined above
        bool checkOpenCircuit() const {
            bool isOpenCircuit = analogRead(inputPin) < ocThreshold;
            digitalWrite(ocLedPin, isOpenCircuit);
            #ifdef DEBUGGING
            printf("Pedal %d: Open Circuit %s\n", inputPin, isOpenCircuit ? "detected" : "not detected");
            #endif
            return isOpenCircuit;
        }

        // Checks for a Short Circuit by reading the value of the sensor and comparing it to the threshold defined above
        bool checkShortCircuit() const {
            bool isShortCircuit = analogRead(inputPin) > scThreshold;
            digitalWrite(scLedPin, isShortCircuit);
            #ifdef DEBUGGING
            printf("Pedal %d: Short Circuit %s\n", inputPin, isShortCircuit ? "detected" : "not detected");
            #endif
            return isShortCircuit;
        }
}

/* The devicetree node identifier for the "led0" alias. */

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */

int main(void) {}
