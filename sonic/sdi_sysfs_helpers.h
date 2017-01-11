/*
 * /sysfs helpers
 *
 * Copyright (C) 2016 Extreme Networks, Inc.
 *
 */

#ifndef __SDI_SYSFS_HELPERS_H__
#define __SDI_SYSFS_HELPERS_H__

#include <stdio.h>
#include <sys/queue.h>

typedef enum {
  SDI_SYSFS_READ_ONLY,
  SDI_SYSFS_WRITE_ONLY,
  SDI_SYSFS_READ_WRITE,
} sdi_sysfs_mode_t;

typedef enum {
  SDI_SYSFS_FMT_INT,
  SDI_SYSFS_FMT_UINT,
  SDI_SYSFS_FMT_BIN,
  SDI_SYSFS_FMT_MAX /* always last */
} sdi_sysfs_fmt_t;

typedef void *sdi_sysfs_attr_hdl_t;

t_std_error sdi_sysfs_open_attr(sdi_device_hdl_t chip, const char *name,
                                const sdi_sysfs_mode_t mode,
                                const sdi_sysfs_fmt_t fmt,
                                sdi_sysfs_attr_hdl_t *hdl);

t_std_error sdi_sysfs_read_int32(sdi_sysfs_attr_hdl_t hdl, int32_t *val);
t_std_error sdi_sysfs_write_int32(sdi_sysfs_attr_hdl_t hdl, const int32_t val);

#endif /* __SDI_SYSFS_HELPERS_H__ */
