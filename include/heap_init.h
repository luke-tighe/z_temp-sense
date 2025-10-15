#pragma once
#include "zephyr-common.h"
#include "zephyr/kernel.h"

/* heap_init encapsulates heap initialization, returns heap ptr */
k_heap* heap_init(void);
