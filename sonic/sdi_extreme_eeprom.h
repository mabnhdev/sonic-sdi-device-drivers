/*
 * Copyright (c) 2016 Extreme Networks Inc.
 */

/*
 * filename: sdi_extreme_eeprom.h
 */


/*******************************************************************
 * @file    sdi_extreme_eeprom.h
 * @brief   Declaration of EXTREME EEPROM related info.
 *
 *******************************************************************/
#ifndef _SDI_EXTREME_EEPROM_H_
#define _SDI_EXTREME_EEPROM_H_

#include "std_config_node.h"
#include "sdi_entity_info_internal.h"

/* Extreme_Delta PSU EEPROM Specific Fields */

#define SDI_STR_EXTREME_DELTA_PSU_EEPROM "EXTREME_DELTA_PSU_EEPROM"

#define SDI_EXTREME_DELTA_PSU_SERIAL_NUM "EXTREME_DELTA_PSU_SERIAL_NUM"
#define SDI_EXTREME_DELTA_PSU_PART_NUM   "EXTREME_DELTA_PSU_PART_NUM"
#define SDI_EXTREME_DELTA_PSU_LABEL_REV  "EXTREME_DELTA_PSU_LABEL_REV"
#define SDI_EXTREME_DELTA_PSU_VENDOR     "EXTREME_DELTA_PSU_VENDOR"

#define SDI_EXTREME_DELTA_PSU_SERIAL_NUM_OFFSET  0x23
#define SDI_EXTREME_DELTA_PSU_PART_NUM_OFFSET    0x12
#define SDI_EXTREME_DELTA_PSU_LABEL_REV_OFFSET   0x1f
#define SDI_EXTREME_DELTA_PSU_VENDOR_OFFSET      0xd8

#define SDI_EXTREME_DELTA_PSU_SERIAL_NUM_SIZE  14
#define SDI_EXTREME_DELTA_PSU_PART_NUM_SIZE    11
#define SDI_EXTREME_DELTA_PSU_LABEL_REV_SIZE    3
#define SDI_EXTREME_DELTA_PSU_VENDOR_SIZE      23

/**
 * Extreme_Delta PSU EEPROM data get
 *
 * param[in] resource_hdl  - resource handler
 * param[out] entity_info  - entity_info structure to fill
 *
 * return STD_ERR_OK for success and the respective error code in case of failure.
 */

t_std_error sdi_extreme_delta_psu_eeprom_data_get(void *resource_hdl,
                                                  sdi_entity_info_t *entity_info);

#endif // _SDI_EXTREME_EEPROM_H_
