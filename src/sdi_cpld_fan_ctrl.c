/*
 * Copyright (c) 2016 Extreme Networks Inc.
 */

/*
 * filename: sdi_cpld_fan_ctrl.c
 */


/******************************************************************************
  * sdi_cpld_fan_ctrl.c
  * Implements the driver for CPLD-based fan control.
  ******************************************************************************/

#include "sdi_driver_internal.h"
#include "sdi_resource_internal.h"
#include "sdi_fan_internal.h"
#include "sdi_common_attr.h"
#include "sdi_fan_resource_attr.h"
#include "sdi_pin_group_bus_framework.h"
#include "sdi_pin_group_bus_api.h"
#include "sdi_pin_bus_framework.h"
#include "sdi_pin_bus_api.h"
#include "sdi_i2c_bus_api.h"
#include "sdi_device_common.h"
#include "std_assert.h"
#include "std_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/*
 * CPLD per-fan private data
 */
#define CPLD_MAX_FANS 16
typedef struct cpld_fan_device
{
    char *alias;
    sdi_pin_group_bus_hdl_t speed_hdl;
    sdi_pin_group_bus_hdl_t pwm_hdl;
    sdi_pin_bus_hdl_t status_hdl;
    uint_t max_rpm;
} cpld_fan_device_t;

/*
 * CPLD Fan Control device private data
 */
typedef struct cpld_fan_ctrl_device
{
    uint_t speed_multiplier;
    uint_t pwm_off;
    uint_t pwm_full;
    cpld_fan_device_t fans[CPLD_MAX_FANS];
} cpld_fan_ctrl_device_t;

/*
 * CPLD per-fan resource handle
 */
typedef struct cpld_fan_resource_hdl
{
    sdi_device_hdl_t cpld_fan_ctrl_hdl;
    uint_t fan_id;
} cpld_fan_resource_hdl_t;

static t_std_error cpld_fan_ctrl_fan_speed_get(void *resource_hdl, uint_t *speed);
static t_std_error cpld_fan_ctrl_fan_speed_set(void *resource_hdl, uint_t speed);
static t_std_error cpld_fan_ctrl_fan_status_get(void *resource_hdl, bool *status);
static t_std_error cpld_fan_ctrl_resource_init(void *resource_hdl, uint_t max_rpm);
static t_std_error cpld_fan_ctrl_register(std_config_node_t node, void *bus_handle,
                                          sdi_device_hdl_t* device_hdl);
static t_std_error cpld_fan_ctrl_init(sdi_device_hdl_t device_hdl);

/*
 * CPLD fan callbacks
 */
fan_ctrl_t cpld_fan_ctrl_fan_resource = {
    cpld_fan_ctrl_resource_init,
    cpld_fan_ctrl_fan_speed_get,
    cpld_fan_ctrl_fan_speed_set,
    cpld_fan_ctrl_fan_status_get
};

/* 
 * Export the Driver table
 */
sdi_driver_t cpld_fan_ctrl_entry = {
        cpld_fan_ctrl_register,
        cpld_fan_ctrl_init
};

/*
 * Callback function to retrieve the speed of the fan referred by resource
 * [in] resource_hdl - callback data for this function,chip instance is passed as a callback data
 * [out] speed - pointer to a buffer to get the fan speed
 * Return - STD_ERR_OK for success or the respective error code from i2c API in case of failure
 */
/*  Read the tach counter and multiply by the tach multiplier to get RPM.
 */
static t_std_error cpld_fan_ctrl_fan_speed_get(void *resource_hdl, uint_t *speed)
{
    cpld_fan_resource_hdl_t *resource = (cpld_fan_resource_hdl_t *)resource_hdl;
    cpld_fan_device_t *fan;
    cpld_fan_ctrl_device_t *dev;
    t_std_error rc = STD_ERR_OK;
    uint_t rpm;

    STD_ASSERT(resource != NULL);
    STD_ASSERT(resource->cpld_fan_ctrl_hdl != NULL);
    dev = (cpld_fan_ctrl_device_t *)(resource->cpld_fan_ctrl_hdl->private_data);
    STD_ASSERT(dev != NULL);
    STD_ASSERT(resource->fan_id < CPLD_MAX_FANS);
    fan = &dev->fans[resource->fan_id];

    rc = sdi_pin_group_read_level(fan->speed_hdl, &rpm);
    if (rc != STD_ERR_OK) {
        SDI_DEVICE_ERRMSG_LOG("Reading value from pin group failed with rc : %d", rc);
        goto exit;
    }

    *speed = rpm * dev->speed_multiplier;
exit:
    return rc;
}

/*
 * Callback function to set the speed of the fan referred by resource
 * Parameters:
 * [in] resource_hdl - callback data for this function,chip instance is passed as a callback data
 * [in] speed - Speed to be set
 * Return - STD_ERR_OK for success or the respective error code from i2c API in case of failure
 */
static t_std_error cpld_fan_ctrl_fan_speed_set(void *resource_hdl, uint_t speed)
{
    cpld_fan_resource_hdl_t *resource = (cpld_fan_resource_hdl_t *)resource_hdl;
    cpld_fan_device_t *fan;
    cpld_fan_ctrl_device_t *dev;
    t_std_error rc = STD_ERR_OK;
    uint_t pwm;

    STD_ASSERT(resource != NULL);
    STD_ASSERT(resource->cpld_fan_ctrl_hdl != NULL);
    dev = (cpld_fan_ctrl_device_t *)(resource->cpld_fan_ctrl_hdl->private_data);
    STD_ASSERT(dev != NULL);
    STD_ASSERT(resource->fan_id < CPLD_MAX_FANS);
    fan = &dev->fans[resource->fan_id];

    pwm = (speed * (dev->pwm_full - dev->pwm_off)) / fan->max_rpm;

    rc = sdi_pin_group_acquire_bus(fan->pwm_hdl);
    if (rc != STD_ERR_OK) {
        SDI_DEVICE_ERRMSG_LOG("Acquire pin group bus failed with rc : %d", rc);
        goto exit;
    }

    rc = sdi_pin_group_write_level(fan->pwm_hdl, pwm);
    if (rc != STD_ERR_OK) {
        SDI_DEVICE_ERRMSG_LOG("Writing value to pin group bus is failed with rc : %d", rc);
        goto exit_locked;
    }

exit_locked:
    sdi_pin_group_release_bus(fan->pwm_hdl);
exit:
    return rc;
}

/*
 * Callback function to retrieve the fault status of the fan referred by resource
 * Parameters:
 * [in] resource_hdl - callback data for this function
 * [in] status - pointer to a buffer to get the fault status of the fan
 * Return - STD_ERR_OK for success or the respective error code from i2c API in case of failure
 */
static t_std_error cpld_fan_ctrl_fan_status_get(void *resource_hdl, bool *status)
{
    cpld_fan_resource_hdl_t *resource = (cpld_fan_resource_hdl_t *)resource_hdl;
    cpld_fan_device_t *fan = NULL;
    cpld_fan_ctrl_device_t *dev = NULL;
    t_std_error rc = STD_ERR_OK;
    sdi_pin_bus_level_t pin_val = SDI_PIN_LEVEL_LOW;

    STD_ASSERT(resource != NULL);
    STD_ASSERT(resource->cpld_fan_ctrl_hdl != NULL);
    dev = (cpld_fan_ctrl_device_t *)(resource->cpld_fan_ctrl_hdl->private_data);
    STD_ASSERT(dev != NULL);
    STD_ASSERT(resource->fan_id < CPLD_MAX_FANS);
    fan = &dev->fans[resource->fan_id];

    rc = sdi_pin_read_level(fan->status_hdl, &pin_val);
    if (rc != STD_ERR_OK) {
        SDI_DEVICE_ERRMSG_LOG("Reading value from pin failed with rc : %d", rc);
        goto exit;
    }

    STD_ASSERT(status != NULL);
    *status = (pin_val != 0);
exit:
    return rc;
}

 /*
 * Callback function to initialize the  fan referred by resource
 * [in] resource_hdl - callback data for this function
 * [in] max_rpm - Maximum speed of the fan referred by resource
 * Return - STD_ERR_OK for success or the respective error code from i2c API in case of failure
 */
static t_std_error cpld_fan_ctrl_resource_init(void *resource_hdl, uint_t max_rpm)
{
    cpld_fan_resource_hdl_t *resource = (cpld_fan_resource_hdl_t *)resource_hdl;
    cpld_fan_ctrl_device_t *dev;
    cpld_fan_device_t *fan;
    t_std_error rc = STD_ERR_OK;

    STD_ASSERT(resource != NULL);
    STD_ASSERT(resource->cpld_fan_ctrl_hdl != NULL);
    dev = (cpld_fan_ctrl_device_t *)(resource->cpld_fan_ctrl_hdl->private_data);
    STD_ASSERT(dev != NULL);
    STD_ASSERT(resource->fan_id < CPLD_MAX_FANS);
    fan = &dev->fans[resource->fan_id];

    fan->max_rpm = max_rpm;

    /* Start the fan out at half speed. */
    return cpld_fan_ctrl_fan_speed_set(resource_hdl, max_rpm/2);
}

/*
 * Creates the resource handle for a specific fan.
 * Parameters:
 * [in] dev_hdl - cpld fan device handle
 * [in] fan_id - id for the specific fan
 * Return cpld_fan_resource_hdl_t - resource handle for the specific fan
 */
static cpld_fan_resource_hdl_t *
cpld_fan_ctrl_create_resource_hdl(sdi_device_hdl_t dev_hdl, uint_t fan_id)
{
    cpld_fan_resource_hdl_t *resource;

    STD_ASSERT(dev_hdl != NULL);

    resource = calloc(sizeof(cpld_fan_resource_hdl_t), 1);
    STD_ASSERT(resource != NULL);
    resource->cpld_fan_ctrl_hdl = dev_hdl;
    resource->fan_id = fan_id;

    return resource;
}

/*
 * Update the database for a specific fan.
 * Parametrs:
 * [in] cur_node - temperature sensor node
 * [in] device_hdl - device handle of max6620 device
 * Return - none
 */
static void cpld_fan_register(std_config_node_t cur_node, void* device_hdl)
{
    sdi_device_hdl_t dev_hdl = (sdi_device_hdl_t)device_hdl;
    uint_t fan_id = 0;
    char *node_attr = NULL;
    size_t node_attr_len = 0;
    cpld_fan_ctrl_device_t *cpld_fan_ctrl_data;
    cpld_fan_device_t *fan = NULL;

    STD_ASSERT(device_hdl != NULL);
    cpld_fan_ctrl_data = (cpld_fan_ctrl_device_t *)dev_hdl->private_data;
    STD_ASSERT(cpld_fan_ctrl_data != NULL);

    if (strncmp(std_config_name_get(cur_node), SDI_DEV_NODE_FAN,
                strlen(SDI_DEV_NODE_FAN)))
    {
        return;
    }

    node_attr = std_config_attr_get(cur_node, SDI_DEV_ATTR_INSTANCE);
    if(node_attr != NULL)
    {
        fan_id = (uint_t) strtoul(node_attr, NULL, 0);
    }

    STD_ASSERT(fan_id < CPLD_MAX_FANS);
    fan = &cpld_fan_ctrl_data->fans[fan_id];

    node_attr = std_config_attr_get(cur_node, SDI_DEV_ATTR_ALIAS );
    if(node_attr == NULL)
    {
        fan->alias = malloc(SDI_MAX_NAME_LEN);
        STD_ASSERT(fan->alias != NULL);
        snprintf(fan->alias, SDI_MAX_NAME_LEN, "cpld-fan-%d-%d",
                 dev_hdl->instance, fan_id);
    }
    else
    {
        node_attr_len = strlen(node_attr)+1;
        fan->alias = malloc(node_attr_len);
        STD_ASSERT(fan->alias != NULL);
        memcpy(fan->alias, node_attr, node_attr_len);
    }

    node_attr = std_config_attr_get(cur_node, SDI_DEV_CPLD_FAN_SPEED);
    STD_ASSERT(node_attr != NULL);
    fan->speed_hdl = sdi_get_pin_group_bus_handle_by_name(node_attr);
    STD_ASSERT(fan->speed_hdl != NULL);

    node_attr = std_config_attr_get(cur_node, SDI_DEV_CPLD_FAN_PWM);
    STD_ASSERT(node_attr != NULL);
    fan->pwm_hdl = sdi_get_pin_group_bus_handle_by_name(node_attr);
    STD_ASSERT(fan->pwm_hdl != NULL);

    node_attr = std_config_attr_get(cur_node, SDI_DEV_CPLD_FAN_STATUS);
    STD_ASSERT(node_attr != NULL);
    fan->status_hdl = sdi_get_pin_bus_handle_by_name(node_attr);
    STD_ASSERT(fan->status_hdl != NULL);

    sdi_resource_add(SDI_RESOURCE_FAN, fan->alias,
                     cpld_fan_ctrl_create_resource_hdl(dev_hdl, fan_id),
                     &cpld_fan_ctrl_fan_resource);
}

/*
 * The configuration file format for the CPLD-Fan device node is as follows
 *<cpld_fan_ctrl instance="<chip_instance>" fan_speed_multiplier="tach multiplier" fan_pwm_off="off setting" fan_pwm_full="full setting" />
 *<fan instance="<fan no>" alias="<fan alias>" fan_speed_bus="cpld_pin_group" fan_pwm_bus="cpld_pin_group" fan_status_bus="cpld_pin"/>
 *</cpld_fan_ctrl >
 */

/* Registers the fan and it's resources with the SDI framework.
 * Parameters:
 * [in] node - cpld_fan_ctrl device node from the configuration file
 * [in] bus_handle - Parent bus handle of the device
 * [out] device_hdl - Pointer to the cpld_fan_ctrl device handle which will get filled by this function
 * Return - STD_ERR_OK, this function is kept as non void as the driver table function pointer
 * requires it
 */
static t_std_error cpld_fan_ctrl_register(std_config_node_t node, void *bus_handle,
                                          sdi_device_hdl_t* device_hdl)
{
    char *node_attr = NULL;
    sdi_device_hdl_t dev_hdl = NULL;
    t_std_error rc = STD_ERR_OK;
    cpld_fan_ctrl_device_t *cpld_fan_ctrl_data = NULL;

    STD_ASSERT(node != NULL);
    STD_ASSERT(bus_handle != NULL);
    STD_ASSERT(device_hdl != NULL);
    STD_ASSERT(((sdi_bus_t*)bus_handle)->bus_type == SDI_I2C_BUS);

    dev_hdl = calloc(sizeof(sdi_device_entry_t), 1);
    STD_ASSERT(dev_hdl != NULL);

    cpld_fan_ctrl_data = calloc(sizeof(cpld_fan_ctrl_device_t), 1);
    STD_ASSERT(cpld_fan_ctrl_data != NULL);

    dev_hdl->bus_hdl = bus_handle;
    dev_hdl->callbacks = &cpld_fan_ctrl_entry;
    dev_hdl->private_data = cpld_fan_ctrl_data;

    node_attr = std_config_attr_get(node, SDI_DEV_ATTR_INSTANCE);
    STD_ASSERT(node_attr != NULL);
    dev_hdl->instance = (uint_t) strtoul(node_attr, NULL, 0);

    node_attr = std_config_attr_get(node, SDI_DEV_ATTR_ALIAS);
    if (node_attr == NULL) {
        snprintf(dev_hdl->alias, SDI_MAX_NAME_LEN, "sdi-cpld-fan-ctrl-%d",
                 dev_hdl->instance);
    } else {
        safestrncpy(dev_hdl->alias,node_attr,SDI_MAX_NAME_LEN);
    }

    node_attr = std_config_attr_get(node, SDI_DEV_CPLD_FAN_MULTIPLIER);
    STD_ASSERT(node_attr != NULL);
    cpld_fan_ctrl_data->speed_multiplier = (uint_t) strtoul(node_attr, NULL, 0);

    node_attr = std_config_attr_get(node, SDI_DEV_CPLD_FAN_PWM_OFF);
    STD_ASSERT(node_attr != NULL);
    cpld_fan_ctrl_data->pwm_off = (uint_t) strtoul(node_attr, NULL, 0);

    node_attr = std_config_attr_get(node, SDI_DEV_CPLD_FAN_PWM_FULL);
    STD_ASSERT(node_attr != NULL);
    cpld_fan_ctrl_data->pwm_full = (uint_t) strtoul(node_attr, NULL, 0);

    std_config_for_each_node(node, cpld_fan_register, dev_hdl);

    *device_hdl = dev_hdl;

    return rc;
}

/*
 * CPLD-controlled fan initialization.
 * [in] device_hdl - device handle of the specific device
 * Return - STD_ERR_OK for success or the respective error code from i2c API in case of failure
 */
static t_std_error cpld_fan_ctrl_init(sdi_device_hdl_t device_hdl)
{
    t_std_error rc = STD_ERR_OK;
    
    return rc;
}
