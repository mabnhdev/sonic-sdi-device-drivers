/*
 * Copyright (c) 2016 Extreme Networks Inc.
 */

/*
 * filename: sdi_extreme_eeprom.c
 */


/******************************************************************************
 *  Read the EXTREME LEGACY eeprom contents like manufacturing, software info blocks
 *  and related function implementations.
 ******************************************************************************/

#include "sdi_device_common.h"
#include "sdi_entity_info.h"
#include "sdi_eeprom.h"
#include "sdi_extreme_eeprom.h"
#include "sdi_i2c_bus_api.h"
#include "sdi_entity_info.h"
#include "std_assert.h"
#include "std_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct {
    char name[SDI_MAX_NAME_LEN];
    uint32_t offset;
    uint16_t size;
}sdi_eeprom_info;

/* Extreme/Delta PSU EEPROM Specific Fields */
static sdi_eeprom_info sdi_extreme_delta_psu_eeprom_info[] = {
     /* Name, Offset, Size */
    {SDI_EXTREME_DELTA_PSU_SERIAL_NUM, SDI_EXTREME_DELTA_PSU_SERIAL_NUM_OFFSET,
     SDI_EXTREME_DELTA_PSU_SERIAL_NUM_SIZE},
    {SDI_EXTREME_DELTA_PSU_PART_NUM, SDI_EXTREME_DELTA_PSU_PART_NUM_OFFSET,
     SDI_EXTREME_DELTA_PSU_PART_NUM_SIZE},
    {SDI_EXTREME_DELTA_PSU_LABEL_REV, SDI_EXTREME_DELTA_PSU_LABEL_REV_OFFSET,
     SDI_EXTREME_DELTA_PSU_LABEL_REV_SIZE},
    {SDI_EXTREME_DELTA_PSU_VENDOR, SDI_EXTREME_DELTA_PSU_VENDOR_OFFSET,
     SDI_EXTREME_DELTA_PSU_VENDOR_SIZE},
};

/**
 * Fill Extreme/Delta PSU EEPROM info into the corresponding entity_info members
 *
 * param[in] parser_type   - parser format
 * param[out] entity_data  - entity_info structure to fill
 *
 * return STD_ERR_OK for success and the respective error code in case of failure.
 */
static t_std_error sdi_extreme_delta_psu_eeprom_info_fill(sdi_device_hdl_t chip,
        sdi_entity_parser_t parser_type,sdi_entity_info_t *entity_data)
{
    uint8_t offset_index = 0;
    uint8_t total_offsets = 0;
    uint8_t data[SDI_MAX_NAME_LEN];
    char data_buff[SDI_MAX_NAME_LEN];
    uint16_t size = 0;
    t_std_error rc = STD_ERR_OK;
    sdi_eeprom_info *sdi_extreme_delta_eeprom_info = NULL;
    entity_info_device_t *eeprom_data = NULL;

    eeprom_data = (entity_info_device_t *)chip->private_data;

    /*
     * Extreme EEPROM format does not support no.of fans and maximum speed of the
     * fan. But Entity-Info expect it.Hence return the values that were obtained from
     * configuration file
     */
    entity_data->num_fans = eeprom_data->no_of_fans;
    entity_data->max_speed = eeprom_data->max_fan_speed;

    memset(data, 0, sizeof(data));
    memset(data_buff, 0, sizeof(data_buff));
    if (parser_type == SDI_EXTREME_DELTA_PSU_EEPROM) {
        size = sizeof(sdi_extreme_delta_psu_eeprom_info);
        sdi_extreme_delta_eeprom_info = &sdi_extreme_delta_psu_eeprom_info[0];
        total_offsets = (size / sizeof(sdi_extreme_delta_psu_eeprom_info[0]));
    }

    while (offset_index < total_offsets) {
        size = sdi_extreme_delta_eeprom_info->size;
        rc = sdi_smbus_read_multi_byte(chip->bus_hdl, chip->addr.i2c_addr,
                sdi_extreme_delta_eeprom_info->offset,
                data, size, SDI_I2C_FLAG_NONE);
        if (rc != STD_ERR_OK) {
            SDI_DEVICE_ERRMSG_LOG("SMBUS Read failed:  %d\n", rc);
            return rc;
        }

        if(sdi_extreme_delta_eeprom_info->offset == SDI_EXTREME_DELTA_PSU_SERIAL_NUM_OFFSET){
            safestrncpy(entity_data->ppid, (const char *)(data), size+1);
        }else if(sdi_extreme_delta_eeprom_info->offset == SDI_EXTREME_DELTA_PSU_PART_NUM_OFFSET){
            safestrncpy(entity_data->prod_name, (const char *)(data), size+1);
        }else if(sdi_extreme_delta_eeprom_info->offset == SDI_EXTREME_DELTA_PSU_LABEL_REV_OFFSET){
            safestrncpy(entity_data->hw_revision, (const char *)(data), size+1);
        }else if(sdi_extreme_delta_eeprom_info->offset == SDI_EXTREME_DELTA_PSU_VENDOR_OFFSET){
            safestrncpy(entity_data->platform_name, (const char *)(data), size+1);
        }else{/* do nothing */}

        sdi_extreme_delta_eeprom_info++;
        offset_index++;
    }

    return rc;
}

/**
 * Fill entity_info members for Extreme/Delta PSU.
 *
 * param[in] parser_type   - parser format
 * param[out] entity_data  - entity_info structure to fill
 *
 * return STD_ERR_OK for success and the respective error code in case of failure.
 */
static t_std_error sdi_extreme_delta_psu_eeprom_entity_info_fill(sdi_device_hdl_t chip,
                   sdi_entity_parser_t parser_type, sdi_entity_info_t *entity_data)
{
    return(sdi_extreme_delta_psu_eeprom_info_fill(chip, parser_type, entity_data));
}

/**
 * Read the EXTREME/DELTA EEPROM data and fill the entity_info.
 *
 * param[in] resource_hdl  - resource handler
 * param[in] format        - parser format
 * param[out] entity_info  - entity_info structure to fill
 *
 * return STD_ERR_OK for success and the respective error code in case of failure.
 */
t_std_error sdi_extreme_delta_psu_eeprom_data_get(void *resource_hdl,
                                                  sdi_entity_info_t *entity_info)
{
    sdi_device_hdl_t chip = NULL;

    /** Validate arguments */
    chip = (sdi_device_hdl_t)resource_hdl;
    STD_ASSERT(chip != NULL);
    STD_ASSERT(entity_info != NULL);

    return sdi_extreme_delta_psu_eeprom_entity_info_fill(chip,
                       SDI_EXTREME_DELTA_PSU_EEPROM, entity_info);
}
