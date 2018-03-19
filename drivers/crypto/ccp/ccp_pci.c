/*   SPDX-License-Identifier: BSD-3-Clause
 *   Copyright(c) 2018 Advanced Micro Devices, Inc. All rights reserved.
 */

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rte_string_fns.h>

#include "ccp_pci.h"

static const char * const uio_module_names[] = {
	"igb_uio",
	"uio_pci_generic",
};

int
ccp_check_pci_uio_module(void)
{
	FILE *fp;
	int i;
	char buf[BUFSIZ];

	fp = fopen(PROC_MODULES, "r");
	if (fp == NULL)
		return -1;
	i = 0;
	while (uio_module_names[i] != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			if (!strncmp(buf, uio_module_names[i],
				     strlen(uio_module_names[i])))
				return i;
		}
		i++;
		rewind(fp);
	}
	printf("Insert igb_uio or uio_pci_generic kernel module(s)");
	return -1;/* uio not inserted */
}

/*
 * split up a pci address into its constituent parts.
 */
int
ccp_parse_pci_addr_format(const char *buf, int bufsize, uint16_t *domain,
			  uint8_t *bus, uint8_t *devid, uint8_t *function)
{
	/* first split on ':' */
	union splitaddr {
		struct {
			char *domain;
			char *bus;
			char *devid;
			char *function;
		};
		char *str[PCI_FMT_NVAL];
		/* last element-separator is "." not ":" */
	} splitaddr;

	char *buf_copy = strndup(buf, bufsize);

	if (buf_copy == NULL)
		return -1;

	if (rte_strsplit(buf_copy, bufsize, splitaddr.str, PCI_FMT_NVAL, ':')
			!= PCI_FMT_NVAL - 1)
		goto error;
	/* final split is on '.' between devid and function */
	splitaddr.function = strchr(splitaddr.devid, '.');
	if (splitaddr.function == NULL)
		goto error;
	*splitaddr.function++ = '\0';

	/* now convert to int values */
	errno = 0;
	*domain = (uint8_t)strtoul(splitaddr.domain, NULL, 16);
	*bus = (uint8_t)strtoul(splitaddr.bus, NULL, 16);
	*devid = (uint8_t)strtoul(splitaddr.devid, NULL, 16);
	*function = (uint8_t)strtoul(splitaddr.function, NULL, 10);
	if (errno != 0)
		goto error;

	free(buf_copy); /* free the copy made with strdup */
	return 0;
error:
	free(buf_copy);
	return -1;
}

int
ccp_pci_parse_sysfs_value(const char *filename, unsigned long *val)
{
	FILE *f;
	char buf[BUFSIZ];
	char *end = NULL;

	f = fopen(filename, "r");
	if (f == NULL)
		return -1;
	if (fgets(buf, sizeof(buf), f) == NULL) {
		fclose(f);
		return -1;
	}
	*val = strtoul(buf, &end, 0);
	if ((buf[0] == '\0') || (end == NULL) || (*end != '\n')) {
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}

/** IO resource type: */
#define IORESOURCE_IO         0x00000100
#define IORESOURCE_MEM        0x00000200

/* parse one line of the "resource" sysfs file (note that the 'line'
 * string is modified)
 */
static int
ccp_pci_parse_one_sysfs_resource(char *line, size_t len, uint64_t *phys_addr,
				 uint64_t *end_addr, uint64_t *flags)
{
	union pci_resource_info {
		struct {
			char *phys_addr;
			char *end_addr;
			char *flags;
		};
		char *ptrs[PCI_RESOURCE_FMT_NVAL];
	} res_info;

	if (rte_strsplit(line, len, res_info.ptrs, 3, ' ') != 3)
		return -1;
	errno = 0;
	*phys_addr = strtoull(res_info.phys_addr, NULL, 16);
	*end_addr = strtoull(res_info.end_addr, NULL, 16);
	*flags = strtoull(res_info.flags, NULL, 16);
	if (errno != 0)
		return -1;

	return 0;
}

/* parse the "resource" sysfs file */
int
ccp_pci_parse_sysfs_resource(const char *filename, struct rte_pci_device *dev)
{
	FILE *fp;
	char buf[BUFSIZ];
	int i;
	uint64_t phys_addr, end_addr, flags;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return -1;

	for (i = 0; i < PCI_MAX_RESOURCE; i++) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			goto error;
		if (ccp_pci_parse_one_sysfs_resource(buf, sizeof(buf),
				&phys_addr, &end_addr, &flags) < 0)
			goto error;

		if (flags & IORESOURCE_MEM) {
			dev->mem_resource[i].phys_addr = phys_addr;
			dev->mem_resource[i].len = end_addr - phys_addr + 1;
			/* not mapped for now */
			dev->mem_resource[i].addr = NULL;
		}
	}
	fclose(fp);
	return 0;

error:
	fclose(fp);
	return -1;
}

int
ccp_find_uio_devname(const char *dirname)
{

	DIR *dir;
	struct dirent *e;
	char dirname_uio[PATH_MAX];
	unsigned int uio_num;
	int ret = -1;

	/* depending on kernel version, uio can be located in uio/uioX
	 * or uio:uioX
	 */
	snprintf(dirname_uio, sizeof(dirname_uio), "%s/uio", dirname);
	dir = opendir(dirname_uio);
	if (dir == NULL) {
	/* retry with the parent directory might be different kernel version*/
		dir = opendir(dirname);
		if (dir == NULL)
			return -1;
	}

	/* take the first file starting with "uio" */
	while ((e = readdir(dir)) != NULL) {
		/* format could be uio%d ...*/
		int shortprefix_len = sizeof("uio") - 1;
		/* ... or uio:uio%d */
		int longprefix_len = sizeof("uio:uio") - 1;
		char *endptr;

		if (strncmp(e->d_name, "uio", 3) != 0)
			continue;

		/* first try uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + shortprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + shortprefix_len)) {
			ret = uio_num;
			break;
		}

		/* then try uio:uio%d */
		errno = 0;
		uio_num = strtoull(e->d_name + longprefix_len, &endptr, 10);
		if (errno == 0 && endptr != (e->d_name + longprefix_len)) {
			ret = uio_num;
			break;
		}
	}
	closedir(dir);
	return ret;


}
