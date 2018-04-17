/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <rte_string_fns.h>
#include <rte_log.h>
#include <rte_compat.h>
#include <rte_dev.h>
#include <rte_malloc.h>
#include <rte_interrupts.h>
#include <rte_alarm.h>

#include "eal_private.h"

static struct rte_intr_handle intr_handle = {.fd = -1 };
static bool monitor_started;

#define EAL_UEV_MSG_LEN 4096
#define EAL_UEV_MSG_ELEM_LEN 128

static void dev_uev_handler(__rte_unused void *param);

/* identify the system layer which reports this event. */
enum eal_dev_event_subsystem {
	EAL_DEV_EVENT_SUBSYSTEM_PCI, /* PCI bus device event */
	EAL_DEV_EVENT_SUBSYSTEM_UIO, /* UIO driver device event */
	EAL_DEV_EVENT_SUBSYSTEM_VFIO, /* VFIO driver device event */
	EAL_DEV_EVENT_SUBSYSTEM_MAX
};

static int
dev_uev_socket_fd_create(void)
{
	struct sockaddr_nl addr;
	int ret;

	intr_handle.fd = socket(PF_NETLINK, SOCK_RAW | SOCK_CLOEXEC |
			SOCK_NONBLOCK,
			NETLINK_KOBJECT_UEVENT);
	if (intr_handle.fd < 0) {
		RTE_LOG(ERR, EAL, "create uevent fd failed.\n");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_pid = 0;
	addr.nl_groups = 0xffffffff;

	ret = bind(intr_handle.fd, (struct sockaddr *) &addr, sizeof(addr));
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Failed to bind uevent socket.\n");
		goto err;
	}

	return 0;
err:
	close(intr_handle.fd);
	intr_handle.fd = -1;
	return ret;
}

static int
dev_uev_parse(const char *buf, struct rte_dev_event *event, int length)
{
	char action[EAL_UEV_MSG_ELEM_LEN];
	char subsystem[EAL_UEV_MSG_ELEM_LEN];
	char pci_slot_name[EAL_UEV_MSG_ELEM_LEN];
	int i = 0;

	memset(action, 0, EAL_UEV_MSG_ELEM_LEN);
	memset(subsystem, 0, EAL_UEV_MSG_ELEM_LEN);
	memset(pci_slot_name, 0, EAL_UEV_MSG_ELEM_LEN);

	while (i < length) {
		for (; i < length; i++) {
			if (*buf)
				break;
			buf++;
		}
		/**
		 * check device uevent from kernel side, no need to check
		 * uevent from udev.
		 */
		if (!strncmp(buf, "libudev", 7)) {
			buf += 7;
			i += 7;
			return -1;
		}
		if (!strncmp(buf, "ACTION=", 7)) {
			buf += 7;
			i += 7;
			strlcpy(action, buf, sizeof(action));
		} else if (!strncmp(buf, "SUBSYSTEM=", 10)) {
			buf += 10;
			i += 10;
			strlcpy(subsystem, buf, sizeof(subsystem));
		} else if (!strncmp(buf, "PCI_SLOT_NAME=", 14)) {
			buf += 14;
			i += 14;
			strlcpy(pci_slot_name, buf, sizeof(subsystem));
			event->devname = strdup(pci_slot_name);
		}
		for (; i < length; i++) {
			if (*buf == '\0')
				break;
			buf++;
		}
	}

	/* parse the subsystem layer */
	if (!strncmp(subsystem, "uio", 3))
		event->subsystem = EAL_DEV_EVENT_SUBSYSTEM_UIO;
	else if (!strncmp(subsystem, "pci", 3))
		event->subsystem = EAL_DEV_EVENT_SUBSYSTEM_PCI;
	else if (!strncmp(subsystem, "vfio", 4))
		event->subsystem = EAL_DEV_EVENT_SUBSYSTEM_VFIO;
	else
		return -1;

	/* parse the action type */
	if (!strncmp(action, "add", 3))
		event->type = RTE_DEV_EVENT_ADD;
	else if (!strncmp(action, "remove", 6))
		event->type = RTE_DEV_EVENT_REMOVE;
	else
		return -1;
	return 0;
}

static void
dev_delayed_unregister(void *param)
{
	rte_intr_callback_unregister(&intr_handle, dev_uev_handler, param);
	close(intr_handle.fd);
	intr_handle.fd = -1;
}

static void
dev_uev_handler(__rte_unused void *param)
{
	struct rte_dev_event uevent;
	int ret;
	char buf[EAL_UEV_MSG_LEN];

	memset(&uevent, 0, sizeof(struct rte_dev_event));
	memset(buf, 0, EAL_UEV_MSG_LEN);

	ret = recv(intr_handle.fd, buf, EAL_UEV_MSG_LEN, MSG_DONTWAIT);
	if (ret < 0 && errno == EAGAIN)
		return;
	else if (ret <= 0) {
		/* connection is closed or broken, can not up again. */
		RTE_LOG(ERR, EAL, "uevent socket connection is broken.\n");
		rte_eal_alarm_set(1, dev_delayed_unregister, NULL);
		return;
	}

	ret = dev_uev_parse(buf, &uevent, EAL_UEV_MSG_LEN);
	if (ret < 0) {
		RTE_LOG(DEBUG, EAL, "It is not an valid event "
			"that need to be handle.\n");
		return;
	}

	RTE_LOG(DEBUG, EAL, "receive uevent(name:%s, type:%d, subsystem:%d)\n",
		uevent.devname, uevent.type, uevent.subsystem);

	if (uevent.devname)
		dev_callback_process(uevent.devname, uevent.type);
}

int __rte_experimental
rte_dev_event_monitor_start(void)
{
	int ret;

	if (monitor_started)
		return 0;

	ret = dev_uev_socket_fd_create();
	if (ret) {
		RTE_LOG(ERR, EAL, "error create device event fd.\n");
		return -1;
	}

	intr_handle.type = RTE_INTR_HANDLE_DEV_EVENT;
	ret = rte_intr_callback_register(&intr_handle, dev_uev_handler, NULL);

	if (ret) {
		RTE_LOG(ERR, EAL, "fail to register uevent callback.\n");
		return -1;
	}

	monitor_started = true;

	return 0;
}

int __rte_experimental
rte_dev_event_monitor_stop(void)
{
	int ret;

	if (!monitor_started)
		return 0;

	ret = rte_intr_callback_unregister(&intr_handle, dev_uev_handler,
					   (void *)-1);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "fail to unregister uevent callback.\n");
		return ret;
	}

	close(intr_handle.fd);
	intr_handle.fd = -1;
	monitor_started = false;
	return 0;
}
