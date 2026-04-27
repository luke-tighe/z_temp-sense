#pragma once

#include "hardware.h"
#include "threads/periodic_task.h"

class APPSTask : public PeriodicTask<APPSTask>
{
    friend class PeriodicTask<APPSTask>;

  public:
    void set_hardware(Hardware *hw)
    {
        hardware_ = hw;
    }

  private:
    Hardware *hardware_ = nullptr;
    int64_t agreement_fault_deadline_ = 0;

    void on_init();
    void on_deadline_miss();
    void run();

};

// Start the APPS task. Stack and task instance are owned inside APPS.cpp.
void start_apps_task(Hardware *hw, uint32_t period_ms = 100, int priority = 5);

APPSTask &get_apps_task();
