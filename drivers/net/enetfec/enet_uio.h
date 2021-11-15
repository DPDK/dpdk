/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 NXP
 */

#include "enet_ethdev.h"

/* Prefix path to sysfs directory where UIO device attributes are exported.
 * Path for UIO device X is /sys/class/uio/uioX
 */
#define FEC_UIO_DEVICE_SYS_ATTR_PATH	"/sys/class/uio"

/* Subfolder in sysfs where mapping attributes are exported
 * for each UIO device. Path for mapping Y for device X is:
 * /sys/class/uio/uioX/maps/mapY
 */
#define FEC_UIO_DEVICE_SYS_MAP_ATTR	"maps/map"

/* Name of UIO device file prefix. Each UIO device will have a device file
 * /dev/uioX, where X is the minor device number.
 */
#define FEC_UIO_DEVICE_FILE_NAME	"/dev/uio"
/*
 * Name of UIO device. User space FEC will have a corresponding
 * UIO device.
 * Maximum length is #FEC_UIO_MAX_DEVICE_NAME_LENGTH.
 *
 * @note  Must be kept in sync with FEC kernel driver
 * define #FEC_UIO_DEVICE_NAME !
 */
#define FEC_UIO_DEVICE_NAME     "imx-fec-uio"

/* Maximum length for the name of an UIO device file.
 * Device file name format is: /dev/uioX.
 */
#define FEC_UIO_MAX_DEVICE_FILE_NAME_LENGTH	30

/* Maximum length for the name of an attribute file for an UIO device.
 * Attribute files are exported in sysfs and have the name formatted as:
 * /sys/class/uio/uioX/<attribute_file_name>
 */
#define FEC_UIO_MAX_ATTR_FILE_NAME	100

/* The id for the mapping used to export ENETFEC registers and BD memory to
 * user space through UIO device.
 */
#define FEC_UIO_REG_MAP_ID		0
#define FEC_UIO_BD_MAP_ID		1

#define MAP_PAGE_SIZE			4096

struct uio_job {
	uint32_t fec_id;
	int uio_fd;
	void *bd_start_addr;
	void *register_base_addr;
	int map_size;
	uint64_t map_addr;
	int uio_minor_number;
};

int enetfec_configure(void);
int config_enetfec_uio(struct enetfec_private *fep);
void enetfec_uio_init(void);
void enetfec_cleanup(struct enetfec_private *fep);
