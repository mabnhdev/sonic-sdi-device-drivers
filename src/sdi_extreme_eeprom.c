/*
 * Copyright (c) 2016 Extreme Networks Inc.
 */

/*
 * filename: sdi_extreme_eeprom.c
 */


/******************************************************************************
 *  Read the EXTREME EXOS eeprom contents .
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
#include <errno.h>
#include <endian.h>

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif


/* 
 * Alphanetworks Vendor Mappings.
 */

static sdi_exos_vendor_t sdi_exos_vendor[] = {
  { "N-4", "Alphanetworks China CS" },
  { "G-8", "Alphanetworks China DG" },
  { "G-0", "Alphanetworks Taiwan" },
  { "A-4", "SEMCO" },
  { "W-8", "Emerson Philippines" },
  { "B-4", "Delta" },
};
#define N_VENDORS (sizeof(sdi_exos_vendor) / sizeof(sdi_exos_vendor_t))

static char *sdi_extreme_exos_vendor(const char *vendor_code)
{
  int i;

  for (i = 0; i < N_VENDORS; i++) {
    if (strncmp(sdi_exos_vendor[i].vendor_code, vendor_code,
                SDI_EXTREME_EXOS_VENDOR_SIZE) == 0) {
      return sdi_exos_vendor[i].vendor_name;
    }
  }
  return SDI_STR_EXOS_VENDOR_NAME;
}


/*
 * Extreme EXOS PSU EEPROM format.
 */

static sdi_exos_info_t sdi_exos_psu_info[] = {
  { "800747-00", "PSU 770W AC FB", 1, 21000, 770, 1 },
  { "800748-00", "PSU 770W AC BF", 1, 21000, 770, 1 },
  { "800749-00", "PSU 1100W DC FB", 1, 21000, 1100, 0 },
  { "800750-00", "PSU 1100W DC BF", 1, 21000, 1100, 0 },
};
#define N_PSU_INFO (sizeof(sdi_exos_psu_info) / sizeof(sdi_exos_info_t))

t_std_error sdi_extreme_exos_psu_eeprom_data_get(void *resource_hdl,
                                                 sdi_entity_info_t *entity_info)
{
  uint8_t buf[SDI_EXTREME_EXOS_PSU_SIZE];
  sdi_device_hdl_t chip = NULL;
  t_std_error rc = STD_ERR_OK;
  uint8_t cksum = 0;
  entity_info_device_t *eeprom_data = NULL;
  char *vstr;
  int i;

  /** Validate arguments */
  chip = (sdi_device_hdl_t)resource_hdl;
  STD_ASSERT(chip != NULL);
  STD_ASSERT(entity_info != NULL);

  eeprom_data = (entity_info_device_t *)chip->private_data;

  /* Read the data from the eeprom */
  rc = sdi_smbus_read_multi_byte(chip->bus_hdl, chip->addr.i2c_addr,
                                 SDI_EXTREME_EXOS_PSU_OFFSET, buf,
                                 sizeof(buf), SDI_I2C_FLAG_NONE);
  if (rc != STD_ERR_OK) {
    SDI_DEVICE_ERRMSG_LOG("PSU EEPROM Read failed: %08x\n", rc);
    return rc;
  }

  /* Validate the checkum */
  for (i = SDI_EXTREME_EXOS_PSU_CHECKSUM_FIRST;
       i <= SDI_EXTREME_EXOS_PSU_CHECKSUM_LAST; i++)
    cksum += buf[i];
  if (cksum != buf[SDI_EXTREME_EXOS_PSU_CHECKSUM_OFFSET]) {
    SDI_DEVICE_ERRMSG_LOG("PSU EEPROM bad checksum: got %02x, expected %02x\n",
                          cksum, buf[i]);
    return SDI_DEVICE_ERRCODE(EINVAL);
  }

  /* PSU info based on EXOS partnum */
  for (i = 0; i < N_PSU_INFO; i++) {
    if (strncmp(sdi_exos_psu_info[i].partnum,
                &buf[SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET],
                strlen(sdi_exos_psu_info[i].partnum)) == 0) {
      safestrncpy(entity_info->prod_name, sdi_exos_psu_info[i].partnum,
                  MIN(NAME_MAX,SDI_EXTREME_EXOS_PART_REV_SIZE+1));
      safestrncpy(entity_info->ppid, sdi_exos_psu_info[i].prod_name,
                  MIN(SDI_PPID_LEN, strlen(sdi_exos_psu_info[i].prod_name)+1));
      entity_info->num_fans = sdi_exos_psu_info[i].num_fans;
      entity_info->max_speed = sdi_exos_psu_info[i].max_speed;
      entity_info->power_rating = sdi_exos_psu_info[i].power_rating;
      if (sdi_exos_psu_info[i].ac)
        entity_info->power_type.ac_power = 1;
      else
        entity_info->power_type.dc_power = 1;
      break;
    }
  }
  if (i == N_PSU_INFO) {
    char partnum[SDI_EXTREME_EXOS_PART_NUM_SIZE];
    safestrncpy(partnum, &buf[SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET],
                sizeof(partnum));
    SDI_DEVICE_ERRMSG_LOG("PSU EEPROM unknown partnum: %s\n", partnum);
    return SDI_DEVICE_ERRCODE(EINVAL);
  }

  vstr = sdi_extreme_exos_vendor(&buf[SDI_EXTREME_EXOS_PSU_VENDOR_OFFSET]);
  if (vstr)
    safestrncpy(entity_info->vendor_name, vstr, NAME_MAX);

  /* Obtained directly from the EXOS EEPROM */
  safestrncpy(entity_info->hw_revision,
              &buf[SDI_EXTREME_EXOS_PSU_HW_REV_OFFSET],
              MIN(SDI_HW_REV_LEN,SDI_EXTREME_EXOS_HW_REV_SIZE+1));
  safestrncpy(entity_info->service_tag,
              &buf[SDI_EXTREME_EXOS_PSU_SERIAL_NUM_OFFSET],
              MIN(NAME_MAX,SDI_EXTREME_EXOS_SERIAL_NUM_SIZE+1));
  /* Note: drop the final '-' from the EXOS part number */
  safestrncpy(entity_info->part_number,
              &buf[SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET],
              MIN(NAME_MAX,SDI_EXTREME_EXOS_PSU_PART_NUM_OFFSET));
  entity_info->air_flow =
    ((buf[SDI_EXTREME_EXOS_PSU_DIRECTION_OFFSET] == SDI_EXTREME_EXOS_F2B) ?
     SDI_PWR_AIR_FLOW_NORMAL : SDI_PWR_AIR_FLOW_REVERSE);

  /* Constants */
  safestrncpy(entity_info->platform_name, SDI_STR_EXOS_PLATFORM_NAME, NAME_MAX);

  return STD_ERR_OK;
}


/*
 * Extreme EXOS EEPROM TLV.
 */

static t_std_error sdi_extreme_exos_tlv_get(sdi_device_hdl_t chip,
                                            size_t *offset,
                                            sdi_exos_tlv_t *tlv)
{
  t_std_error rc;
  uint16_t u16[2];

  /* Get type/len */
  rc = sdi_smbus_read_multi_byte(chip->bus_hdl, chip->addr.i2c_addr,
                                 *offset, (uint8_t *)u16, sizeof(u16),
                                 SDI_I2C_FLAG_NONE);
  if (rc != STD_ERR_OK) {
    return rc;
  }
  *offset += sizeof(u16);

  tlv->type = be16toh(u16[0]);
  tlv->len  = be16toh(u16[1]);
  if (tlv->len > MAX_TLV_VALUE_LEN) {
    return SDI_DEVICE_ERRCODE(EINVAL);
  }

  /* Get the value */
  rc = sdi_smbus_read_multi_byte(chip->bus_hdl, chip->addr.i2c_addr,
                                 *offset, tlv->value, tlv->len,
                                 SDI_I2C_FLAG_NONE);
  if (rc != STD_ERR_OK) {
    return rc;
  }
  *offset += tlv->len;

  return STD_ERR_OK;
}


/*
 * Extreme EXOS FAN EEPROM format.
 */

static sdi_exos_info_t sdi_exos_fan_info[] = {
  { "800752-00", "Fantray Back To Front Airflow", 2, 21000, 0, 0 },
  { "800751-00", "Fantray Front To Back Airflow", 2, 21000, 0, 0 },
};
#define N_FAN_INFO (sizeof(sdi_exos_fan_info) / sizeof(sdi_exos_info_t))

t_std_error sdi_extreme_exos_fan_eeprom_data_get(void *resource_hdl,
                                                 sdi_entity_info_t *entity_info)
{
  sdi_device_hdl_t chip = NULL;
  size_t offset = 0;
  size_t eeprom_size = 0;
  int is_fantray = 0;
  sdi_exos_tlv_t tlv;
  char *vstr;
  t_std_error rc;
  int i;

  /** Validate arguments */
  chip = (sdi_device_hdl_t)resource_hdl;
  STD_ASSERT(chip != NULL);
  STD_ASSERT(entity_info != NULL);

  do {
    rc = sdi_extreme_exos_tlv_get(chip, &offset, &tlv);
    if (rc != STD_ERR_OK) {
      SDI_DEVICE_ERRMSG_LOG("FAN EEPROM read failed: %08x\n", rc);
      return rc;
    }
    switch (tlv.type) {
    case EXOS_TLV_TYPE_DEVICE_SIZE:
      eeprom_size = be32toh(*((uint32_t *)tlv.value));
      break;
    case EXOS_TLV_TYPE_SOFTWARE_ID:
      is_fantray = (strncmp(tlv.value, EXOS_TLV_VAL_FANTRAY,
                            strlen(EXOS_TLV_VAL_FANTRAY)) == 0);
      break;
    case EXOS_TLV_TYPE_PART_NUM:
      /* Note: drop the final '-' in the EXOS partnum */
      safestrncpy(entity_info->part_number, tlv.value,
                  MIN(SDI_PART_NUM_LEN, SDI_EXTREME_EXOS_PART_NUM_SIZE));
      safestrncpy(entity_info->hw_revision,
                  &tlv.value[SDI_EXTREME_EXOS_HW_REV_OFFSET],
                  MIN(SDI_HW_REV_LEN, SDI_EXTREME_EXOS_HW_REV_SIZE + 1));
      for (i = 0; i < N_FAN_INFO; i++) {
        if (strncmp(sdi_exos_fan_info[i].partnum, tlv.value,
                    strlen(sdi_exos_fan_info[i].partnum)) == 0) {
          safestrncpy(entity_info->prod_name, tlv.value,
                      MIN(NAME_MAX, tlv.len + 1));
          safestrncpy(entity_info->ppid, sdi_exos_fan_info[i].prod_name,
                      MIN(SDI_PPID_LEN,
                          strlen(sdi_exos_fan_info[i].prod_name)+1));
          entity_info->num_fans = sdi_exos_fan_info[i].num_fans;
          entity_info->max_speed = sdi_exos_fan_info[i].max_speed;
          break;
        }
      }
      if (i == N_FAN_INFO) {
        char partnum[17];
        safestrncpy(partnum, tlv.value, MIN(sizeof(partnum), tlv.len + 1));
        SDI_DEVICE_ERRMSG_LOG("FAN EEPROM unknown partnum \"%s\".\n", partnum);
        return SDI_DEVICE_ERRCODE(EINVAL);
      }
      break;
    case EXOS_TLV_TYPE_SERIAL_NUM:
      safestrncpy(entity_info->service_tag, tlv.value,
                  MIN(NAME_MAX, tlv.len + 1));
      vstr =
        sdi_extreme_exos_vendor(&tlv.value[SDI_EXTREME_EXOS_VENDOR_OFFSET]);
      if (vstr)
        safestrncpy(entity_info->vendor_name, vstr, NAME_MAX);
      break;
    case EXOS_TLV_TYPE_DIRECTION:
      if (tlv.value[0] == SDI_EXTREME_EXOS_F2B)
        entity_info->air_flow = SDI_PWR_AIR_FLOW_NORMAL;
      else if (tlv.value[0] == SDI_EXTREME_EXOS_B2F)
        entity_info->air_flow = SDI_PWR_AIR_FLOW_REVERSE;
      else {
        SDI_DEVICE_WARNMSG_LOG("FAN EEPROM unknown direction %02x.\n",
                               tlv.value[0]);
      }
      break;
    case EXOS_TLV_TYPE_MFG_CHECKSUM:
    case EXOS_TLV_TYPE_CLEI:
    case EXOS_TLV_TYPE_PCB_PART_NUM:
    case EXOS_TLV_TYPE_PCB_SERIAL_NUM:
    case EXOS_TLV_TYPE_DIAG_VERSION:
    case EXOS_TLV_TYPE_DIAG_FAILED:
    case EXOS_TLV_TYPE_DIAG_RUN:
    case EXOS_TLV_TYPE_DIAG_PASS:
      /* nothing to do */
    case EXOS_TLV_TYPE_EOI:
      /* end of data */
      break;
    default:
      SDI_DEVICE_ERRMSG_LOG("FAN EEPROM unknown TLV type: %04x:%04x\n",
                            tlv.type, tlv.len);
      return SDI_DEVICE_ERRCODE(EINVAL);
    }

    /* No more room for another TLV */
    if ((eeprom_size < MIN_TLV_LEN) || (offset > (eeprom_size - MIN_TLV_LEN)))
      break;

  } while (tlv.type != EXOS_TLV_TYPE_EOI);

  if (!offset || !is_fantray) {
    SDI_DEVICE_ERRMSG_LOG("FAN EEPROM invalid format %d-%d.\n",
                          offset, is_fantray);
    return SDI_DEVICE_ERRCODE(EINVAL);
  }

  if (tlv.type != EXOS_TLV_TYPE_EOI) {
    SDI_DEVICE_ERRMSG_LOG("FAN EEPROM premature end at offset: %lu\n", offset);
    return SDI_DEVICE_ERRCODE(EINVAL);
  }

  safestrncpy(entity_info->platform_name, SDI_STR_EXOS_PLATFORM_NAME, NAME_MAX);

  return STD_ERR_OK;
}
