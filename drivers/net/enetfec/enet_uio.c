/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021,2024-2025 NXP
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include "enet_pmd_logs.h"
#include "enet_uio.h"

static struct uio_job enetfec_uio_job;
static int enetfec_count;

/** @brief Checks if a file name contains a certain substring.
 * This function assumes a filename format of: [text][number].
 * @param [in]  filename    File name
 * @param [in]  match       String to match in file name
 *
 * @retval true if file name matches the criteria
 * @retval false if file name does not match the criteria
 */
static bool
file_name_match_extract(const char filename[], const char match[])
{
	char *substr = NULL;

	substr = strstr(filename, match);
	if (substr == NULL)
		return false;

	return true;
}

/*
 * @brief Reads first line from a file.
 * Composes file name as: root/subdir/filename
 *
 * @param [in]  root     Root path
 * @param [in]  subdir   Subdirectory name
 * @param [in]  filename File name
 * @param [out] line     The first line read from file.
 *
 * @retval 0 for success
 * @retval other value for error
 */
static int
file_read_first_line(const char root[], const char subdir[],
			const char filename[], char *line)
{
	char absolute_file_name[FEC_UIO_MAX_ATTR_FILE_NAME];
	int fd = 0, ret = 0;

	/*compose the file name: root/subdir/filename */
	memset(absolute_file_name, 0, sizeof(absolute_file_name));
	snprintf(absolute_file_name, FEC_UIO_MAX_ATTR_FILE_NAME,
		"%s/%s/%s", root, subdir, filename);

	fd = open(absolute_file_name, O_RDONLY);
	if (fd < 0) {
		ENETFEC_PMD_ERR("Error opening file %s", absolute_file_name);
		return fd;
	}

	/* read UIO device name from first line in file */
	ret = read(fd, line, FEC_UIO_MAX_DEVICE_FILE_NAME_LENGTH);
	if (ret <= 0) {
		ENETFEC_PMD_ERR("Error reading file %s", absolute_file_name);
		close(fd);
		return ret;
	}
	close(fd);

	/* NULL-ify string */
	line[ret] = '\0';

	return 0;
}

/*
 * @brief Maps rx-tx bd range assigned for a bd ring.
 *
 * @param [in] uio_device_fd    UIO device file descriptor
 * @param [in] uio_device_id    UIO device id
 * @param [in] uio_map_id       UIO allows maximum 5 different mapping for
				each device. Maps start with id 0.
 * @param [out] map_size        Map size.
 * @param [out] map_addr	Map physical address
 *
 * @retval  NULL if failed to map registers
 * @retval  Virtual address for mapped register address range
 */
static void *
uio_map_mem(int uio_device_fd, int uio_device_id,
		int uio_map_id, int *map_size, uint64_t *map_addr)
{
	void *mapped_address = NULL;
	unsigned int uio_map_size = 0;
	unsigned int uio_map_p_addr = 0;
	char uio_sys_root[FEC_UIO_MAX_ATTR_FILE_NAME];
	char uio_sys_map_subdir[FEC_UIO_MAX_ATTR_FILE_NAME];
	char uio_map_size_str[FEC_UIO_MAX_DEVICE_FILE_NAME_LENGTH + 1];
	char uio_map_p_addr_str[32];
	int ret = 0;

	/* compose the file name: root/subdir/filename */
	memset(uio_sys_root, 0, sizeof(uio_sys_root));
	memset(uio_sys_map_subdir, 0, sizeof(uio_sys_map_subdir));
	memset(uio_map_size_str, 0, sizeof(uio_map_size_str));
	memset(uio_map_p_addr_str, 0, sizeof(uio_map_p_addr_str));

	/* Compose string: /sys/class/uio/uioX */
	snprintf(uio_sys_root, sizeof(uio_sys_root), "%s/%s%d",
			FEC_UIO_DEVICE_SYS_ATTR_PATH, "uio", uio_device_id);
	/* Compose string: maps/mapY */
	snprintf(uio_sys_map_subdir, sizeof(uio_sys_map_subdir), "%s%d",
			FEC_UIO_DEVICE_SYS_MAP_ATTR, uio_map_id);

	/* Read first (and only) line from file
	 * /sys/class/uio/uioX/maps/mapY/size
	 */
	ret = file_read_first_line(uio_sys_root, uio_sys_map_subdir,
				"size", uio_map_size_str);
	if (ret < 0) {
		ENETFEC_PMD_ERR("file_read_first_line() failed");
		return NULL;
	}
	ret = file_read_first_line(uio_sys_root, uio_sys_map_subdir,
				"addr", uio_map_p_addr_str);
	if (ret < 0) {
		ENETFEC_PMD_ERR("file_read_first_line() failed");
		return NULL;
	}
	/* Read mapping size and physical address expressed in hexa(base 16) */
	uio_map_size = strtol(uio_map_size_str, NULL, 16);
	if (uio_map_size <= 0 || uio_map_size > INT_MAX) {
		ENETFEC_PMD_ERR("Invalid mapping size: %u.", uio_map_size);
		return NULL;
	}
	uio_map_p_addr = strtol(uio_map_p_addr_str, NULL, 16);

	if (uio_map_id == 0) {
		/* Map the register address in user space when map_id is 0 */
		mapped_address = mmap(0 /*dynamically choose virtual address */,
				uio_map_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, uio_device_fd, 0);
	} else {
		/* Map the BD memory in user space */
		mapped_address = mmap(NULL, uio_map_size,
				PROT_READ | PROT_WRITE,
				MAP_SHARED, uio_device_fd, (1 * MAP_PAGE_SIZE));
	}

	if (mapped_address == MAP_FAILED) {
		ENETFEC_PMD_ERR("Failed to map! errno = %d uio job fd = %d,"
			"uio device id = %d, uio map id = %d", errno,
			uio_device_fd, uio_device_id, uio_map_id);
		return NULL;
	}

	/* Save the map size to use it later on for munmap-ing */
	*map_size = uio_map_size;
	*map_addr = uio_map_p_addr;
	ENETFEC_PMD_INFO("UIO dev[%d] mapped region [id =%d] size 0x%x at %p",
		uio_device_id, uio_map_id, uio_map_size, mapped_address);

	return mapped_address;
}

int
config_enetfec_uio(struct enetfec_private *fep)
{
	char uio_device_file_name[32];
	struct uio_job *uio_job = NULL;

	/* Mapping is done only one time */
	if (enetfec_count > 0) {
		ENETFEC_PMD_INFO("Mapped!");
		return 0;
	}

	uio_job = &enetfec_uio_job;

	/* Find UIO device created by ENETFEC-UIO kernel driver */
	memset(uio_device_file_name, 0, sizeof(uio_device_file_name));
	snprintf(uio_device_file_name, sizeof(uio_device_file_name), "%s%d",
			FEC_UIO_DEVICE_FILE_NAME, uio_job->uio_minor_number);

	/* Open device file */
	uio_job->uio_fd = open(uio_device_file_name, O_RDWR);
	if (uio_job->uio_fd < 0) {
		ENETFEC_PMD_WARN("Unable to open ENETFEC_UIO file");
		return -1;
	}

	ENETFEC_PMD_INFO("US_UIO: Open device(%s) file with uio_fd = %d",
			uio_device_file_name, uio_job->uio_fd);

	fep->hw_baseaddr_v = uio_map_mem(uio_job->uio_fd,
		uio_job->uio_minor_number, FEC_UIO_REG_MAP_ID,
		&uio_job->map_size, &uio_job->map_addr);
	if (fep->hw_baseaddr_v == NULL)
		return -ENOMEM;
	fep->hw_baseaddr_p = uio_job->map_addr;
	fep->reg_size = uio_job->map_size;

	fep->bd_addr_v = uio_map_mem(uio_job->uio_fd,
		uio_job->uio_minor_number, FEC_UIO_BD_MAP_ID,
		&uio_job->map_size, &uio_job->map_addr);
	if (fep->hw_baseaddr_v == NULL)
		return -ENOMEM;
	fep->bd_addr_p = (uint32_t)uio_job->map_addr;
	fep->bd_size = uio_job->map_size;

	enetfec_count++;

	return 0;
}

int
enetfec_configure(void)
{
	char uio_name[32];
	int uio_minor_number = -1;
	int ret;
	DIR *d = NULL;
	struct dirent *dir;

	d = opendir(FEC_UIO_DEVICE_SYS_ATTR_PATH);
	if (d == NULL) {
		ENETFEC_PMD_ERR("Error opening directory '%s': %s",
			FEC_UIO_DEVICE_SYS_ATTR_PATH, strerror(errno));
		return -1;
	}

	/* Iterate through all subdirs */
	while ((dir = readdir(d)) != NULL) {
		if (!strncmp(dir->d_name, ".", 1) ||
				!strncmp(dir->d_name, "..", 2))
			continue;

		if (file_name_match_extract(dir->d_name, "uio")) {
			/*
			 * As substring <uio> was found in <d_name>
			 * read number following <uio> substring in <d_name>
			 */
			ret = sscanf(dir->d_name + strlen("uio"), "%d",
							&uio_minor_number);
			if (ret < 0)
				ENETFEC_PMD_ERR("Error: not find minor number");
			/*
			 * Open file uioX/name and read first line which
			 * contains the name for the device. Based on the
			 * name check if this UIO device is for enetfec.
			 */
			memset(uio_name, 0, sizeof(uio_name));
			ret = file_read_first_line(FEC_UIO_DEVICE_SYS_ATTR_PATH,
					dir->d_name, "name", uio_name);
			if (ret != 0) {
				ENETFEC_PMD_INFO("file_read_first_line failed");
				closedir(d);
				return -1;
			}

			if (file_name_match_extract(uio_name,
						FEC_UIO_DEVICE_NAME)) {
				enetfec_uio_job.uio_minor_number =
							uio_minor_number;
				ENETFEC_PMD_INFO("enetfec device uio name: %s",
						 uio_name);
			}
		}
	}
	closedir(d);
	return 0;
}

void
enetfec_cleanup(struct enetfec_private *fep)
{
	munmap(fep->hw_baseaddr_v, fep->cbus_size);
}
