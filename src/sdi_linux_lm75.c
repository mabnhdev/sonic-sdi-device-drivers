/*
 * Linux lm75 driver
 *
 * Copyright (C) 2016 Extreme Networks, Inc.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "std_assert.h"
#include "std_utils.h"
#include "sdi_driver_internal.h"
#include "sdi_resource_internal.h"
#include "sdi_device_common.h"
#include "sdi_thermal_internal.h"
#include "sdi_temperature_resource_attr.h"

#include "sdi_tmp75_reg.h"
#include "sdi_sysfs_helpers.h"

#define ATTR_TEMP     "temp1_input"
#define ATTR_MAX      "temp1_max"
#define ATTR_MAX_HYST "temp1_max_hyst"

/*
 * The LM75 expresses temperatures in millidegrees.
 * The sdi expresses temperatures in degrees.
 * Internally, we maintain addresses in lm75 format.
 * Externally, we expect sdi format.
 * The macros convert between the two.
 */
#define LM75_TO_SDI(t75) (((((t75) % 1000) >= 500) ? (t75) + 500 : (t75)) / 1000)
#define SDI_TO_LM75(tx) ((tx) * 1000)

typedef struct linux_lm75_device
{
  /* Sensor threshold limits */
  int low_threshold;
  int high_threshold;
  /* Default sensor limits */
  int default_low_threshold;
  int default_high_threshold;
  /* Current temperature */
  int32_t current_temp;
  /* Attribute handles */
  sdi_sysfs_attr_hdl_t temperature;
  sdi_sysfs_attr_hdl_t max;
  sdi_sysfs_attr_hdl_t max_hyst;
} linux_lm75_device_t;

static t_std_error sdi_linux_lm75_register(std_config_node_t node,
                                           void *bus_handle,
                                           sdi_device_hdl_t *device_hdl);
static t_std_error sdi_linux_lm75_chip_init(sdi_device_hdl_t device_hdl);

static t_std_error sdi_linux_lm75_temperature_get(void *resource_hdl,
                                                  int *temperature)
{
  sdi_device_hdl_t chip = (sdi_device_hdl_t)resource_hdl;
  linux_lm75_device_t *lm75_data = NULL;  
  t_std_error rc = STD_ERR_OK;

  STD_ASSERT(chip != NULL);
  lm75_data = (linux_lm75_device_t *)chip->private_data;
  STD_ASSERT(lm75_data != NULL);

  rc = sdi_sysfs_read_int32(lm75_data->temperature, &lm75_data->current_temp);
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d cannot read %s - %08x",
                          chip->instance, ATTR_TEMP, rc);
    return rc;
  }

  *temperature = LM75_TO_SDI(lm75_data->current_temp);

  return rc;
}

static t_std_error sdi_linux_lm75_threshold_get(void *resource_hdl,
                                                sdi_threshold_t type,
                                                int *temperature)
{
  sdi_device_hdl_t chip = (sdi_device_hdl_t)resource_hdl;
  linux_lm75_device_t *lm75_data = NULL;  
  t_std_error rc = STD_ERR_OK;

  STD_ASSERT(chip != NULL);
  lm75_data = (linux_lm75_device_t *)chip->private_data;
  STD_ASSERT(lm75_data != NULL);

  switch(type) {
  case SDI_LOW_THRESHOLD:
    *temperature = LM75_TO_SDI(lm75_data->low_threshold);
    break;

  case SDI_HIGH_THRESHOLD:
    *temperature = LM75_TO_SDI(lm75_data->high_threshold);
    break;

  default:
    return SDI_DEVICE_ERRCODE(EOPNOTSUPP);
  }
  return STD_ERR_OK;
}

static t_std_error sdi_linux_lm75_threshold_set(void *resource_hdl,
                                                sdi_threshold_t type,
                                                int temperature)
{
  sdi_device_hdl_t chip = (sdi_device_hdl_t)resource_hdl;
  linux_lm75_device_t *lm75_data = NULL;  
  t_std_error rc = STD_ERR_OK;

  STD_ASSERT(chip != NULL);
  lm75_data = (linux_lm75_device_t *)chip->private_data;
  STD_ASSERT(lm75_data != NULL);

  if(temperature & (~(0xff))) {
    return SDI_DEVICE_ERRCODE(EINVAL);
  }

  switch(type) {
  case SDI_LOW_THRESHOLD:
    lm75_data->low_threshold = SDI_TO_LM75(temperature);
    break;

  case SDI_HIGH_THRESHOLD:
    lm75_data->high_threshold = SDI_TO_LM75(temperature);
    rc = sdi_sysfs_write_int32(lm75_data->max, lm75_data->high_threshold);
    if (STD_IS_ERR(rc)) {
      SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s cannot be set - %08x",
                            chip->instance, ATTR_MAX, rc);
      return rc;
    }
    rc = sdi_sysfs_write_int32(lm75_data->max_hyst,
                               lm75_data->high_threshold - SDI_TO_LM75(5));
    if (STD_IS_ERR(rc)) {
      SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s cannot be set - %08x",
                            chip->instance, ATTR_MAX_HYST, rc);
      return rc;
    }
    break;

  default:
    return SDI_DEVICE_ERRCODE(EOPNOTSUPP);
  }
  return rc;
}

static t_std_error sdi_linux_lm75_status_get(void *resource_hdl, bool *status)
{
  sdi_device_hdl_t chip = (sdi_device_hdl_t)resource_hdl;
  linux_lm75_device_t *lm75_data = NULL;  
  t_std_error rc = STD_ERR_OK;
  int32_t temp;

  STD_ASSERT(chip != NULL);
  lm75_data = (linux_lm75_device_t *)chip->private_data;
  STD_ASSERT(lm75_data != NULL);

  rc = sdi_linux_lm75_temperature_get(resource_hdl, &temp);
  if (STD_IS_ERR(rc)) {
    return rc;
  }
  temp = SDI_TO_LM75(temp);

  *status = ((temp >= lm75_data->high_threshold) ||
             (temp <= lm75_data->low_threshold)) ? true : false;

  return rc;
}

temperature_sensor_t linux_lm75_sensor={
  NULL, /* As the init is done as part of chip init, 
           resource init is not required*/
  sdi_linux_lm75_temperature_get,
  sdi_linux_lm75_threshold_get,
  sdi_linux_lm75_threshold_set,
  sdi_linux_lm75_status_get
};

/* Export the Driver table */
sdi_driver_t linux_lm75_entry={
  sdi_linux_lm75_register,
  sdi_linux_lm75_chip_init
};

/*
 * The config file format will be as below for tmp75 devices
 *
 * <linux_lm75 instance="<chip_instance>"
 * addr="<Address of the device>"
 * low_threshold="<low threshold value>" high_threshold="<high threshold value>"
 * alias="<Alias name for the particular devide>">
 * </linux_lm75>
 * Mandatory attributes    : instance and addr
 */
 /*
 * This the call back function for the device registration
 * [in] node - Config node for the device
 * [in] bus_handle - Parent bus handle of the device
 * [out] device_hdl - pointer to the device handle which will be updated by this function
 * Return - STD_ERR_OK, this function is kept as non void as the driver table fp requires it
 */
static t_std_error sdi_linux_lm75_register(std_config_node_t node,
                                           void *bus_handle,
                                           sdi_device_hdl_t *device_hdl)
{
  char *node_attr = NULL;
  sdi_device_hdl_t chip = NULL;
  sdi_bus_t *bus = (sdi_bus_t*)bus_handle;
  linux_lm75_device_t *lm75_data = NULL;

  STD_ASSERT(node != NULL);
  STD_ASSERT(bus_handle != NULL);
  STD_ASSERT(device_hdl != NULL);
  STD_ASSERT(bus->bus_type == SDI_I2C_BUS);

  chip = calloc(sizeof(sdi_device_entry_t),1);
  STD_ASSERT(chip != NULL);

  lm75_data = calloc(sizeof(*lm75_data),1);
  STD_ASSERT(lm75_data);

  chip->bus_hdl = bus_handle;

  node_attr = std_config_attr_get(node, SDI_DEV_ATTR_INSTANCE);
  STD_ASSERT(node_attr != NULL);
  chip->instance = (uint_t) strtoul(node_attr, NULL, 0);

  node_attr = std_config_attr_get(node, SDI_DEV_ATTR_ADDRESS);
  STD_ASSERT(node_attr != NULL);
  chip->addr.i2c_addr = (i2c_addr_t) strtoul(node_attr, NULL, 16);

  chip->callbacks = &linux_lm75_entry;
  chip->private_data = (void*)lm75_data;

  node_attr = std_config_attr_get(node, SDI_DEV_ATTR_ALIAS);
  if (node_attr == NULL) {
    snprintf(chip->alias,SDI_MAX_NAME_LEN,"lm75-%d", chip->instance );
  } else {
    safestrncpy(chip->alias,node_attr,SDI_MAX_NAME_LEN);
  }

  node_attr = std_config_attr_get(node, SDI_DEV_ATTR_TEMP_LOW_THRESHOLD);
  if(node_attr != NULL) {
    lm75_data->default_low_threshold = (int) strtol(node_attr, NULL, 0);
  } else {
    lm75_data->default_low_threshold = TMP75_DEFAULT_TLOW;
  }
  lm75_data->default_low_threshold = SDI_TO_LM75(lm75_data->default_low_threshold);
  lm75_data->low_threshold = lm75_data->default_low_threshold;

  node_attr = std_config_attr_get(node, SDI_DEV_ATTR_TEMP_HIGH_THRESHOLD);
  if(node_attr != NULL) {
    lm75_data->default_high_threshold = (int) strtol(node_attr, NULL, 0);
  } else {
    lm75_data->default_high_threshold = TMP75_DEFAULT_THIGH;
  }
  lm75_data->default_high_threshold = SDI_TO_LM75(lm75_data->default_high_threshold);
  lm75_data->high_threshold = lm75_data->default_high_threshold;

  sdi_resource_add(SDI_RESOURCE_TEMPERATURE, chip->alias,
                   (void*)chip, &linux_lm75_sensor);

  *device_hdl = chip;
  
  return STD_ERR_OK;
}

/*
 * Initialize the lm75 chip.
 *
 * Read the initial temperature, set the max and max_hyst.
 */
static t_std_error sdi_linux_lm75_chip_init(sdi_device_hdl_t device_hdl)
{
  linux_lm75_device_t *lm75_data = NULL;  
  t_std_error rc = STD_ERR_OK;

  STD_ASSERT(device_hdl != NULL);
  lm75_data = (linux_lm75_device_t *)device_hdl->private_data;
  STD_ASSERT(lm75_data != NULL);

  rc = sdi_sysfs_open_attr(device_hdl, ATTR_TEMP, SDI_SYSFS_READ_ONLY,
                           SDI_SYSFS_FMT_INT, &lm75_data->temperature);
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s not found - %08x",
                          device_hdl->instance, ATTR_TEMP, rc);
    return rc;
  }

  rc = sdi_sysfs_read_int32(lm75_data->temperature, &lm75_data->current_temp);
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d cannot read %s - %08x",
                          device_hdl->instance, ATTR_TEMP, rc);
    return rc;
  }

  rc = sdi_sysfs_open_attr(device_hdl, ATTR_MAX, SDI_SYSFS_READ_WRITE,
                           SDI_SYSFS_FMT_INT, &lm75_data->max);
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s not found - %08x",
                          device_hdl->instance, ATTR_MAX, rc);
    return rc;
  }

  rc = sdi_sysfs_write_int32(lm75_data->max, lm75_data->high_threshold);
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s cannot be set - %08x",
                          device_hdl->instance, ATTR_MAX, rc);
    return rc;
  }

  rc = sdi_sysfs_open_attr(device_hdl, ATTR_MAX_HYST, SDI_SYSFS_READ_WRITE,
                           SDI_SYSFS_FMT_INT, &lm75_data->max_hyst);
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s not found - %08x",
                          device_hdl->instance, ATTR_MAX_HYST, rc);
    return rc;
  }

  rc = sdi_sysfs_write_int32(lm75_data->max_hyst,
                             lm75_data->high_threshold - SDI_TO_LM75(5));
  if (STD_IS_ERR(rc)) {
    SDI_DEVICE_ERRMSG_LOG("chip-%d attribute %s cannot be set - %08x",
                          device_hdl->instance, ATTR_MAX_HYST, rc);
    return rc;
  }

  return rc;
}
