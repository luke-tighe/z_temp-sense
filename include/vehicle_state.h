// vehicle_state.h
#pragma once
#include <zephyr/kernel.h>
#include <zephyr/drivers/can.h>
#include <cstdint>

class VehicleState {
    public:
    // === Raw Sensor Data ===
    struct {
        struct {
            float channel0;
            float channel1;
            float channel2;
            float channel3;
            float channel4;
            float channel5;
            float channel6;
            float channel7;  
        }Voltage;

        struct 
        {
            float channel0;
            float channel1;
            float channel2;
            float channel3;
            float channel4;
            float channel5;
            float channel6;
            float channel7;   
        }Raw;
    } Analog;
    
    // === Processed Inputs ===
    struct {
        float throttle_percent;      // 0.0 - 1.0
        bool brake_pressed;
        bool rtd_button_pressed;
    } inputs;
    
    // === Motor Controller Data (from CAN) ===
    struct {
        float rpm;
        float torque_actual;
        float dc_bus_voltage;
        float motor_temp;
        uint8_t error_code;
        uint64_t last_rx_time;
    } motor;
    
    // === BMS Data (from CAN) ===
    struct {
        float pack_voltage;
        float pack_current;
        float soc_percent;
        float max_cell_temp;
        uint8_t fault_flags;
        uint64_t last_rx_time;
    } battery;
    
    // === Control Outputs ===
    struct {
        int16_t motor_torque_command;  // Nm * 10
        bool drive_enabled;
        bool brake_light_on;
    } commands;
    
    // === System State ===
    struct {
        enum State {
            INIT,
            IDLE,
            READY,
            DRIVING,
            ERROR
        } current;
        
        uint32_t error_flags;
        uint64_t uptime_ms;
    } system;
    
    // Helper functions
    bool motor_data_fresh() const {
        return (k_uptime_get() - motor.last_rx_time) < 100;  // 100ms timeout
    }
    
    bool battery_data_fresh() const {
        return (k_uptime_get() - battery.last_rx_time) < 500;  // 500ms timeout
    }
    
    // CAN decoding functions
    void decode_motor_status(const can_frame* frame);
    void decode_battery_status(const can_frame* frame);
};