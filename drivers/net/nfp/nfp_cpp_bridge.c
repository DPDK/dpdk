/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 */

#include "nfp_cpp_bridge.h"

#include <unistd.h>
#include <sys/ioctl.h>

#include "nfpcore/nfp_cpp.h"
#include "nfp_logs.h"
#include "nfp_service.h"

#define NFP_CPP_MEMIO_BOUNDARY    (1 << 20)
#define NFP_BRIDGE_OP_READ        20
#define NFP_BRIDGE_OP_WRITE       30
#define NFP_BRIDGE_OP_IOCTL       40

#define NFP_IOCTL 'n'
#define NFP_IOCTL_CPP_IDENTIFICATION _IOW(NFP_IOCTL, 0x8f, uint32_t)

/* Prototypes */
static int nfp_cpp_bridge_service_func(void *args);

int
nfp_enable_cpp_service(struct nfp_pf_dev *pf_dev)
{
	int ret;
	const char *pci_name;
	struct rte_service_spec cpp_service = {
		.callback          = nfp_cpp_bridge_service_func,
		.callback_userdata = (void *)pf_dev,
	};

	pci_name = strchr(pf_dev->pci_dev->name, ':') + 1;
	snprintf(cpp_service.name, sizeof(cpp_service.name), "%s_cpp_service", pci_name);

	ret = nfp_service_enable(&cpp_service, &pf_dev->cpp_service_info);
	if (ret != 0) {
		PMD_INIT_LOG(DEBUG, "Could not enable service %s.", cpp_service.name);
		return ret;
	}

	return 0;
}

void
nfp_disable_cpp_service(struct nfp_pf_dev *pf_dev)
{
	nfp_service_disable(&pf_dev->cpp_service_info);
}

/*
 * Serving a write request to NFP from host programs. The request
 * sends the write size and the CPP target. The bridge makes use
 * of CPP interface handler configured by the PMD setup.
 */
static int
nfp_cpp_bridge_serve_write(int sockfd,
		struct nfp_cpp *cpp)
{
	int err;
	off_t offset;
	uint32_t pos;
	uint32_t len;
	size_t count;
	size_t curlen;
	uint32_t cpp_id;
	off_t nfp_offset;
	uint32_t tmpbuf[16];
	struct nfp_cpp_area *area;

	PMD_CPP_LOG(DEBUG, "%s: offset size %zu, count_size: %zu.", __func__,
			sizeof(off_t), sizeof(size_t));

	/* Reading the count param */
	err = recv(sockfd, &count, sizeof(off_t), 0);
	if (err != sizeof(off_t))
		return -EINVAL;

	curlen = count;

	/* Reading the offset param */
	err = recv(sockfd, &offset, sizeof(off_t), 0);
	if (err != sizeof(off_t))
		return -EINVAL;

	/* Obtain target's CPP ID and offset in target */
	cpp_id = (offset >> 40) << 8;
	nfp_offset = offset & ((1ull << 40) - 1);

	PMD_CPP_LOG(DEBUG, "%s: count %zu and offset %jd.", __func__, count,
			offset);
	PMD_CPP_LOG(DEBUG, "%s: cpp_id %08x and nfp_offset %jd.", __func__,
			cpp_id, nfp_offset);

	/* Adjust length if not aligned */
	if (((nfp_offset + (off_t)count - 1) & ~(NFP_CPP_MEMIO_BOUNDARY - 1)) !=
			(nfp_offset & ~(NFP_CPP_MEMIO_BOUNDARY - 1))) {
		curlen = NFP_CPP_MEMIO_BOUNDARY -
				(nfp_offset & (NFP_CPP_MEMIO_BOUNDARY - 1));
	}

	while (count > 0) {
		/* Configure a CPP PCIe2CPP BAR for mapping the CPP target */
		area = nfp_cpp_area_alloc_with_name(cpp, cpp_id, "nfp.cdev",
				nfp_offset, curlen);
		if (area == NULL) {
			PMD_CPP_LOG(ERR, "Area alloc fail.");
			return -EIO;
		}

		/* Mapping the target */
		err = nfp_cpp_area_acquire(area);
		if (err < 0) {
			PMD_CPP_LOG(ERR, "Area acquire failed.");
			nfp_cpp_area_free(area);
			return -EIO;
		}

		for (pos = 0; pos < curlen; pos += len) {
			len = curlen - pos;
			if (len > sizeof(tmpbuf))
				len = sizeof(tmpbuf);

			PMD_CPP_LOG(DEBUG, "%s: Receive %u of %zu.", __func__,
					len, count);
			err = recv(sockfd, tmpbuf, len, MSG_WAITALL);
			if (err != (int)len) {
				PMD_CPP_LOG(ERR, "Error when receiving, %d of %zu.",
						err, count);
				nfp_cpp_area_release(area);
				nfp_cpp_area_free(area);
				return -EIO;
			}

			err = nfp_cpp_area_write(area, pos, tmpbuf, len);
			if (err < 0) {
				PMD_CPP_LOG(ERR, "The nfp_cpp_area_write error.");
				nfp_cpp_area_release(area);
				nfp_cpp_area_free(area);
				return -EIO;
			}
		}

		nfp_offset += pos;
		nfp_cpp_area_release(area);
		nfp_cpp_area_free(area);

		count -= pos;
		curlen = (count > NFP_CPP_MEMIO_BOUNDARY) ?
				NFP_CPP_MEMIO_BOUNDARY : count;
	}

	return 0;
}

/*
 * Serving a read request to NFP from host programs. The request
 * sends the read size and the CPP target. The bridge makes use
 * of CPP interface handler configured by the PMD setup. The read
 * data is sent to the requester using the same socket.
 */
static int
nfp_cpp_bridge_serve_read(int sockfd,
		struct nfp_cpp *cpp)
{
	int err;
	off_t offset;
	uint32_t pos;
	uint32_t len;
	size_t count;
	size_t curlen;
	uint32_t cpp_id;
	off_t nfp_offset;
	uint32_t tmpbuf[16];
	struct nfp_cpp_area *area;

	PMD_CPP_LOG(DEBUG, "%s: offset size %zu, count_size: %zu.", __func__,
			sizeof(off_t), sizeof(size_t));

	/* Reading the count param */
	err = recv(sockfd, &count, sizeof(off_t), 0);
	if (err != sizeof(off_t))
		return -EINVAL;

	curlen = count;

	/* Reading the offset param */
	err = recv(sockfd, &offset, sizeof(off_t), 0);
	if (err != sizeof(off_t))
		return -EINVAL;

	/* Obtain target's CPP ID and offset in target */
	cpp_id = (offset >> 40) << 8;
	nfp_offset = offset & ((1ull << 40) - 1);

	PMD_CPP_LOG(DEBUG, "%s: count %zu and offset %jd.", __func__, count,
			offset);
	PMD_CPP_LOG(DEBUG, "%s: cpp_id %08x and nfp_offset %jd.", __func__,
			cpp_id, nfp_offset);

	/* Adjust length if not aligned */
	if (((nfp_offset + (off_t)count - 1) & ~(NFP_CPP_MEMIO_BOUNDARY - 1)) !=
			(nfp_offset & ~(NFP_CPP_MEMIO_BOUNDARY - 1))) {
		curlen = NFP_CPP_MEMIO_BOUNDARY -
				(nfp_offset & (NFP_CPP_MEMIO_BOUNDARY - 1));
	}

	while (count > 0) {
		area = nfp_cpp_area_alloc_with_name(cpp, cpp_id, "nfp.cdev",
				nfp_offset, curlen);
		if (area == NULL) {
			PMD_CPP_LOG(ERR, "Area alloc failed.");
			return -EIO;
		}

		err = nfp_cpp_area_acquire(area);
		if (err < 0) {
			PMD_CPP_LOG(ERR, "Area acquire failed.");
			nfp_cpp_area_free(area);
			return -EIO;
		}

		for (pos = 0; pos < curlen; pos += len) {
			len = curlen - pos;
			if (len > sizeof(tmpbuf))
				len = sizeof(tmpbuf);

			err = nfp_cpp_area_read(area, pos, tmpbuf, len);
			if (err < 0) {
				PMD_CPP_LOG(ERR, "The nfp_cpp_area_read error.");
				nfp_cpp_area_release(area);
				nfp_cpp_area_free(area);
				return -EIO;
			}
			PMD_CPP_LOG(DEBUG, "%s: sending %u of %zu.", __func__,
					len, count);

			err = send(sockfd, tmpbuf, len, 0);
			if (err != (int)len) {
				PMD_CPP_LOG(ERR, "Error when sending: %d of %zu.",
						err, count);
				nfp_cpp_area_release(area);
				nfp_cpp_area_free(area);
				return -EIO;
			}
		}

		nfp_offset += pos;
		nfp_cpp_area_release(area);
		nfp_cpp_area_free(area);

		count -= pos;
		curlen = (count > NFP_CPP_MEMIO_BOUNDARY) ?
				NFP_CPP_MEMIO_BOUNDARY : count;
	}

	return 0;
}

/*
 * Serving a ioctl command from host NFP tools. This usually goes to
 * a kernel driver char driver but it is not available when the PF is
 * bound to the PMD. Currently just one ioctl command is served and it
 * does not require any CPP access at all.
 */
static int
nfp_cpp_bridge_serve_ioctl(int sockfd,
		struct nfp_cpp *cpp)
{
	int err;
	uint32_t cmd;
	uint32_t tmp;
	uint32_t ident_size;

	/* Reading now the IOCTL command */
	err = recv(sockfd, &cmd, 4, 0);
	if (err != 4) {
		PMD_CPP_LOG(ERR, "Read error from socket.");
		return -EIO;
	}

	/* Only supporting NFP_IOCTL_CPP_IDENTIFICATION */
	if (cmd != NFP_IOCTL_CPP_IDENTIFICATION) {
		PMD_CPP_LOG(ERR, "Unknown cmd %d.", cmd);
		return -EINVAL;
	}

	err = recv(sockfd, &ident_size, 4, 0);
	if (err != 4) {
		PMD_CPP_LOG(ERR, "Read error from socket.");
		return -EIO;
	}

	tmp = nfp_cpp_model(cpp);

	PMD_CPP_LOG(DEBUG, "%s: sending NFP model %08x.", __func__, tmp);

	err = send(sockfd, &tmp, 4, 0);
	if (err != 4) {
		PMD_CPP_LOG(ERR, "Error writing to socket.");
		return -EIO;
	}

	tmp = nfp_cpp_interface(cpp);

	PMD_CPP_LOG(DEBUG, "%s: sending NFP interface %08x.", __func__, tmp);

	err = send(sockfd, &tmp, 4, 0);
	if (err != 4) {
		PMD_CPP_LOG(ERR, "Error writing to socket.");
		return -EIO;
	}

	return 0;
}

/*
 * This is the code to be executed by a service core. The CPP bridge interface
 * is based on a unix socket and requests usually received by a kernel char
 * driver, read, write and ioctl, are handled by the CPP bridge. NFP host tools
 * can be executed with a wrapper library and LD_LIBRARY being completely
 * unaware of the CPP bridge performing the NFP kernel char driver for CPP
 * accesses.
 */
static int
nfp_cpp_bridge_service_func(void *args)
{
	int op;
	int ret;
	int sockfd;
	int datafd;
	struct nfp_cpp *cpp;
	const char *pci_name;
	char socket_handle[14];
	struct sockaddr address;
	struct nfp_pf_dev *pf_dev;
	struct timeval timeout = {1, 0};

	pf_dev = args;

	pci_name = strchr(pf_dev->pci_dev->name, ':') + 1;
	snprintf(socket_handle, sizeof(socket_handle), "/tmp/%s", pci_name);

	unlink(socket_handle);
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		PMD_CPP_LOG(ERR, "Socket creation error. Service failed.");
		return -EIO;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

	memset(&address, 0, sizeof(struct sockaddr));

	address.sa_family = AF_UNIX;
	strcpy(address.sa_data, socket_handle);

	ret = bind(sockfd, (const struct sockaddr *)&address,
			sizeof(struct sockaddr));
	if (ret < 0) {
		PMD_CPP_LOG(ERR, "Bind error (%d). Service failed.", errno);
		close(sockfd);
		return ret;
	}

	ret = listen(sockfd, 20);
	if (ret < 0) {
		PMD_CPP_LOG(ERR, "Listen error(%d). Service failed.", errno);
		close(sockfd);
		return ret;
	}

	cpp = pf_dev->cpp;
	while (rte_service_runstate_get(pf_dev->cpp_service_info.id) != 0) {
		datafd = accept(sockfd, NULL, NULL);
		if (datafd < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				continue;

			PMD_CPP_LOG(ERR, "Accept call error (%d).", errno);
			PMD_CPP_LOG(ERR, "Service failed.");
			close(sockfd);
			return -EIO;
		}

		for (;;) {
			ret = recv(datafd, &op, 4, 0);
			if (ret <= 0) {
				PMD_CPP_LOG(DEBUG, "%s: socket close.", __func__);
				break;
			}

			PMD_CPP_LOG(DEBUG, "%s: getting op %u.", __func__, op);

			if (op == NFP_BRIDGE_OP_READ)
				nfp_cpp_bridge_serve_read(datafd, cpp);

			if (op == NFP_BRIDGE_OP_WRITE)
				nfp_cpp_bridge_serve_write(datafd, cpp);

			if (op == NFP_BRIDGE_OP_IOCTL)
				nfp_cpp_bridge_serve_ioctl(datafd, cpp);

			if (op == 0)
				break;
		}

		close(datafd);
	}

	close(sockfd);

	return 0;
}
