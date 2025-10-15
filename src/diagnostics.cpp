#include "zephyr-common.h"



LOG_MODULE_REGISTER(soc_health, LOG_LEVEL_INF); 

#define DIAG_THREAD_STACK_SIZE 1024
#define DIAG_THREAD_PRIORITY   5


constexpr int health_task_period = 100; //in ms 


void diagnostics_thread(void*,void*,void*){
        while(1){
            uint64_t uptime_ms = k_uptime_get(); 

                struct sys_memory_stats memstats = {0,0,0}; 
                if(sys_heap == NULL){
                    return; 
                }
                sys_heap_runtime_stats_get((struct sys_heap*)&sys_heap->heap, &memstats);
                uint8_t load = cpu_load_get(1); 

                LOG_INF("Uptime: %llu ms | Heap: %zu/%zu bytes | CPU Load: %d",
                uptime_ms,
                memstats.allocated_bytes,
                memstats.allocated_bytes + memstats.free_bytes,
                load);


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