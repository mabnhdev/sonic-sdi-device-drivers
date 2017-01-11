/*
 * /sys filesystem helpers
 *
 * Copyright (C) 2016 Extreme Networks, Inc.
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include "std_assert.h"
#include "sdi_driver_internal.h"
#include "sdi_device_common.h"
#include "sdi_resource_internal.h"
#include "sdi_sysfs_helpers.h"

/*
 * MAX_OPEN_ATTRS == 0
 *
 * Attribute file is opened by the process only for the time
 * that the attribute is being accessed.
 *
 * MAX_OPEN_ATTRS > 0
 *
 * Attribute file is kept open by the library for the life
 * of the attribute or until it needs to be closed because
 * the max number of open files is about to be exceeded.
 *
 * The trade-off is the mutual exclusion of file usage and
 * management of an LRU list versus the cost of open/close
 * for each access to the attribute.
 */
/*
 * TODO: MAX_OPEN_ATTRS > 0 is not fully implemented yet.
 *       Must come up with a per-attribute locking mechanism -
 *           Probably shm_open(), etc.
 */
#define MAX_OPEN_ATTRS 0

static char *sdi_sysfs_fmt_str[SDI_SYSFS_FMT_MAX] =
  {
    [SDI_SYSFS_FMT_INT] = "%d\n",
    [SDI_SYSFS_FMT_UINT] = "%u\n",
    [SDI_SYSFS_FMT_BIN] = NULL,
  };

typedef struct sdi_sysfs_attr
{
  TAILQ_ENTRY(sdi_sysfs_attr) lru;
  int ref;
#if (MAX_OPEN_ATTRS > 0)
  FILE *fp;
#endif
  sdi_sysfs_mode_t mode;
  char modestr[4];
  sdi_sysfs_fmt_t fmt;
  const char *sfmt;
  char path[]; /* must always be last */
} sdi_sysfs_attr_t;

#if (MAX_OPEN_ATTRS > 0)
static int n_open_attr_files = 0;
#endif
static TAILQ_HEAD(sdi_sysfs_attrs, sdi_sysfs_attr) sdi_sysfs_attrs =
  TAILQ_HEAD_INITIALIZER(sdi_sysfs_attrs);

#define SDI_SYSFS_MUTEX_PATH "/sdi_sysfs_mutex"
static sem_t *sdi_sysfs_mutex = NULL;

#define SYSFS_I2C_BUS_FMT "/sys/bus/i2c/devices/i2c-%d/%d-%04x"

/*
 * Mutual exclusion for the attribute list.
 */
static void sdi_sysfs_mutex_lock(void)
{
  int rc;

  /* Create lock of it doesn't exist */
  if (!sdi_sysfs_mutex) {
    sdi_sysfs_mutex = sem_open(SDI_SYSFS_MUTEX_PATH, O_CREAT,
                               S_IRUSR | S_IWUSR, 1);
    STD_ASSERT(sdi_sysfs_mutex != NULL);
  }
  rc = sem_wait(sdi_sysfs_mutex);
  STD_ASSERT(rc == 0);
}

static void sdi_sysfs_mutex_unlock(void)
{
  int rc;

  STD_ASSERT(sdi_sysfs_mutex != NULL);
  rc = sem_post(sdi_sysfs_mutex);
  STD_ASSERT(rc == 0);
}

/*
 * Open the attribute file if it is not already open.
 * Place the file at the tail of the LRU list of attributes.
 *
 * If the number of open files will exceed a maximum, then
 * close the least recently used file.
 */
static char *sdi_sysfs_mode_to_str[] = {"r", "w", "a+"}; 
static FILE *sdi_sysfs_open_attr_file(sdi_sysfs_attr_t *attr)
{
  int rc = 0;
  FILE *fp = NULL;

#if (MAX_OPEN_ATTRS > 0)
  sdi_sysfs_mutex_lock();

  /* Move the entry to the end of the LRU list. */
  TAILQ_REMOVE(&sdi_sysfs_attrs, attr, lru);
  TAILQ_INSERT_TAIL(&sdi_sysfs_attrs, attr, lru);

  /* If already open, position fp to start. */
  if ((fp = attr->fp)) {
    rewind(fp);
    goto exit;
  }

  /* Too many files open, close the LRU */
  while (n_open_attr_files >= MAX_OPEN_ATTRS) {
    sdi_sysfs_attr_t *lru;
    TAILQ_FOREACH(lru, &sdi_sysfs_attrs, lru) {
      if (lru->fp) {
        fclose(lru->fp);
        lru->fp = NULL;
        n_open_attr_files--;
      }
    }
  }
#endif

  fp = fopen(attr->path, sdi_sysfs_mode_to_str[attr->mode]);
  if (!fp) {
    goto exit;
  }

#if (MAX_OPEN_ATTRS > 0)
  else {
    n_open_attr_files++;
    attr->fp = fp;
  }
#endif

 exit:
#if (MAX_OPEN_ATTRS > 0)
  /* TODO - lock the file */
  sdi_sysfs_mutex_unlock();
#endif
  return fp;
}

/* 
 * Finish access to attribute file.
 */
static void sdi_sysfs_close_attr_file(FILE *fp, sdi_sysfs_attr_t *attr)
{
#if (MAX_OPEN_ATTRS == 0)
  fclose(fp);
#else
  /* TODO - unlock the file */
  return;
#endif
}


/*
 * Read from an attribute using a formatted string.
 */
t_std_error sdi_sysfs_read_format(sdi_sysfs_attr_hdl_t hdl, const size_t nvals,
                                  const char *fmt, ...)
{
  sdi_sysfs_attr_t *attr = (sdi_sysfs_attr_t *)hdl;
  va_list valist;
  int rc;
  FILE *fp;

  STD_ASSERT(attr != NULL);
  fp = sdi_sysfs_open_attr_file(attr);
  if (!fp) {
    rc = ENOENT;
    goto exit;
  }

  va_start(valist, fmt);
  rc = vfscanf(fp, fmt, valist);
  va_end(valist);
  if (rc == nvals)
    rc = 0;
  else
    rc = errno;

 exit:
  if (fp)
    sdi_sysfs_close_attr_file(fp, attr);
  if (rc)
    return SDI_DEVICE_ERRCODE(-rc);
  else
    return 0;
}

/*
 * Write to an attribute using a formatted string.
 */
t_std_error sdi_sysfs_write_format(sdi_sysfs_attr_hdl_t hdl, const size_t nvals,
                                   const char *fmt, ...)
{
  sdi_sysfs_attr_t *attr = (sdi_sysfs_attr_t *)hdl;
  va_list valist;
  int rc;
  FILE *fp;

  STD_ASSERT(attr != NULL);
  fp = sdi_sysfs_open_attr_file(attr);
  if (!fp) {
    rc = ENOENT;
    goto exit;
  }

  va_start(valist, fmt);
  rc = vfprintf(fp, fmt, valist);
  va_end(valist);
  if (rc > 0)
    rc = 0;
  else
    rc = errno;

 exit:
  if (fp)
    sdi_sysfs_close_attr_file(fp, attr);
  if (rc)
    return SDI_DEVICE_ERRCODE(-rc);
  else
    return 0;
}

/*
 * Return the chip's root dir in the /sys filesystem.
 */
static void sdi_sysfs_chip_to_path(sdi_device_hdl_t chip, char *buf,
                                   size_t maxlen)
{
  sdi_bus_t *bus;

  STD_ASSERT(chip != NULL);
  bus = (sdi_bus_t*)chip->bus_hdl;
  STD_ASSERT(bus != NULL);
  STD_ASSERT(bus->bus_type == SDI_I2C_BUS);
  
  snprintf(buf, maxlen, SYSFS_I2C_BUS_FMT, (unsigned)bus->bus_id,
           (unsigned)bus->bus_id, (unsigned )(chip->addr.i2c_addr));
}

/*
 * Find the named attribute under the device's root dir.
 * This is a recursive function.
 */
static int sdi_sysfs_find_attr(const char *root, const char *name,
                               char *path, size_t maxlen)
{
  DIR *dp;
  struct dirent *entry;
  struct stat stat;
  char dpath[256];
  int err = -ENOENT;

  /* Traverse the directory */
  dp = opendir(root);
  if (dp == NULL)
    return (-errno);
  while ((entry = readdir(dp))) {

    /* Ignore special links. */
    if ((strcmp(".", entry->d_name) == 0) || (strcmp("..", entry->d_name) == 0))
        continue;

    /* Found it.  Return full path */
    if (strcmp(entry->d_name, name) == 0) {
      snprintf(path, maxlen, "%s/%s", root, entry->d_name);
      err = 0;
      break;
    }

    /* Process subdir */
    snprintf(dpath, sizeof(dpath), "%s/%s", root, entry->d_name);
    err = lstat(dpath, &stat);
    if (err != 0) {
      err = -errno;
      break;
    }

    /* Don't follow links. */
    if (S_ISDIR(stat.st_mode) && !S_ISLNK(stat.st_mode)) {
      /* Recurse into subdir */
      err = sdi_sysfs_find_attr(dpath, name, path, maxlen);
      if (err == 0)
        break;
    }
  }
  closedir(dp);
  return err;
}

/*
 * This function locates the named /sys fs attribute provided by
 * the specified chip's linux driver.  The open mode and attribute's
 * format are also supplied.
 *
 * The function returns a handle to the attribute which is used
 * to access the attribute in the future.
 */
t_std_error sdi_sysfs_open_attr(sdi_device_hdl_t chip, const char *name,
                                const sdi_sysfs_mode_t mode,
                                const sdi_sysfs_fmt_t fmt,
                                sdi_sysfs_attr_hdl_t *hdl)
{
  t_std_error rc = STD_ERR_OK;
  sdi_sysfs_attr_t *attr = NULL;
  char path[256];
  char apath[256];

  /* Get the root path to the chip's attributes. */
  sdi_sysfs_chip_to_path(chip, path, sizeof(path));

  /* Get the full path to the desired attribute. */
  rc = sdi_sysfs_find_attr(path, name, apath, sizeof(apath));
  if (rc) {
    goto exit_no_lock;
  }

  sdi_sysfs_mutex_lock();

  /* See if we already have a reference to this attribute. */
  TAILQ_FOREACH(attr, &sdi_sysfs_attrs, lru) {
    if (strcmp(attr->path, apath) == 0) {
      attr->ref++;
      goto exit;
    }
  }

  /* Create a new reference for this attribute. */
  attr = calloc(sizeof(*attr) + strlen(apath) + 1, 1);
  if (!attr) {
    rc = -EINVAL;
    goto exit;
  }
  strcpy(attr->path, apath);
  attr->ref = 1;
  attr->mode = mode;
  attr->fmt = fmt;
  attr->sfmt = sdi_sysfs_fmt_str[attr->fmt];

  /* Insert the attribute at the end of the LRU list of attributes. */
  TAILQ_INSERT_TAIL(&sdi_sysfs_attrs, attr, lru);

 exit:
  sdi_sysfs_mutex_unlock();  
 exit_no_lock:
  *hdl = attr;
  if (rc)
    rc = SDI_DEVICE_ERRCODE(-rc);
  return rc;
}

/* 
 * Read an int32 from a /sys fs attribute. 
 */
t_std_error sdi_sysfs_read_int32(sdi_sysfs_attr_hdl_t hdl, int32_t *val)
{
  sdi_sysfs_attr_t *attr = (sdi_sysfs_attr_t *)hdl;  
  STD_ASSERT(attr != NULL);
  STD_ASSERT(attr->fmt == SDI_SYSFS_FMT_INT);
  return sdi_sysfs_read_format(hdl, 1, attr->sfmt, val);
}

/* 
 * Write an int32 to a /sys fs attribute. 
 */
t_std_error sdi_sysfs_write_int32(sdi_sysfs_attr_hdl_t hdl, const int32_t val)
{
  sdi_sysfs_attr_t *attr = (sdi_sysfs_attr_t *)hdl;  
  STD_ASSERT(attr != NULL);
  STD_ASSERT(attr->fmt == SDI_SYSFS_FMT_INT);
  return sdi_sysfs_write_format(hdl, 1, attr->sfmt, val);
}
