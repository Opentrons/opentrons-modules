/**
  ******************************************************************************
  * @file    usbd_conf_template.c
  * @author  MCD Application Team
  * @brief   USB Device configuration and interface file
  *          This template should be copied to the user folder,
  *          renamed and customized following user needs.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                      www.st.com/SLA0044
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "stm32f3xx.h"
#include "stm32f3xx_ll_usb.h"
#include "FreeRTOS.h"
#include "task.h"

static uint8_t cdc_classhandle_backing_store[sizeof(USBD_CDC_HandleTypeDef)];

void *cdc_classhandle_malloc(size_t size) {
  return cdc_classhandle_backing_store;
}
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define EP_CNT 8
#define PMA_SIZE 1024
#define BUFS_PER_EP 2
#define PMA_CHUNK_SIZE (PMA_SIZE / (BUFS_PER_EP * EP_CNT))
/* Private macro -------------------------------------------------------------*/
// ep should be the endpoint number (from 0 - EP_CNT-1) and buf should be the buffer
// number (0 or 1 in the double-buffer case)
#define EP_BUF_OFFSET(ep_addr) (((ep_addr & 0x7f) << 1) | ((ep_addr & 0x80) >> 7))
#define PMA_ADDR_FOR_EP(ep, buf) (PMA_CHUNK_SIZE * (EP_BUF_OFFSET(ep)*BUFS_PER_EP+buf))

/* Private variables ---------------------------------------------------------*/

typedef struct {
  USB_TypeDef *usbx;
  USB_EPTypeDef eps[EP_CNT];
} USB_LL_Bindings_t;

static USB_LL_Bindings_t ll_bindings = {
   .usbx = USB,
   .eps = {}
};
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Initializes the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  // Bind in the hardware instance. There's only one so it's hardcoded.
  pdev->pData = (void*)&ll_bindings;

  // We don't have to wrap this to USB_CoreInit because it doesn't do anything on an
  // F3
  return USBD_OK;
}

/**
  * @brief  De-Initializes the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  return USBD_OK;
}

/**
  * @brief  Starts the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  USB_TypeDef *usbx = ((USB_LL_Bindings_t*)pdev->pData)->usbx;
  USB_CfgTypeDef cfg = {};
  USB_DevInit(usbx, cfg);
  return USB_EnableGlobalInt(usbx);
}

/**
  * @brief  Stops the Low Level portion of the Device driver.
  * @param  pdev: Device handle
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  USB_TypeDef *usbx = ((USB_LL_Bindings_t*)pdev->pData)->usbx;
  USB_DisableGlobalInt(usbx);
  return USB_StopDevice(usbx);
}

/**
  * @brief  Opens an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  ep_type: Endpoint Type
  * @param  ep_mps: Endpoint Max Packet Size
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  ep->num = ep_addr & 0x7f;
  ep->type = ep_type;
  ep->is_in = (ep_addr & 0x80) != 0;
  ep->pmaaddr0 = PMA_ADDR_FOR_EP(ep_addr, 0);
  ep->pmaaddr1 = PMA_ADDR_FOR_EP(ep_addr, 1);
  ep->pmaadress = ep->pmaaddr0;
  ep->is_stall = 0;
  ep->data_pid_start = 0;
  ep->doublebuffer = (BUFS_PER_EP != 1);
  ep->maxpacket = ep_mps;
  return USB_ActivateEndpoint(bindings->usbx, ep);
}

/**
  * @brief  Closes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  return USB_DeactivateEndpoint(bindings->usbx, ep);
}

/**
  * @brief  Flushes an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return USBD_OK;
}

/**
  * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  return USB_EPSetStall(bindings->usbx, ep);
}

/**
  * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev,
                                        uint8_t ep_addr)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  return USB_EPClearStall(bindings->usbx, ep);
}

/**
  * @brief  Returns Stall condition.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval Stall (1: Yes, 0: No)
  */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  return bindings->eps[EP_BUF_OFFSET(ep_addr)].is_stall;
}

/**
  * @brief  Assigns a USB address to the device.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev,
                                         uint8_t dev_addr)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  return USB_SetDevAddress(bindings->usbx, dev_addr);
}

/**
  * @brief  Transmits data over an endpoint.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf: Pointer to data to be sent
  * @param  size: Data size
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint16_t size)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  ep->xfer_buff = pbuf;
  ep->xfer_len = size;
  ep->xfer_count = 0;
  ep->xfer_len_db = size;
  ep->xfer_fill_db = 0;
  return USB_EPStartXfer(bindings->usbx, ep);
}

/**
  * @brief  Prepares an endpoint for reception.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @param  pbuf: Pointer to data to be received
  * @param  size: Data size
  * @retval USBD Status
  */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,
                                          uint8_t ep_addr, uint8_t *pbuf,
                                          uint16_t size)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  ep->xfer_buff = pbuf;
  ep->xfer_len = size;
  ep->xfer_count = 0;
  ep->xfer_len_db = size;
  ep->xfer_fill_db = 0;
  return USB_EPStartXfer(bindings->usbx, ep);
}

/**
  * @brief  Returns the last transferred packet size.
  * @param  pdev: Device handle
  * @param  ep_addr: Endpoint Number
  * @retval Recived Data Size
  */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  USB_LL_Bindings_t *bindings = (USB_LL_Bindings_t*)pdev->pData;
  USB_EPTypeDef *ep = &bindings->eps[EP_BUF_OFFSET(ep_addr)];
  return ep->xfer_len;
}

/**
  * @brief  Delays routine for the USB Device Library.
  * @param  Delay: Delay in ms
  * @retval None
  */
void USBD_LL_Delay(uint32_t Delay)
{
  vTaskDelay(Delay);
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

