#include "globals.h"
#include "zephyr-common.h"



LOG_MODULE_REGISTER(soc_health, LOG_LEVEL_INF); 

 constexpr int DIAG_THREAD_STACK_SIZE = 1024;
 constexpr int DIAG_THREAD_PRIORITY  = 5;


constexpr int health_task_period = 1000; //in ms 


void diagnostics_thread(void*,void*,void*){
        while(1){
            uint64_t uptime_ms = k_uptime_get(); 
            enum can_state state ; 
            can_get_state(can1, &state,NULL);
                struct sys_memory_stats memstats = {0,0,0}; 
                if(sys_heap == NULL){
                    return; 
                }
                sys_heap_runtime_stats_get((struct sys_heap*)&sys_heap->heap, &memstats);
                uint8_t load = cpu_load_get(1); 

                LOG_INF("Uptime: %llu ms | Heap: %zu/%zu bytes | CPU Load: %d.%d%% ",
                uptime_ms,
                memstats.allocated_bytes,
                memstats.allocated_bytes + memstats.free_bytes,
                static_cast<int>(load/10),
                load%10);

                switch (state)
                {
                    case CAN_STATE_ERROR_ACTIVE:
                        LOG_INF("CANBUS is In running condition"); 
                        break;
                    case CAN_STATE_ERROR_WARNING: 
                        LOG_INF("CANBUS is In running (warning) condition"); 
                        break; 
                    case CAN_STATE_ERROR_PASSIVE: 
                        LOG_INF("CANBUS is In Error (passive) condition"); 
                        break; 
                    case CAN_STATE_BUS_OFF:
                        LOG_INF("CANBUS is in OFF condition"); 
                    case CAN_STATE_STOPPED: 
                        LOG_INF("CANBUS is OFF ( CONTROLLER STOPPED)"); 
                        break; 
                    default:
                        LOG_INF("Failed to get CANBUS condition");
                        break;
                }


                k_msleep(health_task_period);

        }
}


void diagnostics_init(void)
{
    /* Locally scoped (function-static) persistent thread & stack */
    static struct k_thread diag_thread;
    static K_THREAD_STACK_DEFINE(diag_stack, DIAG_THREAD_STACK_SIZE);

    k_thread_create(&diag_thread,
                    diag_stack,
                    K_THREAD_STACK_SIZEOF(diag_stack),
                    diagnostics_thread,
                    NULL, NULL, NULL,
                    K_PRIO_PREEMPT(DIAG_THREAD_PRIORITY),
                    0, K_NO_WAIT);

    LOG_INF("Diagnostics thread started");
}