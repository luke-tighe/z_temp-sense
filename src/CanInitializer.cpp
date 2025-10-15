#include "zephyr-common.h"

LOG_MODULE_REGISTER(canInitializer, LOG_LEVEL_INF);


namespace {
    constexpr int CAN1_BAUD = 1000000;         
    constexpr int CAN1_SAMPLE_POINT = 875;       
    constexpr uint16_t CAN1_STATUS_MSG_ID = 0x090; 
}



static void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data) {
    LOG_INF("Received CAN frame ID=0x%x DLC=%d", frame->id, frame->dlc);
}

static void can_status_callback(const struct device *dev, int error, void *user_data) {
    if (error) {
        LOG_ERR("CAN Status MSG Transmit failed: %d", error);
    } else {
        LOG_INF("CAN Status MSG Transmit succeeded");
    }
}
uint8_t can_init() {
    const struct device *can1 = DEVICE_DT_GET(DT_NODELABEL(fdcan1));

    if (!device_is_ready(can1)) {
        LOG_ERR("CAN1 controller not ready");
        return static_cast<uint8_t>(-1);
    }

    LOG_INF("CAN1 device ready — calculating timing...");
    struct can_timing timing {};
    int ret = can_calc_timing(can1, &timing, CAN1_BAUD, CAN1_SAMPLE_POINT);
    if (ret != 0) {
        LOG_ERR("can_calc_timing() failed with code %d", ret);
        return static_cast<uint8_t>(ret);
    }

    struct can_bus_err_cnt can1err; 
    enum can_state state;
    can_get_state(can1, &state,&can1err);
    if (state != CAN_STATE_STOPPED) {
        LOG_INF("Stopping CAN1 to Configure Timing!");
        can_stop(can1);
        if (ret != 0) {
        LOG_ERR("can_stop() failed with code %d", ret);
        return static_cast<uint8_t>(ret);
    }
    }

    LOG_INF("Applying timing configuration...");
    ret = can_set_timing(can1, &timing);
    if (ret != 0) {
        LOG_ERR("can_set_timing() failed with code %d", ret);
        return static_cast<uint8_t>(ret);
    }

    LOG_INF("Starting CAN1...");
    ret = can_start(can1);
    if (ret != 0) {
        LOG_ERR("can_start() failed with code %d", ret);
        return static_cast<uint8_t>(ret);
    }

    LOG_INF("Configuring default RX filter...");
    const struct can_filter filter = {
        .id = 0x000,
        .mask = 0x000,
        .flags = 0
    };

    int filter_id = can_add_rx_filter(can1, can_rx_callback, nullptr, &filter);
    if (filter_id < 0) {
        LOG_ERR("Failed to add RX filter: %d", filter_id);
    } else {
        LOG_INF("RX filter installed successfully (id=%d)", filter_id);
    }

    struct can_frame msg_can1_status{
        .id = CAN1_STATUS_MSG_ID, 
        .dlc = can_bytes_to_dlc(8), 
        .flags = 0, 
        .data = {0,1,2,3,4,5,6,7}
    }; 

    can_send(can1,&msg_can1_status,K_MSEC(10),can_status_callback,nullptr); 

    LOG_INF("CAN1 initialization complete.");
    return 0;


}
