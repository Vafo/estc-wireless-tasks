/**
 * Copyright 2022 Evgeniy Morozov
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE
*/

#include "estc_service.h"

#include "app_error.h"
#include "nrf_log.h"

#include "ble.h"
#include "ble_gatts.h"
#include "ble_srv_common.h"

ble_uuid128_t base_uuid = {
    .uuid128 = ESTC_BASE_UUID
};

uint8_t m_char_user_desc[] = "Custom Characteristic";

uint8_t m_char_hello_val[] = "Hello";
uint8_t m_char_hello_val_reversed[] = "olleH";

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service);
static ret_code_t estc_ble_check_user_need_for_hvx(uint16_t conn_handle, uint16_t cccd_handle, uint16_t hvx_type);

ret_code_t estc_ble_service_init(ble_estc_service_t *service)
{
    VERIFY_PARAM_NOT_NULL(service);
    ret_code_t error_code = NRF_SUCCESS;
    ble_uuid_t service_uuid = {
        .uuid = ESTC_SERVICE_UUID
    };

    // TODO: 4. Add service UUIDs to the BLE stack table using `sd_ble_uuid_vs_add`
    error_code = sd_ble_uuid_vs_add(&base_uuid, &service_uuid.type);
    APP_ERROR_CHECK(error_code);

    // TODO: 5. Add service to the BLE stack using `sd_ble_gatts_service_add`
    error_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service_uuid, &service->service_handle);
    APP_ERROR_CHECK(error_code);

    NRF_LOG_DEBUG("%s:%d | Service UUID: 0x%04x", __FUNCTION__, __LINE__, service_uuid.uuid);
    NRF_LOG_DEBUG("%s:%d | Service UUID type: 0x%02x", __FUNCTION__, __LINE__, service_uuid.type);
    NRF_LOG_DEBUG("%s:%d | Service handle: 0x%04x", __FUNCTION__, __LINE__, service->service_handle);

    service->connection_handle = BLE_CONN_HANDLE_INVALID;

    return estc_ble_add_characteristics(service);
}

void estc_ble_service_on_ble_event(const ble_evt_t *p_ble_evt, void *ctx)
{
    // ret_code_t err_code = NRF_SUCCESS;
    ble_estc_service_t *p_estc_service = (ble_estc_service_t *) ctx;
    uint16_t m_conn_handle;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_DISCONNECTED:
            p_estc_service->connection_handle = BLE_CONN_HANDLE_INVALID;
            break;

        case BLE_GAP_EVT_CONNECTED:
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            p_estc_service->connection_handle = m_conn_handle;
            p_estc_service->hvn_available_queue_element_count = ESTC_SERVICE_HVN_QUEUE_SIZE;
            break;

        case BLE_GATTS_EVT_HVN_TX_COMPLETE:
            m_conn_handle = p_ble_evt->evt.gatts_evt.conn_handle;
            if(m_conn_handle == p_estc_service->connection_handle)
            {
                p_estc_service->hvn_available_queue_element_count += \
                            p_ble_evt->evt.gatts_evt.params.hvn_tx_complete.count;
            }
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            
            break;

        
        default:
            // No implementation needed.
            break;
    }
}

static ret_code_t estc_ble_add_characteristics(ble_estc_service_t *service)
{
    VERIFY_PARAM_NOT_NULL(service);
    ret_code_t error_code = NRF_SUCCESS;
    ble_uuid_t char_uuid = {
        .uuid = ESTC_GATT_CHAR_1_UUID
    };

    // TODO: 6.1. Add custom characteristic UUID using `sd_ble_uuid_vs_add`, same as in step 4
    error_code = sd_ble_uuid_vs_add(&base_uuid, &char_uuid.type);
    APP_ERROR_CHECK(error_code);

    // TODO: 6.5. Configure Characteristic metadata (enable read and write)
    ble_gatts_char_md_t char_md = { 0 };
    char_md.char_props.read = 1;
    char_md.char_props.write = 1;
    
    // Add User Description Descriptor
    char_md.p_char_user_desc = m_char_user_desc;
    char_md.char_user_desc_size = sizeof(m_char_user_desc) / sizeof(m_char_user_desc[0]);
    char_md.char_user_desc_max_size = sizeof(m_char_user_desc) / sizeof(m_char_user_desc[0]);
    char_md.p_user_desc_md = NULL;  // Default md values of user descr attr 

    // Configures attribute metadata. For now we only specify that the attribute will be stored in the softdevice
    ble_gatts_attr_md_t attr_md = { 0 };
    attr_md.vloc = BLE_GATTS_VLOC_STACK;


    // TODO: 6.6. Set read/write security levels to our attribute metadata using `BLE_GAP_CONN_SEC_MODE_SET_OPEN`
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);

    // TODO: 6.2. Configure the characteristic value attribute (set the UUID and metadata)
    ble_gatts_attr_t attr_char_value = { 0 };
    attr_char_value.p_attr_md = &attr_md;
    attr_char_value.p_uuid = &char_uuid;

    // TODO: 6.7. Set characteristic length in number of bytes in attr_char_value structure
    attr_char_value.init_len = sizeof(uint16_t);
    attr_char_value.max_len = sizeof(uint16_t);

    // TODO: 6.4. Add new characteristic to the service using `sd_ble_gatts_characteristic_add`
    error_code = sd_ble_gatts_characteristic_add(service->service_handle, &char_md, &attr_char_value, &service->char_1);
    APP_ERROR_CHECK(error_code);

    // Hello Characteristic
    ble_uuid_t char_hello_uuid = {
        .uuid = ESTC_GATT_CHAR_HELLO_UUID
    };

    error_code = sd_ble_uuid_vs_add(&base_uuid, &char_hello_uuid.type);
    APP_ERROR_CHECK(error_code);

    ble_gatts_attr_md_t char_hello_cccd_md = {
        .vloc = BLE_GATTS_VLOC_STACK
    };
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_hello_cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_hello_cccd_md.write_perm);

    ble_gatts_char_pf_t char_hello_pf = {
        .format = BLE_GATT_CPF_FORMAT_UTF8S,
    };

    ble_gatts_char_md_t char_hello_md = {0};
    char_hello_md.char_props.read = 1;
    char_hello_md.char_props.notify = 1;
    char_hello_md.p_char_pf = &char_hello_pf;
    char_hello_md.p_cccd_md = &char_hello_cccd_md;
    
    ble_gatts_attr_md_t char_hello_value_md = {0};
    char_hello_value_md.vloc = BLE_GATTS_VLOC_STACK;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&char_hello_value_md.read_perm);

    ble_gatts_attr_t char_hello_value = {0};
    char_hello_value.p_attr_md = &char_hello_value_md;
    char_hello_value.p_uuid = &char_hello_uuid;
    char_hello_value.p_value = m_char_hello_val;
    char_hello_value.init_len = sizeof(m_char_hello_val) / sizeof(m_char_hello_val[0]);
    char_hello_value.max_len = sizeof(m_char_hello_val) / sizeof(m_char_hello_val[0]);
    
    error_code = sd_ble_gatts_characteristic_add(service->service_handle, &char_hello_md, &char_hello_value, &service->char_hello);
    APP_ERROR_CHECK(error_code);

    // BTN_STATE Charecteristic

    ble_uuid_t btn_st_uuid = {
        .uuid = ESTC_GATT_CHAR_BTN_STATE_UUID
    };

    error_code = sd_ble_uuid_vs_add(&base_uuid, &btn_st_uuid.type);
    APP_ERROR_CHECK(error_code);

    ble_gatts_char_md_t btn_st_char_md = {0};

    ble_gatts_attr_md_t btn_st_cccd_md = {
        .vloc = BLE_GATTS_VLOC_STACK
    };

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&btn_st_cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&btn_st_cccd_md.write_perm);

    ble_gatts_char_pf_t btn_st_char_pf = {
        .format = BLE_GATT_CPF_FORMAT_BOOLEAN
    };

    btn_st_char_md.char_props.read = 1;
    btn_st_char_md.char_props.indicate = 1;
    btn_st_char_md.p_char_pf = &btn_st_char_pf;
    btn_st_char_md.p_cccd_md = &btn_st_cccd_md;


    ble_gatts_attr_t btn_st_value = {0};

    ble_gatts_attr_md_t btn_st_value_md = {
        .vloc = BLE_GATTS_VLOC_STACK
    };

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&btn_st_value_md.read_perm);

    uint8_t btn_st_init_val = 0;

    btn_st_value.p_attr_md = &btn_st_value_md;
    btn_st_value.p_uuid = &btn_st_uuid;
    btn_st_value.init_len = sizeof(uint8_t);
    btn_st_value.max_len = sizeof(uint8_t);
    btn_st_value.p_value = &btn_st_init_val;

    error_code = sd_ble_gatts_characteristic_add(service->service_handle, &btn_st_char_md, &btn_st_value, &service->char_btn_state);
    APP_ERROR_CHECK(error_code);

    return NRF_SUCCESS;
}

ret_code_t estc_ble_service_hello_update(ble_estc_service_t *service)
{
    static uint8_t inverter = 0;
    ret_code_t error_code = NRF_SUCCESS;

    uint16_t val_len = inverter ? sizeof(m_char_hello_val_reversed) / sizeof(m_char_hello_val_reversed[0]) : \
                                  sizeof(m_char_hello_val) / sizeof(m_char_hello_val[0]);
    uint8_t *val = inverter ? m_char_hello_val_reversed : m_char_hello_val;

    ble_gatts_value_t new_val = {
        .p_value = val,
        .len = val_len,
        .offset = 0
    };

    error_code = sd_ble_gatts_value_set(service->connection_handle, service->char_hello.value_handle, &new_val);
    APP_ERROR_CHECK(error_code);
    
    error_code = estc_ble_service_hello_notify(service);
    
    if(error_code == NRF_SUCCESS)
    {
        NRF_LOG_INFO("Notified with val %s, val_len = %d", val, val_len);
    }

    inverter ^= 1;

    return NRF_SUCCESS;
}

// Check for events
ret_code_t estc_ble_service_hello_notify(ble_estc_service_t *service)
{
    NRF_LOG_INFO("Trying to notify ...");
    if(service->connection_handle == BLE_CONN_HANDLE_INVALID)
    {
        NRF_LOG_INFO("... connection handle is invalid");
        return BLE_ERROR_INVALID_CONN_HANDLE;
    }
    ret_code_t error_code = NRF_SUCCESS;

    error_code = estc_ble_check_user_need_for_hvx(service->connection_handle, service->char_hello.cccd_handle, BLE_GATT_HVX_NOTIFICATION);
    if(error_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("... attempted to notify client, who doesn't need it");
        return NRF_ERROR_FORBIDDEN;
    }

    ble_gatts_hvx_params_t hvx_params = {
        .handle = service->char_hello.value_handle,
        .type = BLE_GATT_HVX_NOTIFICATION,
        .offset = 0,
        .p_data = NULL,
        .p_len = NULL   // Risky move ?
    };
    
    if(service->hvn_available_queue_element_count == 0)
    {
        NRF_LOG_INFO("No space left in hvn queue");
        return NRF_ERROR_NO_MEM;
    }
    NRF_LOG_INFO("There is space in hvn queue");

    error_code = sd_ble_gatts_hvx(service->connection_handle, &hvx_params);
    NRF_LOG_INFO("Retval of sd_ble_gatts_hvx : %x", error_code);
    VERIFY_SUCCESS(error_code);
    service->hvn_available_queue_element_count--;

    return NRF_SUCCESS;
}

static ret_code_t estc_ble_check_user_need_for_hvx(uint16_t conn_handle, uint16_t cccd_handle, uint16_t hvx_type)
{
    uint16_t m_cccd_value;
    ret_code_t error_code;
    
    ble_gatts_value_t cccd_value_param = {
        .len = sizeof(m_cccd_value),
        .p_value = (uint8_t *) &m_cccd_value
    };

    error_code = sd_ble_gatts_value_get( conn_handle, 
                                         cccd_handle, 
                                         &cccd_value_param);
    
    if(error_code != NRF_SUCCESS)
    {
        NRF_LOG_INFO("sd_ble_gatts_value_get retval %x", error_code);
    }
    
    // BLE_ERROR_GATTS_SYS_ATTR_MISSING is caused, whenever user haven't accessed CCCD
    VERIFY_SUCCESS(error_code);
    
    // Debug info
    if(cccd_value_param.len != sizeof(m_cccd_value))
    {
        NRF_LOG_INFO("... CCCD is more than %d bytes, it is %d", sizeof(m_cccd_value), cccd_value_param.len);
        return NRF_ERROR_INVALID_STATE;
    }
    
    // Maybe there is better way around to mitigate notifying users, who don't need it
    return m_cccd_value == hvx_type ? NRF_SUCCESS : NRF_ERROR_FORBIDDEN;
}