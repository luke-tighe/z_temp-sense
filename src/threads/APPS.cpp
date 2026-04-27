#include "threads/APPS.h"
#include "vehicle_state.h"
#include "zephyr/kernel.h"
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(APPS, LOG_LEVEL_INF);

K_THREAD_STACK_DEFINE(apps_stack, 1024);

static APPSTask apps_task_instance;

APPSTask &get_apps_task()
{
    return apps_task_instance;
}

void start_apps_task(Hardware *hw, uint32_t period_ms, int priority)
{
    apps_task_instance.set_hardware(hw);
    apps_task_instance.start(apps_stack, K_THREAD_STACK_SIZEOF(apps_stack), period_ms, priority, v, K_FP_REGS);
    LOG_INF("APPS task started (%u ms period)", period_ms);
}

void APPSTask::on_init()
{
    LOG_INF("APPS task initialized");
}

void APPSTask::on_deadline_miss()
{
    LOG_ERR("Deadline Missed in  APPS Task");
}

void APPSTask::run()
{
    APPS_data &apps = vehicle()->APPSIf;
   
    apps.faulted = range_fault || apps.errors[PEDAL_AGREEMENT] || apps.errors[BRAKE_OVERLAP];

    if (apps.faulted)
    {
        LOG_WRN("APPS fault active — torque command zeroed");
    }
    else
    {
        /*
         * The previous project sent inverter torque commands here using a
         * DTI-specific CAN message set. That product-specific transmit path
         * has been removed so APPS now stops at producing the normalized torque
         * request in vehicle state. New project-specific CAN command frames can
         * be added here later using hardware_->can1 or hardware_->can2.
         */
    }
}