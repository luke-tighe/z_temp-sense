#include "can.h"

#include <errno.h>
#include <zephyr/logging/log.h>

#include "vehicle_state.h"
#include "zephyr/drivers/can.h"

LOG_MODULE_REGISTER(can);

// ============================================================================
// Construction
// ============================================================================

CanBus::CanBus(VehicleState *vehicle)
    : dev_(nullptr), bitrate_(0), sample_point_(0), initialized_(false), started_(false), vehicle_(vehicle),
      frames_rec(0), frames_sent(0)
{
}

// ============================================================================
// ISR dispatch
// ============================================================================

void CanBus::dispatch(const struct can_frame *frame)
{
    uint16_t id = frame->id & CAN_STD_ID_MASK;

    if (id >= 2048)
    {
        LOG_WRN("CAN frame ID 0x%03X exceeds table size", id);
        return;
    }

    if (bus_handlers[id])
    {
        bus_handlers[id](frame, vehicle_);
    }
    else
    {
        LOG_WRN_ONCE("No handler for CAN ID 0x%03X", id);
    }

    frames_rec++;
}

void CanBus::can1_rx_isr(const struct device *dev, struct can_frame *frame, void *self_ptr)
{
    CanBus *bus = static_cast<CanBus *>(self_ptr);
    bus->dispatch(frame);
}

void CanBus::can2_rx_isr(const struct device *dev, struct can_frame *frame, void *self_ptr)
{
    CanBus *bus = static_cast<CanBus *>(self_ptr);
    bus->dispatch(frame);
}

// ============================================================================
// Init
// ============================================================================

int CanBus::init(const struct device *dev, uint32_t bitrate, uint32_t sample_point)
{
    if (!dev)
    {
        LOG_ERR("CAN device is NULL");
        return -1;
    }

    if (!device_is_ready(dev))
    {
        LOG_ERR("CAN device not ready");
        return -2;
    }

    dev_ = dev;
    bitrate_ = bitrate;
    sample_point_ = sample_point;

    LOG_INF("CAN device ready - calculating timing...");

    // Calculate timing parameters
    struct can_timing timing;
    int ret = can_calc_timing(dev_, &timing, bitrate_, sample_point_);
    if (ret != 0)
    {
        LOG_ERR("can_calc_timing() failed: %d", ret);
        return ret;
    }

    // Stop CAN if already running
    enum can_state state;
    can_get_state(dev_, &state, nullptr);
    if (state != CAN_STATE_STOPPED)
    {
        LOG_INF("Stopping CAN to configure timing");
        ret = can_stop(dev_);
        if (ret != 0)
        {
            LOG_ERR("can_stop() failed: %d", ret);
            return ret;
        }
        started_ = false;
    }

    // Apply timing configuration
    LOG_INF("Applying timing configuration...");
    ret = can_set_timing(dev_, &timing);
    if (ret != 0)
    {
        LOG_ERR("can_set_timing() failed: %d", ret);
        return ret;
    }

    // Select ISR callback based on which CAN peripheral this is
    can_rx_callback_t callback;

    if (dev_ == DEVICE_DT_GET(DT_NODELABEL(fdcan1)))
    {
        callback = can1_rx_isr;
    }
    else if (dev_ == DEVICE_DT_GET(DT_NODELABEL(fdcan2)))
    {
        callback = can2_rx_isr;
    }
    else
    {
        LOG_ERR("Unknown CAN device during callback assignment");
        return -3;
    }

    constexpr struct can_filter accept_all_filter = {
        .id = 0x000,
        .mask = 0x00U,
        .flags = 0U,
    };

    int filter_id = can_add_rx_filter(dev_, callback, this, &accept_all_filter);
    if (filter_id < 0)
    {
        LOG_ERR("can_add_rx_filter() failed: %d", filter_id);
        return filter_id;
    }
    LOG_INF("Callback attached with code %d", filter_id);

    initialized_ = true;
    LOG_INF("CAN initialized (%d bps, sample point %d)", bitrate_, sample_point_);
    return 0;
}

// ============================================================================
// Start / Stop / Send
// ============================================================================

int CanBus::start()
{
    if (!initialized_)
    {
        LOG_ERR("CAN not initialized");
        return -1;
    }

    if (started_)
    {
        LOG_WRN("CAN already started");
        return 0;
    }

    LOG_INF("Starting CAN...");
    int ret = can_start(dev_);
    if (ret != 0)
    {
        LOG_ERR("can_start() failed: %d", ret);
        return ret;
    }

    started_ = true;
    LOG_INF("CAN started successfully");
    return 0;
}

int CanBus::stop()
{
    if (!initialized_)
    {
        return -1;
    }

    if (!started_)
    {
        return 0;
    }

    int ret = can_stop(dev_);
    if (ret == 0)
    {
        started_ = false;
        LOG_INF("CAN stopped");
    }
    return ret;
}

int CanBus::send(const struct can_frame *frame, k_timeout_t timeout, can_tx_callback_t callback, void *user_data)
{
    if (!initialized_ || !started_)
    {
        return -1;
    }

    if (frame == nullptr)
    {
        return -EINVAL;
    }

    const bool is_fd_frame = (frame->flags & CAN_FRAME_FDF) != 0U;
    const uint8_t max_dlc = is_fd_frame ? CANFD_MAX_DLC : CAN_MAX_DLC;
    if (frame->dlc > max_dlc)
    {
        return -EINVAL;
    }

    const bool is_extended_id = (frame->flags & CAN_FRAME_IDE) != 0U;
    if (!is_extended_id && frame->id > CAN_STD_ID_MASK)
    {
        return -EINVAL;
    }

    if (is_extended_id && frame->id > CAN_EXT_ID_MASK)
    {
        return -EINVAL;
    }

    if ((frame->flags & (CAN_FRAME_BRS | CAN_FRAME_ESI)) != 0U && !is_fd_frame)
    {
        return -EINVAL;
    }

    return can_send(dev_, frame, timeout, callback, user_data);
}

// ============================================================================
// Filter / State / Mode
// ============================================================================

int CanBus::register_handler(uint16_t standard_id, frame_handler_t handler)
{
    if (standard_id >= 2048)
    {
        return -EINVAL;
    }

    bus_handlers[standard_id] = handler;
    return 0;
}

int CanBus::add_rx_filter_msgq(struct k_msgq *msgq, const struct can_filter *filter)
{
    if (!initialized_)
    {
        return -1;
    }

    return can_add_rx_filter_msgq(dev_, msgq, filter);
}

void CanBus::remove_rx_filter(int filter_id)
{
    if (!initialized_)
    {
        return;
    }

    can_remove_rx_filter(dev_, filter_id);
}

int CanBus::get_state(enum can_state *state) const
{
    if (!initialized_ || !state)
    {
        return -1;
    }

    return can_get_state(dev_, state, nullptr);
}

int CanBus::set_mode(can_mode_t mode)
{
    if (!initialized_)
    {
        return -1;
    }

    bool was_started = started_;
    if (started_)
    {
        int ret = stop();
        if (ret != 0)
        {
            LOG_ERR("Failed to stop CAN before mode change");
            return ret;
        }
    }

    int ret = can_set_mode(dev_, mode);
    if (ret != 0)
    {
        LOG_ERR("can_set_mode failed: %d", ret);
        return ret;
    }

    if (was_started)
    {
        ret = start();
        if (ret != 0)
        {
            LOG_ERR("Failed to restart CAN after mode change");
            return ret;
        }
    }

    LOG_INF("CAN mode set to 0x%x", mode);
    return 0;
}

can_mode_t CanBus::get_mode() const
{
    if (!initialized_)
    {
        return (can_mode_t)0;
    }

    return can_get_mode(dev_);
}
