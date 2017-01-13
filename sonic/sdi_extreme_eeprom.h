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


/**
 * Extreme Networks EXOS EEPROM format.
 */
#define SDI_STR_EXOS_PLATFORM_NAME "Summit"
#define SDI_STR_EXOS_VENDOR_NAME "Extreme Networks"

#define SDI_STR_EXTREME_EXOS_PSU_EEPROM "EXTREME_EXOS_PSU_EEPROM"
#define SDI_EXTREME_EXOS_PSU_OFFSET             0xd8
#define SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET    0
#define SDI_EXTREME_EXOS_PART_NUM_SIZE          10
#define SDI_EXTREME_EXOS_HW_REV_OFFSET          \
  (SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET +       \
   SDI_EXTREME_EXOS_PART_NUM_SIZE)
#define SDI_EXTREME_EXOS_PSU_HW_REV_OFFSET      \
  (SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET +       \
   SDI_EXTREME_EXOS_HW_REV_OFFSET)
#define SDI_EXTREME_EXOS_HW_REV_SIZE            2
#define SDI_EXTREME_EXOS_PART_REV_SIZE          \
  (SDI_EXTREME_EXOS_PART_NUM_SIZE +             \
   SDI_EXTREME_EXOS_HW_REV_SIZE)
#define SDI_EXTREME_EXOS_PSU_SERIAL_NUM_OFFSET  \
  (SDI_EXTREME_EXOS_PSU_HW_REV_OFFSET +         \
   SDI_EXTREME_EXOS_HW_REV_SIZE)
#define SDI_EXTREME_EXOS_SERIAL_NUM_SIZE        11
#define SDI_EXTREME_EXOS_PSU_DIRECTION_OFFSET   \
  (SDI_EXTREME_EXOS_PSU_SERIAL_NUM_OFFSET +     \
   SDI_EXTREME_EXOS_SERIAL_NUM_SIZE)
#define SDI_EXTREME_EXOS_DIRECTION_SIZE         1
#define SDI_EXTREME_EXOS_PSU_PAD_SIZE           15
#define SDI_EXTREME_EXOS_PSU_CHECKSUM_OFFSET    \
  (SDI_EXTREME_EXOS_PSU_DIRECTION_OFFSET +      \
   SDI_EXTREME_EXOS_DIRECTION_SIZE +            \
   SDI_EXTREME_EXOS_PSU_PAD_SIZE)
#define SDI_EXTREME_EXOS_CHECKSUM_SIZE          1

#define SDI_EXTREME_EXOS_PSU_CHECKSUM_FIRST     \
  SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET
#define SDI_EXTREME_EXOS_PSU_CHECKSUM_LAST      \
  (SDI_EXTREME_EXOS_PSU_CHECKSUM_OFFSET - 1)

#define SDI_EXTREME_EXOS_PSU_SIZE               \
  ((SDI_EXTREME_EXOS_PSU_CHECKSUM_OFFSET +      \
    SDI_EXTREME_EXOS_CHECKSUM_SIZE) -           \
    SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET)

#if (SDI_EXTREME_EXOS_PSU_SIZE != 0x28)
#error Incorrect SDI_EXTREME_EXOS_PSU_SIZE
#endif

#if (SDI_EXTREME_EXOS_PSU_CHECKSUM_OFFSET != \
     (SDI_EXTREME_EXOS_PSU_SIZE - \
      SDI_EXTREME_EXOS_CHECKSUM_SIZE))
#error Incorrect SDI_EXTREME_EXOS_PSU_CHECKSUM_OFFSET
#endif

#define SDI_EXTREME_EXOS_VENDOR_OFFSET          4
#define SDI_EXTREME_EXOS_PSU_VENDOR_OFFSET      \
  (SDI_EXTREME_EXOS_PSU_SERIAL_NUM_OFFSET +     \
   SDI_EXTREME_EXOS_VENDOR_OFFSET)
#define SDI_EXTREME_EXOS_VENDOR_SIZE 3

#define SDI_EXTREME_EXOS_F2B 0x01
#define SDI_EXTREME_EXOS_B2F 0x02

typedef struct {
  const char *partnum;
  const char *prod_name;
  const int num_fans;
  const int max_speed;
  const int power_rating;
  const int ac;
} sdi_exos_info_t;

typedef struct {
  const char *vendor_code;
  char *vendor_name;
} sdi_exos_vendor_t;

#define SDI_STR_EXTREME_EXOS_FAN_EEPROM "EXTREME_EXOS_FAN_EEPROM"

#define EXOS_TLV_TYPE_DEVICE_SIZE    0x8001
#define EXOS_TLV_TYPE_MFG_CHECKSUM   0x8002
#define EXOS_TLV_TYPE_EOI            0x8005
#define EXOS_TLV_TYPE_CLEI           0x8009
#define EXOS_TLV_TYPE_SOFTWARE_ID    0x800a
#define EXOS_TLV_TYPE_PCB_PART_NUM   0x800b
#define EXOS_TLV_TYPE_PCB_SERIAL_NUM 0x800c
#define EXOS_TLV_TYPE_PART_NUM       0x800d
#define EXOS_TLV_TYPE_SERIAL_NUM     0x800e
#define EXOS_TLV_TYPE_DIAG_VERSION   0x8037
#define EXOS_TLV_TYPE_DIAG_FAILED    0x803b
#define EXOS_TLV_TYPE_DIAG_RUN       0x8039
#define EXOS_TLV_TYPE_DIAG_PASS      0x803f
#define EXOS_TLV_TYPE_DIRECTION      0x8070

#define EXOS_TLV_VAL_FANTRAY         "FANTRAY"

#define MAX_TLV_VALUE_LEN 256
typedef struct sdi_exos_tlv_s {
  uint16_t type;
  uint16_t len;
  uint8_t value[MAX_TLV_VALUE_LEN];
} sdi_exos_tlv_t;
#define MIN_TLV_LEN (sizeof(sdi_exos_tlv_t) - MAX_TLV_VALUE_LEN)


t_std_error sdi_extreme_exos_psu_eeprom_data_get(void *resource_hdl,
                                                 sdi_entity_info_t *entity_info);
t_std_error sdi_extreme_exos_fan_eeprom_data_get(void *resource_hdl,
                                                 sdi_entity_info_t *entity_info);

#endif // _SDI_EXTREME_EEPROM_H_
