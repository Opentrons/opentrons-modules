/*
 * firmware-specific functions, data, and hooks for host comms control
 */
#include "firmware/freertos_comms_task.hpp"

#include <array>
#include <utility>
#include <functional>

#include "FreeRTOS.h"
#include "task.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#include "usbd_cdc.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#pragma GCC diagnostic pop

#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/tasks.hpp"

constexpr size_t MAX_USB_MESSAGE_SIZE_BYTES = 128;

struct CommsTaskFreeRTOS {
    USBD_CDC_ItfTypeDef cdc_class_fops;
    USBD_HandleTypeDef usb_handle;
    std::array<uint8_t, MAX_USB_MESSAGE_SIZE_BYTES> rx_buf;
    std::array<uint8_t, MAX_USB_MESSAGE_SIZE_BYTES> tx_buf;
};

static int8_t CDC_Init(void);
static int8_t CDC_DeInit(void);
static int8_t CDC_Control(uint8_t, uint8_t *, uint16_t);
static int8_t CDC_Receive(uint8_t*, uint32_t*);


CommsTaskFreeRTOS local_task = {
  .cdc_class_fops = {
     .Init = CDC_Init,
     .DeInit = CDC_DeInit,
     .Control = CDC_Control,
     .Receive = CDC_Receive,
   },
  .usb_handle = {},
  .rx_buf = {},
  .tx_buf = {}
};

USBD_CDC_LineCodingTypeDef linecoding =
{
  115200, /* baud rate*/
  0x00,   /* stop bits-1*/
  0x00,   /* parity - none*/
  0x08    /* nb. of bits 8*/
};




namespace host_comms_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static FreeRTOSMessageQueue<host_comms_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _comms_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                 "Comms Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _top_task = host_comms_task::HostCommsTask(_comms_queue);
static auto _local_task = local_task;
static auto _tasks = std::make_pair(&_top_task, &_local_task);

static constexpr uint32_t stack_size = 500;
// Stack as a std::array because why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, stack_size> stack;

// Internal FreeRTOS data structure
static StaticTask_t
    data;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Actual function that runs in the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    auto *task_pair = static_cast<decltype(_tasks) *>(param);
    auto *local_task = task_pair->second;
    USBD_Init(&local_task->usb_handle, &CDC_Desc, 0);
    USBD_RegisterClass(&local_task->usb_handle, USBD_CDC_CLASS);
    USBD_CDC_SetTxBuffer(
        &local_task->usb_handle, local_task->tx_buf.data(), local_task->tx_buf.size());
    USBD_CDC_SetRxBuffer(
        &local_task->usb_handle, local_task->rx_buf.data());
    USBD_CDC_RegisterInterface(&local_task->usb_handle, &local_task->cdc_class_fops);
    USBD_Start(&local_task->usb_handle);
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Function that creates and spins up the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   host_comms_task::HostCommsTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "HostCommsControl", stack.size(),
                                     &_tasks, 1, stack.data(), &data);
    _comms_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_top_task)>{.handle = handle,
                                                          .task = _top_task};
}
}  // namespace host_comms_control_task


static int8_t CDC_Init(void)
{
  /*
     Add your initialization code here
  */
  return (0);
}


static int8_t CDC_DeInit(void)
{
  /*
     Add your deinitialization code here
  */
  return (0);
}



static int8_t CDC_Control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
  switch (cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:
      /* Add your code here */
      break;

    case CDC_GET_ENCAPSULATED_RESPONSE:
      /* Add your code here */
      break;

    case CDC_SET_COMM_FEATURE:
      /* Add your code here */
      break;

    case CDC_GET_COMM_FEATURE:
      /* Add your code here */
      break;

    case CDC_CLEAR_COMM_FEATURE:
      /* Add your code here */
      break;

    case CDC_SET_LINE_CODING:
      linecoding.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | \
                                         (pbuf[2] << 16) | (pbuf[3] << 24));
      linecoding.format     = pbuf[4];
      linecoding.paritytype = pbuf[5];
      linecoding.datatype   = pbuf[6];

      /* Add your code here */
      break;

    case CDC_GET_LINE_CODING:
      pbuf[0] = (uint8_t)(linecoding.bitrate);
      pbuf[1] = (uint8_t)(linecoding.bitrate >> 8);
      pbuf[2] = (uint8_t)(linecoding.bitrate >> 16);
      pbuf[3] = (uint8_t)(linecoding.bitrate >> 24);
      pbuf[4] = linecoding.format;
      pbuf[5] = linecoding.paritytype;
      pbuf[6] = linecoding.datatype;

      /* Add your code here */
      break;

    case CDC_SET_CONTROL_LINE_STATE:
      /* Add your code here */
      break;

    case CDC_SEND_BREAK:
      /* Add your code here */
      break;

    default:
      break;
  }

  return (0);
}

static int8_t CDC_Receive(uint8_t *Buf, uint32_t *Len)
{
    std::copy(Buf, Buf + *Len, host_comms_control_task::_tasks.second->tx_buf.begin());
    return USBD_CDC_TransmitPacket(&host_comms_control_task::_tasks.second->usb_handle);
}
