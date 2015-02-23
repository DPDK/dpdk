/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rte_log.h>

#include "eventfd_link/eventfd_link.h"
#include "eventfd_copy.h"
#include "vhost-net.h"

static const char eventfd_cdev[] = "/dev/eventfd-link";

/*
 * This function uses the eventfd_link kernel module to copy an eventfd file
 * descriptor provided by QEMU in to our process space.
 */
int
eventfd_copy(int target_fd, int target_pid)
{
	int eventfd_link, ret;
	struct eventfd_copy eventfd_copy;
	int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);

	if (fd == -1)
		return -1;

	/* Open the character device to the kernel module. */
	/* TODO: check this earlier rather than fail until VM boots! */
	eventfd_link = open(eventfd_cdev, O_RDWR);
	if (eventfd_link < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"eventfd_link module is not loaded\n");
		close(fd);
		return -1;
	}

	eventfd_copy.source_fd = fd;
	eventfd_copy.target_fd = target_fd;
	eventfd_copy.target_pid = target_pid;
	/* Call the IOCTL to copy the eventfd. */
	ret = ioctl(eventfd_link, EVENTFD_COPY, &eventfd_copy);
	close(eventfd_link);

	if (ret < 0) {
		RTE_LOG(ERR, VHOST_CONFIG,
			"EVENTFD_COPY ioctl failed\n");
		close(fd);
		return -1;
	}

	return fd;
}
