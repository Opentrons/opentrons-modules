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
#include "stm32f3xx_hal.h"

#include "usbd_core.h"
#include "usbd_cdc.h"

#include "FreeRTOS.h"
#include "task.h"


static uint8_t cdc_classhandle_backing_store[sizeof(USBD_CDC_HandleTypeDef)];
static PCD_HandleTypeDef pcd_handle;

// All the HAL_PCD functions here are statically-defined callbacks from the HAL.
// The ones that don't do anything are present only as a reminder they exist.
//
// The structure is copied from the example project in the BSP,
// Projects/STM32303E_EVAL/Applications/USB_Device/DFU_Standalone because everything
// below the actual usb class  is identical.

void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
  GPIO_InitTypeDef GPIO_InitStruct;

  /* Enable the GPIOA clock for USB DataLines */
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* Enable the GPIOB clock for USB external Pull-Up */
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Configure USB DM and DP pins */
  GPIO_InitStruct.Pin = (GPIO_PIN_11 | GPIO_PIN_12);
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF14_USB;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Enable USB FS Clock */
  __HAL_RCC_USB_CLK_ENABLE();

  /* Enable SYSCFG Clock */
  __HAL_RCC_SYSCFG_CLK_ENABLE();

  /* Set USB Default FS Interrupt priority */
  HAL_NVIC_SetPriority(USB_LP_CAN_RX0_IRQn, 5, 0);

  /* Enable USB FS Interrupt */
  HAL_NVIC_EnableIRQ(USB_LP_CAN_RX0_IRQn);

}

void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SOF(hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_SetSpeed(hpcd->pData, USBD_SPEED_FULL);
  /* Reset Device */
  USBD_LL_Reset(hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
  USBD_LL_IsoINIncomplete(hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevConnected(hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
  USBD_LL_DevDisconnected(hpcd->pData);
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
  /* Disable USB FS Clock */
  __HAL_RCC_USB_CLK_DISABLE();

  /* Disable SYSCFG Clock */
  __HAL_RCC_SYSCFG_CLK_DISABLE();
}

void *cdc_classhandle_malloc(size_t size) {
  return cdc_classhandle_backing_store;
}


// All the USBD_LL functions are for linking the USBD middleware and the
// HAL. These have to be done with the PCD because the USBD middleware
// requires it in a couple places - that pData must be a pointer to
// PCD handle, the middleware uses a PCD call explicitly in a couple places,
// and so on.

USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
  // Bind in the hardware instance. There's only one so it's hardcoded.
  pcd_handle.Instance = USB;
  pcd_handle.Init.dev_endpoints = 8;
  pcd_handle.Init.phy_itface = PCD_PHY_EMBEDDED;
  pcd_handle.Init.speed = PCD_SPEED_FULL;

  pcd_handle.pData = pdev;
  pdev->pData = &pcd_handle;

  HAL_PCD_Init(pdev->pData);

  // Set up the static endpoint 0 pmas
  HAL_PCDEx_PMAConfig(pdev->pData, 0x00, PCD_SNG_BUF, 0x18);
  HAL_PCDEx_PMAConfig(pdev->pData, 0x80, PCD_SNG_BUF, 0x58);
  HAL_PCDEx_PMAConfig(pdev->pData , CDC_IN_EP , PCD_SNG_BUF, 0xC0);
  HAL_PCDEx_PMAConfig(pdev->pData , CDC_OUT_EP , PCD_SNG_BUF, 0x110);
  HAL_PCDEx_PMAConfig(pdev->pData , CDC_CMD_EP , PCD_SNG_BUF, 0x100);


  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_DeInit(pdev->pData);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_Start(pdev->pData);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
  HAL_PCD_Stop(pdev->pData);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
  HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_PCD_EP_Close(pdev->pData, ep_addr);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_PCD_EP_Flush(pdev->pData, ep_addr);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev,
                                        uint8_t ep_addr)
{
  HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);
  return USBD_OK;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  PCD_HandleTypeDef *hpcd = pdev->pData;

  if((ep_addr & 0x80) == 0x80)
  {
    return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
  }
  else
  {
    return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
  }
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev,
                                         uint8_t dev_addr)
{
  HAL_PCD_SetAddress(pdev->pData, dev_addr);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint16_t size)
{
  HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,
                                          uint8_t ep_addr, uint8_t *pbuf,
                                          uint16_t size)
{
  HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
  return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
  return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)
{
  vTaskDelay(Delay);
}

void USB_LP_CAN_RX0_IRQHandler(void) {
  HAL_PCD_IRQHandler(&pcd_handle);
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
