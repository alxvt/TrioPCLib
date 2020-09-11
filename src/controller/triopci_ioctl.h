/*
 *      triopci_driver.c  --  PCI class driver
 *
 *      Copyright (C) 2008
 *          Simon Martin <smartin@milliways.cl>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *

        Modifications
        0.00 5/17/2008 created
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <linux/ioctl.h>

/*---------------------------------------------------------------------
  -- project includes (import)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes (export)
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
typedef struct TrioValue_t
{
    unsigned long start, stop;
    float *data;
} TrioValue_t;

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- local variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- private functions
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- public functions
  ---------------------------------------------------------------------*/

#define TRIO_IOCTL_MAGIC 0xc0

#define IOCTL_TRIO_GET_VR                       _IOWR(TRIO_IOCTL_MAGIC, 0, TrioValue_t)
#define IOCTL_TRIO_SET_VR                        _IOR(TRIO_IOCTL_MAGIC, 1, TrioValue_t)
#define IOCTL_TRIO_GET_TABLE                    _IOWR(TRIO_IOCTL_MAGIC, 2, TrioValue_t)
#define IOCTL_TRIO_SET_TABLE                     _IOR(TRIO_IOCTL_MAGIC, 3, TrioValue_t)
#define IOCTL_TRIO_LOCK_DPR                       _IO(TRIO_IOCTL_MAGIC, 4)
#define IOCTL_TRIO_QUERY_DPR                    _IOWR(TRIO_IOCTL_MAGIC, 5, int)
#define IOCTL_TRIO_CANCEL                         _IO(TRIO_IOCTL_MAGIC, 6)
#define IOCTL_TRIO_RESET                          _IO(TRIO_IOCTL_MAGIC, 7)
#define IOCTL_TRIO_GET_MPE_WRITE_FIFO_FLAG       _IOR(TRIO_IOCTL_MAGIC, 8, int)
#define IOCTL_TRIO_GET_MPE_READ_FIFO_FLAG        _IOR(TRIO_IOCTL_MAGIC, 9, int)

#define TRIO_IOCTL_MAX 10

