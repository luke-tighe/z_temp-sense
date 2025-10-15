#include "zephyr/kernel.h"
#include "zephyr/logging/log.h"
#define HEAP_SIZE 2048
K_HEAP_DEFINE(main_heap, HEAP_SIZE); 

LOG_MODULE_REGISTER(heap_init); 

k_heap* heap_init (void){

    k_timeout_t testAlloc = K_NO_WAIT;

    void *ptr = k_heap_alloc(&main_heap, sizeof(char) * 64, testAlloc);
    if(ptr == nullptr){
        LOG_ERR("heap initialization failed"); 
    }
    else{
        LOG_INF("Heap Allocation Succeeded"); 
    }

    k_heap_free(&main_heap,ptr); 

    return &main_heap; 
}

