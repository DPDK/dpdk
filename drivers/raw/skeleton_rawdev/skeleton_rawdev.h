/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 NXP
 */

#ifndef __SKELETON_RAWDEV_H__
#define __SKELETON_RAWDEV_H__

#include <rte_rawdev.h>

extern int skeleton_pmd_logtype;

#define SKELETON_PMD_LOG(level, fmt, args...) \
	rte_log(RTE_LOG_ ## level, skeleton_pmd_logtype, "%s(): " fmt "\n", \
		__func__, ##args)

#define SKELETON_PMD_FUNC_TRACE() SKELETON_PMD_LOG(DEBUG, ">>")

#define SKELETON_PMD_DEBUG(fmt, args...) \
	SKELETON_PMD_LOG(DEBUG, fmt, ## args)
#define SKELETON_PMD_INFO(fmt, args...) \
	SKELETON_PMD_LOG(INFO, fmt, ## args)
#define SKELETON_PMD_ERR(fmt, args...) \
	SKELETON_PMD_LOG(ERR, fmt, ## args)
#define SKELETON_PMD_WARN(fmt, args...) \
	SKELETON_PMD_LOG(WARNING, fmt, ## args)
/* Macros for self test application */
#define SKELETON_TEST_INFO	SKELETON_PMD_INFO
#define SKELETON_TEST_DEBUG	SKELETON_PMD_DEBUG
#define SKELETON_TEST_ERR	SKELETON_PMD_ERR
#define SKELETON_TEST_WARN	SKELETON_PMD_WARN

#define SKELETON_SELFTEST_ARG   ("selftest")

#define SKELETON_VENDOR_ID 0x10
#define SKELETON_DEVICE_ID 0x01

#define SKELETON_MAJOR_VER 1
#define SKELETON_MINOR_VER 0
#define SKELETON_SUB_VER   0

#define SKELETON_MAX_QUEUES 1

enum skeleton_firmware_state {
	SKELETON_FW_READY,
	SKELETON_FW_LOADED,
	SKELETON_FW_ERROR
};

enum skeleton_device_state {
	SKELETON_DEV_RUNNING,
	SKELETON_DEV_STOPPED
};

enum skeleton_queue_state {
	SKELETON_QUEUE_DETACH,
	SKELETON_QUEUE_ATTACH
};

#define SKELETON_QUEUE_DEF_DEPTH 10
#define SKELETON_QUEUE_MAX_DEPTH 25

struct skeleton_firmware_version_info {
	uint8_t major;
	uint8_t minor;
	uint8_t subrel;
};

struct skeleton_firmware {
	/**< Device firmware information */
	struct skeleton_firmware_version_info firmware_version;
	/**< Device state */
	enum skeleton_firmware_state firmware_state;

};

#define SKELETON_MAX_ATTRIBUTES 10
#define SKELETON_ATTRIBUTE_NAME_MAX 20

struct skeleton_rawdev_attributes {
	/**< Name of the attribute */
	char *name;
	/**< Value or reference of value of attribute */
	uint64_t value;
};

/**< Device supports firmware loading/unloading */
#define SKELETON_CAPA_FW_LOAD	0x0001
/**< Device supports firmware reset */
#define SKELETON_CAPA_FW_RESET  0x0002
/**< Device support queue based communication */
#define SKELETON_CAPA_QUEUES    0x0004
/**< Default Capabilities: FW_LOAD, FW_RESET, QUEUES */
#define SKELETON_DEFAULT_CAPA   0x7

struct skeleton_rawdev_queue {
	uint8_t state;
	uint32_t depth;
};

struct skeleton_rawdev {
	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t num_queues;
	/**< One of SKELETON_CAPA_* */
	uint16_t capabilities;
	/**< State of device; linked to firmware state */
	enum skeleton_device_state device_state;
	/**< Firmware configuration */
	struct skeleton_firmware fw;
	/**< Collection of all communication channels - which can be referred
	 *  to as queues.
	 */
	struct skeleton_rawdev_queue queues[SKELETON_MAX_QUEUES];
	/**< Global table containing various pre-defined and user-defined
	 * attributes.
	 */
	struct skeleton_rawdev_attributes attr[SKELETON_MAX_ATTRIBUTES];
	struct rte_device *device;
};

struct skeleton_rawdev_conf {
	uint16_t num_queues;
	unsigned int capabilities;
	enum skeleton_device_state device_state;
	enum skeleton_firmware_state firmware_state;
};

static inline struct skeleton_rawdev *
skeleton_rawdev_get_priv(const struct rte_rawdev *rawdev)
{
	return rawdev->dev_private;
}

int test_rawdev_skeldev(void);

#endif /* __SKELETON_RAWDEV_H__ */
