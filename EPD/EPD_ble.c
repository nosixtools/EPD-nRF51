/* Copyright (c) 2012 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <string.h>
#include "nordic_common.h"
#include "ble_srv_common.h"
#include "nrf_log.h"
#include "EPD_4in2.h"
#include "EPD_4in2_V2.h"
#include "EPD_4in2b_V2.h"
#include "EPD_ble.h"

#define BLE_EPD_BASE_UUID                  {{0XEC, 0X5A, 0X67, 0X1C, 0XC1, 0XB6, 0X46, 0XFB, \
                                             0X8D, 0X91, 0X28, 0XD8, 0X22, 0X36, 0X75, 0X62}}
#define BLE_UUID_EPD_SERVICE               0x0001
#define BLE_UUID_EPD_CHARACTERISTIC        0x0002

#define ARRAY_SIZE(arr)                    (sizeof(arr) / sizeof((arr)[0]))

/** EPD drivers */
static epd_driver_t epd_drivers[] = {
    {EPD_DRIVER_4IN2, EPD_4IN2_Init, EPD_4IN2_Clear, 
     EPD_4IN2_SendCommand, EPD_4IN2_SendData,
     EPD_4IN2_UpdateDisplay, EPD_4IN2_Sleep},
    {EPD_DRIVER_4IN2_V2, EPD_4IN2_V2_Init, EPD_4IN2_V2_Clear,
     EPD_4IN2_V2_SendCommand, EPD_4IN2_V2_SendData,
     EPD_4IN2_V2_UpdateDisplay, EPD_4IN2_V2_Sleep},
    {EPD_DRIVER_4IN2B_V2, EPD_4IN2B_V2_Init, EPD_4IN2B_V2_Clear,
     EPD_4IN2B_V2_SendCommand, EPD_4IN2B_V2_SendData,
     EPD_4IN2B_V2_UpdateDisplay, EPD_4IN2B_V2_Sleep},
};

/**@brief Function for handling the @ref BLE_GAP_EVT_CONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_connect(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    p_epd->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
    DEV_Module_Init();
}


/**@brief Function for handling the @ref BLE_GAP_EVT_DISCONNECTED event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_disconnect(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    p_epd->conn_handle = BLE_CONN_HANDLE_INVALID;
    DEV_Module_Exit();
}

static void epd_service_process(ble_epd_t * p_epd, uint8_t * p_data, uint16_t length)
{
    if (p_data == NULL || length <= 0) return;
    NRF_LOG_PRINTF("[EPD]: CMD=0x%02x, LEN=%d\n", p_data[0], length);

    switch (p_data[0])
    {
      case EPD_CMD_SET_PINS:
          if (length < 8) return;
          NRF_LOG_PRINTF("[EPD]: MOSI=0x%02x SCLK=0x%02x CS=0x%02x DC=0x%02x RST=0x%02x BUSY=0x%02x BS=0x%02x\n",
                          p_data[1], p_data[2], p_data[3], p_data[4], p_data[5], p_data[6], p_data[7]);
          EPD_MOSI_PIN = p_data[1];
          EPD_SCLK_PIN = p_data[2];
          EPD_CS_PIN = p_data[3];
          EPD_DC_PIN = p_data[4];
          EPD_RST_PIN = p_data[5];
          EPD_BUSY_PIN = p_data[6];
          EPD_BS_PIN = p_data[7];
          DEV_Module_Exit();
          DEV_Module_Init();
          break;

      case EPD_CMD_INIT:
          if (length > 1)
          {
              for (uint8_t i = 0; i < ARRAY_SIZE(epd_drivers); i++)
              {
                  if (epd_drivers[i].id == p_data[1])
                  {
                      p_epd->driver = &epd_drivers[i];
                  }
              }
          }
          if (p_epd->driver == NULL) p_epd->driver= &epd_drivers[0];
          NRF_LOG_PRINTF("[EPD]: DRIVER=%d\n", p_epd->driver->id);
          p_epd->driver->init();
          break;

      case EPD_CMD_CLEAR:
          p_epd->driver->clear();
          break;

      case EPD_CMD_SEND_COMMAND:
          if (length < 2) return;
          p_epd->driver->send_command(p_data[1]);
          break;

      case EPD_CMD_SEND_DATA:
          for (UWORD i = 0; i < length - 1; i++)
          {
              p_epd->driver->send_data(p_data[i + 1]);
          }
          break;

      case EPD_CMD_DISPLAY:
          p_epd->driver->display();
          DEV_Delay_ms(500);
          break;

      case EPD_CMD_SLEEP:
          p_epd->driver->sleep();
          DEV_Delay_ms(200);
          break;

      default:
        break;
    }
}

/**@brief Function for handling the @ref BLE_GATTS_EVT_WRITE event from the S110 SoftDevice.
 *
 * @param[in] p_epd     EPD Service structure.
 * @param[in] p_ble_evt Pointer to the event received from BLE stack.
 */
static void on_write(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    ble_gatts_evt_write_t * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (
        (p_evt_write->handle == p_epd->char_handles.cccd_handle)
        &&
        (p_evt_write->len == 2)
       )
    {
        if (ble_srv_is_notification_enabled(p_evt_write->data))
        {
            p_epd->is_notification_enabled = true;
        }
        else
        {
            p_epd->is_notification_enabled = false;
        }
    }
    else if (p_evt_write->handle == p_epd->char_handles.value_handle)
    {
        epd_service_process(p_epd, p_evt_write->data, p_evt_write->len);
    }
    else
    {
        // Do Nothing. This event is not relevant for this service.
    }
}


void ble_epd_on_ble_evt(ble_epd_t * p_epd, ble_evt_t * p_ble_evt)
{
    if ((p_epd == NULL) || (p_ble_evt == NULL))
    {
        return;
    }

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            on_connect(p_epd, p_ble_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_disconnect(p_epd, p_ble_evt);
            break;

        case BLE_GATTS_EVT_WRITE:
            on_write(p_epd, p_ble_evt);
            break;

        default:
            // No implementation needed.
            break;
    }
}


static uint32_t epd_service_init(ble_epd_t * p_epd)
{
    ble_uuid128_t base_uuid = BLE_EPD_BASE_UUID;
    ble_uuid_t  ble_uuid;
    uint32_t    err_code;
 
    err_code = sd_ble_uuid_vs_add(&base_uuid, &ble_uuid.type);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    ble_uuid.uuid = BLE_UUID_EPD_SERVICE;
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_epd->service_handle);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    ble_gatts_char_md_t char_md;
    ble_gatts_attr_md_t cccd_md;
    ble_gatts_attr_t    attr_char_value;
    ble_uuid_t          char_uuid;
    ble_gatts_attr_md_t attr_md;

    memset(&cccd_md, 0, sizeof(cccd_md));
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);

    memset(&char_md, 0, sizeof(char_md));
    char_md.char_props.read   = 1;
    char_md.char_props.notify = 1;
    char_md.char_props.write  = 1;
    char_md.char_props.write_wo_resp  = 1;
    char_md.p_cccd_md         = &cccd_md;

    char_uuid.type = ble_uuid.type;
    char_uuid.uuid = BLE_UUID_EPD_CHARACTERISTIC;

    memset(&attr_md, 0, sizeof(attr_md));
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc    = BLE_GATTS_VLOC_STACK;

    memset(&attr_char_value, 0, sizeof(attr_char_value));
    attr_char_value.p_uuid    = &char_uuid;
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.init_len  = sizeof(uint8_t);
    attr_char_value.init_offs = 0;
    attr_char_value.max_len   = BLE_EPD_MAX_DATA_LEN;

    err_code = sd_ble_gatts_characteristic_add(p_epd->service_handle,
                                               &char_md,
                                               &attr_char_value,
                                               &p_epd->char_handles);
    return err_code;
}

uint32_t ble_epd_init(ble_epd_t * p_epd)
{
    if (p_epd == NULL)
    {
        return NRF_ERROR_NULL;
    }

    // Initialize the service structure.
    p_epd->conn_handle             = BLE_CONN_HANDLE_INVALID;
    p_epd->is_notification_enabled = false;

    // Add the service.
    return epd_service_init(p_epd);
}


uint32_t ble_epd_string_send(ble_epd_t * p_epd, uint8_t * p_string, uint16_t length)
{
    ble_gatts_hvx_params_t hvx_params;

    if (p_epd == NULL)
    {
        return NRF_ERROR_NULL;
    }

    if ((p_epd->conn_handle == BLE_CONN_HANDLE_INVALID) || (!p_epd->is_notification_enabled))
    {
        return NRF_ERROR_INVALID_STATE;
    }

    if (length > BLE_EPD_MAX_DATA_LEN)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    memset(&hvx_params, 0, sizeof(hvx_params));

    hvx_params.handle = p_epd->char_handles.value_handle;
    hvx_params.p_data = p_string;
    hvx_params.p_len  = &length;
    hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;

    return sd_ble_gatts_hvx(p_epd->conn_handle, &hvx_params);
}