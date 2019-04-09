/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017-2018 NXP
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
#include <rte_crypto.h>
#include <rte_security.h>

#include <caam_jr_config.h>
#include <caam_jr_hw_specific.h>
#include <caam_jr_pvt.h>
#include <caam_jr_log.h>

/* RTA header files */
#include <hw/desc/common.h>
#include <hw/desc/algo.h>
#include <hw/desc/ipsec.h>

/* Prefix path to sysfs directory where UIO device attributes are exported.
 * Path for UIO device X is /sys/class/uio/uioX
 */
#define SEC_UIO_DEVICE_SYS_ATTR_PATH    "/sys/class/uio"

/* Subfolder in sysfs where mapping attributes are exported
 * for each UIO device. Path for mapping Y for device X is:
 *      /sys/class/uio/uioX/maps/mapY
 */
#define SEC_UIO_DEVICE_SYS_MAP_ATTR     "maps/map"

/* Name of UIO device file prefix. Each UIO device will have a device file
 * /dev/uioX, where X is the minor device number.
 */
#define SEC_UIO_DEVICE_FILE_NAME    "/dev/uio"

/*
 * Name of UIO device. Each user space SEC job ring will have a corresponding
 * UIO device with the name sec-channelX, where X is the job ring id.
 * Maximum length is #SEC_UIO_MAX_DEVICE_NAME_LENGTH.
 *
 * @note  Must be kept in synch with SEC kernel driver
 * define #SEC_UIO_DEVICE_NAME !
 */
#define SEC_UIO_DEVICE_NAME     "fsl-jr"

/* Maximum length for the name of an UIO device file.
 * Device file name format is: /dev/uioX.
 */
#define SEC_UIO_MAX_DEVICE_FILE_NAME_LENGTH 30

/* Maximum length for the name of an attribute file for an UIO device.
 * Attribute files are exported in sysfs and have the name formatted as:
 *      /sys/class/uio/uioX/<attribute_file_name>
 */
#define SEC_UIO_MAX_ATTR_FILE_NAME  100

/* Command that is used by SEC user space driver and SEC kernel driver
 *  to signal a request from the former to the later to disable job DONE
 *  and error IRQs on a certain job ring.
 *  The configuration is done at SEC Controller's level.
 *  @note   Need to be kept in synch with #SEC_UIO_DISABLE_IRQ_CMD from
 *          linux/drivers/crypto/talitos.c !
 */
#define SEC_UIO_DISABLE_IRQ_CMD     0

/* Command that is used by SEC user space driver and SEC kernel driver
 *  to signal a request from the former to the later to enable job DONE
 *  and error IRQs on a certain job ring.
 *  The configuration is done at SEC Controller's level.
 *  @note   Need to be kept in synch with #SEC_UIO_ENABLE_IRQ_CMD from
 *          linux/drivers/crypto/talitos.c !
 */
#define SEC_UIO_ENABLE_IRQ_CMD      1

/** Command that is used by SEC user space driver and SEC kernel driver
 *  to signal a request from the former to the later to do a SEC engine reset.
 *  @note   Need to be kept in synch with #SEC_UIO_RESET_SEC_ENGINE_CMD from
 *          linux/drivers/crypto/talitos.c !
 */
#define SEC_UIO_RESET_SEC_ENGINE_CMD    3

/* The id for the mapping used to export SEC's registers to
 * user space through UIO devices.
 */
#define SEC_UIO_MAP_ID              0

static struct uio_job_ring g_uio_job_ring[MAX_SEC_JOB_RINGS];
static int g_uio_jr_num;

/** @brief Checks if a file name contains a certain substring.
 * If so, it extracts the number following the substring.
 * This function assumes a filename format of: [text][number].
 * @param [in]  filename    File name
 * @param [in]  match       String to match in file name
 * @param [out] number      The number extracted from filename
 *
 * @retval true if file name matches the criteria
 * @retval false if file name does not match the criteria
 */
static bool
file_name_match_extract(const char filename[], const char match[], int *number)
{
	char *substr = NULL;

	substr = strstr(filename, match);
	if (substr == NULL)
		return false;

	/* substring <match> was found in <filename>
	 * read number following <match> substring in <filename>
	 */
	if (sscanf(filename + strlen(match), "%d", number) <= 0)
		return false;

	return true;
}

/** @brief Reads first line from a file.
 * Composes file name as: root/subdir/filename
 *
 * @param [in]  root     Root path
 * @param [in]  subdir   Subdirectory name
 * @param [in]  filename File name
 * @param [out] line     The first line read from file.
 *
 * @retval 0 for succes
 * @retval other value for error
 */
static int
file_read_first_line(const char root[], const char subdir[],
		     const char filename[], char *line)
{
	char absolute_file_name[SEC_UIO_MAX_ATTR_FILE_NAME];
	int fd = 0, ret = 0;

	/*compose the file name: root/subdir/filename */
	memset(absolute_file_name, 0, sizeof(absolute_file_name));
	snprintf(absolute_file_name, SEC_UIO_MAX_ATTR_FILE_NAME,
		 "%s/%s/%s", root, subdir, filename);

	fd = open(absolute_file_name, O_RDONLY);
	SEC_ASSERT(fd > 0, fd, "Error opening file %s",
			absolute_file_name);

	/* read UIO device name from first line in file */
	ret = read(fd, line, SEC_UIO_MAX_DEVICE_FILE_NAME_LENGTH);
	close(fd);

	/* NULL-ify string */
	line[SEC_UIO_MAX_DEVICE_FILE_NAME_LENGTH - 1] = '\0';

	if (ret <= 0) {
		CAAM_JR_ERR("Error reading from file %s", absolute_file_name);
		return ret;
	}

	return 0;
}

/** @brief Uses UIO control to send commands to SEC kernel driver.
 * The mechanism is to write a command word into the file descriptor
 * that the user-space driver obtained for each user-space SEC job ring.
 * Both user-space driver and kernel driver must have the same understanding
 * about the command codes.
 *
 * @param [in]  UIO FD		    The UIO file descriptor
 * @param [in]  uio_command         Command word
 *
 * @retval Result of write operation on the job ring's UIO file descriptor.
 *         Should be sizeof(int) for success operations.
 *         Other values can be returned and used, if desired to add special
 *         meaning to return values, but this has to be programmed in SEC
 *         kernel driver as well. No special return values are used.
 */
static int
sec_uio_send_command(uint32_t uio_fd, int32_t uio_command)
{
	int ret;

	/* Use UIO file descriptor we have for this job ring.
	 * Writing a command code to this file descriptor will make the
	 * SEC kernel driver execute the desired command.
	 */
	ret = write(uio_fd, &uio_command, sizeof(int));
	return ret;
}

/** @brief Request to SEC kernel driver to enable interrupts for
 *         descriptor finished processing
 *  Use UIO to communicate with SEC kernel driver: write command
 *  value that indicates an IRQ enable action into UIO file descriptor
 *  of this job ring.
 *
 * @param [in]  uio_fd     Job Ring UIO File descriptor
 * @retval 0 for success
 * @retval -1 value for error
 */
uint32_t
caam_jr_enable_irqs(uint32_t uio_fd)
{
	int ret;

	/* Use UIO file descriptor we have for this job ring.
	 * Writing a command code to this file descriptor will make the
	 * SEC kernel driver enable DONE and Error IRQs for this job ring,
	 * at Controller level.
	 */
	ret = sec_uio_send_command(uio_fd, SEC_UIO_ENABLE_IRQ_CMD);
	SEC_ASSERT(ret == sizeof(int), -1,
		"Failed to request SEC engine to enable job done and "
		"error IRQs through UIO control. UIO FD %d. Reset SEC driver!",
		uio_fd);
	CAAM_JR_DEBUG("Enabled IRQs on jr with uio_fd %d", uio_fd);
	return 0;
}


/** @brief Request to SEC kernel driver to disable interrupts for descriptor
 *  finished processing
 *  Use UIO to communicate with SEC kernel driver: write command
 *  value that indicates an IRQ disable action into UIO file descriptor
 *  of this job ring.
 *
 * @param [in]  uio_fd    UIO File descripto
 * @retval 0 for success
 * @retval -1 value for error
 *
 */
uint32_t
caam_jr_disable_irqs(uint32_t uio_fd)
{
	int ret;

	/* Use UIO file descriptor we have for this job ring.
	 * Writing a command code to this file descriptor will make the
	 * SEC kernel driver disable IRQs for this job ring,
	 * at Controller level.
	 */

	ret = sec_uio_send_command(uio_fd, SEC_UIO_DISABLE_IRQ_CMD);
	SEC_ASSERT(ret == sizeof(int), -1,
		"Failed to request SEC engine to disable job done and "
		"IRQs through UIO control. UIO_FD %d Reset SEC driver!",
		uio_fd);
	CAAM_JR_DEBUG("Disabled IRQs on jr with uio_fd %d", uio_fd);
	return 0;
}

/** @brief Maps register range assigned for a job ring.
 *
 * @param [in] uio_device_fd    UIO device file descriptor
 * @param [in] uio_device_id    UIO device id
 * @param [in] uio_map_id       UIO allows maximum 5 different mapping for
				each device. Maps start with id 0.
 * @param [out] map_size        Map size.
 * @retval  NULL if failed to map registers
 * @retval  Virtual address for mapped register address range
 */
static void *
uio_map_registers(int uio_device_fd, int uio_device_id,
		  int uio_map_id, int *map_size)
{
	void *mapped_address = NULL;
	unsigned int uio_map_size = 0;
	char uio_sys_root[SEC_UIO_MAX_ATTR_FILE_NAME];
	char uio_sys_map_subdir[SEC_UIO_MAX_ATTR_FILE_NAME];
	char uio_map_size_str[32];
	int ret = 0;

	/* compose the file name: root/subdir/filename */
	memset(uio_sys_root, 0, sizeof(uio_sys_root));
	memset(uio_sys_map_subdir, 0, sizeof(uio_sys_map_subdir));
	memset(uio_map_size_str, 0, sizeof(uio_map_size_str));

	/* Compose string: /sys/class/uio/uioX */
	snprintf(uio_sys_root, sizeof(uio_sys_root), "%s/%s%d",
			SEC_UIO_DEVICE_SYS_ATTR_PATH, "uio", uio_device_id);
	/* Compose string: maps/mapY */
	snprintf(uio_sys_map_subdir, sizeof(uio_sys_map_subdir), "%s%d",
			SEC_UIO_DEVICE_SYS_MAP_ATTR, uio_map_id);

	/* Read first (and only) line from file
	 * /sys/class/uio/uioX/maps/mapY/size
	 */
	ret = file_read_first_line(uio_sys_root, uio_sys_map_subdir,
				 "size", uio_map_size_str);
	SEC_ASSERT(ret == 0, NULL, "file_read_first_line() failed");

	/* Read mapping size, expressed in hexa(base 16) */
	uio_map_size = strtol(uio_map_size_str, NULL, 16);

	/* Map the region in user space */
	mapped_address = mmap(0, /*dynamically choose virtual address */
		uio_map_size, PROT_READ | PROT_WRITE,
		MAP_SHARED, uio_device_fd, 0);
	/* offset = 0 because UIO device has only one mapping
	 * for the entire SEC register memory
	 */
	if (mapped_address == MAP_FAILED) {
		CAAM_JR_ERR(
			"Failed to map registers! errno = %d job ring fd  = %d,"
			"uio device id = %d, uio map id = %d", errno,
			uio_device_fd, uio_device_id, uio_map_id);
		return NULL;
	}

	/*
	 * Save the map size to use it later on for munmap-ing.
	 */
	*map_size = uio_map_size;

	CAAM_JR_INFO("UIO dev[%d] mapped region [id =%d] size 0x%x at %p",
		uio_device_id, uio_map_id, uio_map_size, mapped_address);

	return mapped_address;
}

void
free_job_ring(uint32_t uio_fd)
{
	struct uio_job_ring *job_ring = NULL;
	int i;

	if (!uio_fd)
		return;

	for (i = 0; i < MAX_SEC_JOB_RINGS; i++) {
		if (g_uio_job_ring[i].uio_fd == uio_fd) {
			job_ring = &g_uio_job_ring[i];
			break;
		}
	}

	if (job_ring == NULL) {
		CAAM_JR_ERR("JR not available for fd = %x\n", uio_fd);
		return;
	}

	/* Open device file */
	CAAM_JR_INFO("Closed device file for job ring %d , fd = %d",
			job_ring->jr_id, job_ring->uio_fd);
	close(job_ring->uio_fd);
	g_uio_jr_num--;
	job_ring->uio_fd = 0;
	if (job_ring->register_base_addr == NULL)
		return;

	/* Unmap the PCI memory resource of device */
	if (munmap(job_ring->register_base_addr, job_ring->map_size)) {
		CAAM_JR_INFO("cannot munmap(%p, 0x%lx): %s",
			job_ring->register_base_addr,
			(unsigned long)job_ring->map_size, strerror(errno));
	} else
		CAAM_JR_DEBUG("JR UIO memory is unmapped");

	job_ring->register_base_addr = NULL;
}

struct
uio_job_ring *config_job_ring(void)
{
	char uio_device_file_name[32];
	struct uio_job_ring *job_ring = NULL;
	int i;

	for (i = 0; i < MAX_SEC_JOB_RINGS; i++) {
		if (g_uio_job_ring[i].uio_fd == 0) {
			job_ring = &g_uio_job_ring[i];
			g_uio_jr_num++;
			break;
		}
	}

	if (job_ring == NULL) {
		CAAM_JR_ERR("No free job ring\n");
		return NULL;
	}

	/* Find UIO device created by SEC kernel driver for this job ring. */
	memset(uio_device_file_name, 0, sizeof(uio_device_file_name));
	snprintf(uio_device_file_name, sizeof(uio_device_file_name), "%s%d",
			SEC_UIO_DEVICE_FILE_NAME, job_ring->uio_minor_number);

	/* Open device file */
	job_ring->uio_fd = open(uio_device_file_name, O_RDWR);
	SEC_ASSERT(job_ring->uio_fd > 0, NULL,
		"Failed to open UIO device file for job ring %d",
		job_ring->jr_id);

	CAAM_JR_INFO("Open device(%s) file for job ring=%d , uio_fd = %d",
		uio_device_file_name, job_ring->jr_id, job_ring->uio_fd);

	ASSERT(job_ring->register_base_addr == NULL);
	job_ring->register_base_addr = uio_map_registers(
			job_ring->uio_fd, job_ring->uio_minor_number,
			SEC_UIO_MAP_ID, &job_ring->map_size);

	SEC_ASSERT(job_ring->register_base_addr != NULL, NULL,
		"Failed to map SEC registers");
	return job_ring;
}

int
sec_configure(void)
{
	char uio_name[32];
	int config_jr_no = 0, jr_id = -1;
	int uio_minor_number = -1;
	int ret;
	DIR *d = NULL;
	struct dirent *dir;

	d = opendir(SEC_UIO_DEVICE_SYS_ATTR_PATH);
	if (d == NULL) {
		printf("\nError opening directory '%s': %s\n",
			SEC_UIO_DEVICE_SYS_ATTR_PATH, strerror(errno));
		return -1;
	}

	/* Iterate through all subdirs */
	while ((dir = readdir(d)) != NULL) {
		if (!strncmp(dir->d_name, ".", 1) ||
				!strncmp(dir->d_name, "..", 2))
			continue;

		if (file_name_match_extract
			(dir->d_name, "uio", &uio_minor_number)) {
		/*
		 * Open file uioX/name and read first line which contains
		 * the name for the device. Based on the name check if this
		 * UIO device is UIO device for job ring with id jr_id.
		 */
			memset(uio_name, 0, sizeof(uio_name));
			ret = file_read_first_line(SEC_UIO_DEVICE_SYS_ATTR_PATH,
					dir->d_name, "name", uio_name);
			CAAM_JR_INFO("sec device uio name: %s", uio_name);
			if (ret != 0) {
				CAAM_JR_ERR("file_read_first_line failed\n");
				closedir(d);
				return -1;
			}

			if (file_name_match_extract(uio_name,
						SEC_UIO_DEVICE_NAME,
						&jr_id)) {
				g_uio_job_ring[config_jr_no].jr_id = jr_id;
				g_uio_job_ring[config_jr_no].uio_minor_number =
							uio_minor_number;
				CAAM_JR_INFO("Detected logical JRID:%d", jr_id);
				config_jr_no++;

				/* todo  find the actual ring id
				 * OF_FULLNAME=/soc/crypto@1700000/jr@20000
				 */
			}
		}
	}
	closedir(d);

	if (config_jr_no == 0) {
		CAAM_JR_ERR("! No SEC Job Rings assigned for userspace usage!");
		return 0;
	}
	CAAM_JR_INFO("Total JR detected =%d", config_jr_no);
	return config_jr_no;
}

int
sec_cleanup(void)
{
	int i;
	struct uio_job_ring *job_ring;

	for (i = 0; i < g_uio_jr_num; i++) {
		job_ring = &g_uio_job_ring[i];
		/* munmap SEC's register memory */
		if (job_ring->register_base_addr) {
			munmap(job_ring->register_base_addr,
				job_ring->map_size);
			job_ring->register_base_addr = NULL;
		}
		/* I need to close the fd after shutdown UIO commands need to be
		 * sent using the fd
		 */
		if (job_ring->uio_fd != 0) {
			CAAM_JR_INFO(
			"Closed device file for job ring %d , fd = %d",
			job_ring->jr_id, job_ring->uio_fd);
			close(job_ring->uio_fd);
		}
	}
	return 0;
}
