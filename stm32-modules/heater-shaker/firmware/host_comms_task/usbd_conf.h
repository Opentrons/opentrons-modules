/**
 ******************************************************************************
 * @file    usbd_conf_template.h
 * @author  MCD Application Team
 * @brief   Header file for the usbd_conf_template.c file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CONF_TEMPLATE_H
#define __USBD_CONF_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <string.h>  // for memcpy and memset

#include "stm32f3xx.h"
#include "stm32f3xx_hal_pcd.h"

/**
 * @brief This function is intended to be called BEFORE the USB
 * peripheral is initialized. It will set USB D+ to 0v to try to signal
 * that no device is connected.
 *
 * This is required due to the hard-wired pullup on the D+ line. When
 * the Heater Shaker powers on, a USB Host will expect a valid device
 * even though the USB isn't initialized. This function should signal
 * to the host that the device is restarted and prompt re-enumeration.
 *
 * @param delay_ms Milliseconds to delay before returning
 */
void usb_device_reset(uint32_t delay_ms);

extern void *cdc_classhandle_malloc(size_t size);
// extern void free_noop(void* _ignored) {(void)_ignored;}

/** @addtogroup STM32_USB_DEVICE_LIBRARY
 * @{
 */

/** @defgroup USBD_CONF
 * @brief USB device low level driver configuration file
 * @{
 */

/** @defgroup USBD_CONF_Exported_Defines
 * @{
 */

#define USBD_MAX_NUM_INTERFACES 1U
#define USBD_MAX_NUM_CONFIGURATION 1U
#define USBD_MAX_STR_DESC_SIZ 0x100U
#define USBD_SUPPORT_USER_STRING_DESC 0U
#define USBD_SELF_POWERED 1U
#define USBD_DEBUG_LEVEL 2U

/* CDC Class Config */
#define USBD_CDC_INTERVAL 2000U

/** @defgroup USBD_Exported_Macros
 * @{
 */

/* Memory management macros */
// malloc and free are used by some of the usbd class implementations; we're not
// using them, and therefore these should never be called, but defining them
// this way should give reasonably legible compiler failures if they are so we
// know where the problem is
#define USBD_malloc cdc_classhandle_malloc
#define USBD_free(_unused) ((void)(_unused))
#define USBD_memset memset
#define USBD_memcpy memcpy

/* DEBUG macros */
#define USBD_UsrLog(...) \
    do {                 \
    } while (0)

#define USBD_ErrLog(...) \
    do {                 \
    } while (0)

#define USBD_DbgLog(...) \
    do {                 \
    } while (0)

/**
 * @}
 */

/**
 * @}
 */

/** @defgroup USBD_CONF_Exported_Types
 * @{
 */
/**
 * @}
 */

/** @defgroup USBD_CONF_Exported_Macros
 * @{
 */
/**
 * @}
 */

/** @defgroup USBD_CONF_Exported_Variables
 * @{
 */
/**
 * @}
 */

/** @defgroup USBD_CONF_Exported_FunctionsPrototype
 * @{
 */
/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_TEMPLATE_H */

/**
 * @}
 */

/**
 * @}
 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
