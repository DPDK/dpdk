/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2014-2021 Netronome Systems, Inc.
 * All rights reserved.
 *
 * Small portions derived from code Copyright(c) 2010-2015 Intel Corporation.
 */

/*
 * vim:shiftwidth=8:noexpandtab
 *
 * @file dpdk/pmd/nfp_cpp_bridge.c
 *
 * Netronome vNIC DPDK Poll-Mode Driver: CPP Bridge
 */

#include <rte_service_component.h>

#include "nfpcore/nfp_cpp.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_nsp.h"

#include "nfp_logs.h"
#include "nfp_cpp_bridge.h"

#include <sys/ioctl.h>

/* Prototypes */
static int nfp_cpp_bridge_serve_write(int sockfd, struct nfp_cpp *cpp);
static int nfp_cpp_bridge_serve_read(int sockfd, struct nfp_cpp *cpp);
static int nfp_cpp_bridge_serve_ioctl(int sockfd, struct nfp_cpp *cpp);

void nfp_register_cpp_service(struct nfp_cpp *cpp)
{
	uint32_t *cpp_service_id = NULL;
	struct rte_service_spec service;

	memset(&service, 0, sizeof(struct rte_service_spec));
	snprintf(service.name, sizeof(service.name), "nfp_cpp_service");
	service.callback = nfp_cpp_bridge_service_func;
	service.callback_userdata = (void *)cpp;

	if (rte_service_component_register(&service,
					   cpp_service_id))
		RTE_LOG(WARNING, PMD, "NFP CPP bridge service register() failed");
	else
		RTE_LOG(DEBUG, PMD, "NFP CPP bridge service registered");
}

/*
 * Serving a write request to NFP from host programs. The request
 * sends the write size and the CPP target. The bridge makes use
 * of CPP interface handler configured by the PMD setup.
 */
static int
nfp_cpp_bridge_serve_write(int sockfd, struct nfp_cpp *cpp)
{
	struct nfp_cpp_area *area;
	off_t offset, nfp_offset;
	uint32_t cpp_id, pos, len;
	uint32_t tmpbuf[16];
	size_t count, curlen;
	int err = 0;

	PMD_CPP_LOG(DEBUG, "%s: offset size %zu, count_size: %zu\n", __func__,
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

	PMD_CPP_LOG(DEBUG, "%s: count %zu and offset %jd\n", __func__, count,
		offset);
	PMD_CPP_LOG(DEBUG, "%s: cpp_id %08x and nfp_offset %jd\n", __func__,
		cpp_id, nfp_offset);

	/* Adjust length if not aligned */
	if (((nfp_offset + (off_t)count - 1) & ~(NFP_CPP_MEMIO_BOUNDARY - 1)) !=
	    (nfp_offset & ~(NFP_CPP_MEMIO_BOUNDARY - 1))) {
		curlen = NFP_CPP_MEMIO_BOUNDARY -
			(nfp_offset & (NFP_CPP_MEMIO_BOUNDARY - 1));
	}

	while (count > 0) {
		/* configure a CPP PCIe2CPP BAR for mapping the CPP target */
		area = nfp_cpp_area_alloc_with_name(cpp, cpp_id, "nfp.cdev",
						    nfp_offset, curlen);
		if (!area) {
			RTE_LOG(ERR, PMD, "%s: area alloc fail\n", __func__);
			return -EIO;
		}

		/* mapping the target */
		err = nfp_cpp_area_acquire(area);
		if (err < 0) {
			RTE_LOG(ERR, PMD, "area acquire failed\n");
			nfp_cpp_area_free(area);
			return -EIO;
		}

		for (pos = 0; pos < curlen; pos += len) {
			len = curlen - pos;
			if (len > sizeof(tmpbuf))
				len = sizeof(tmpbuf);

			PMD_CPP_LOG(DEBUG, "%s: Receive %u of %zu\n", __func__,
					   len, count);
			err = recv(sockfd, tmpbuf, len, MSG_WAITALL);
			if (err != (int)len) {
				RTE_LOG(ERR, PMD,
					"%s: error when receiving, %d of %zu\n",
					__func__, err, count);
				nfp_cpp_area_release(area);
				nfp_cpp_area_free(area);
				return -EIO;
			}
			err = nfp_cpp_area_write(area, pos, tmpbuf, len);
			if (err < 0) {
				RTE_LOG(ERR, PMD, "nfp_cpp_area_write error\n");
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
nfp_cpp_bridge_serve_read(int sockfd, struct nfp_cpp *cpp)
{
	struct nfp_cpp_area *area;
	off_t offset, nfp_offset;
	uint32_t cpp_id, pos, len;
	uint32_t tmpbuf[16];
	size_t count, curlen;
	int err = 0;

	PMD_CPP_LOG(DEBUG, "%s: offset size %zu, count_size: %zu\n", __func__,
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

	PMD_CPP_LOG(DEBUG, "%s: count %zu and offset %jd\n", __func__, count,
			   offset);
	PMD_CPP_LOG(DEBUG, "%s: cpp_id %08x and nfp_offset %jd\n", __func__,
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
		if (!area) {
			RTE_LOG(ERR, PMD, "%s: area alloc failed\n", __func__);
			return -EIO;
		}

		err = nfp_cpp_area_acquire(area);
		if (err < 0) {
			RTE_LOG(ERR, PMD, "area acquire failed\n");
			nfp_cpp_area_free(area);
			return -EIO;
		}

		for (pos = 0; pos < curlen; pos += len) {
			len = curlen - pos;
			if (len > sizeof(tmpbuf))
				len = sizeof(tmpbuf);

			err = nfp_cpp_area_read(area, pos, tmpbuf, len);
			if (err < 0) {
				RTE_LOG(ERR, PMD, "nfp_cpp_area_read error\n");
				nfp_cpp_area_release(area);
				nfp_cpp_area_free(area);
				return -EIO;
			}
			PMD_CPP_LOG(DEBUG, "%s: sending %u of %zu\n", __func__,
					   len, count);

			err = send(sockfd, tmpbuf, len, 0);
			if (err != (int)len) {
				RTE_LOG(ERR, PMD,
					"%s: error when sending: %d of %zu\n",
					__func__, err, count);
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
nfp_cpp_bridge_serve_ioctl(int sockfd, struct nfp_cpp *cpp)
{
	uint32_t cmd, ident_size, tmp;
	int err;

	/* Reading now the IOCTL command */
	err = recv(sockfd, &cmd, 4, 0);
	if (err != 4) {
		RTE_LOG(ERR, PMD, "%s: read error from socket\n", __func__);
		return -EIO;
	}

	/* Only supporting NFP_IOCTL_CPP_IDENTIFICATION */
	if (cmd != NFP_IOCTL_CPP_IDENTIFICATION) {
		RTE_LOG(ERR, PMD, "%s: unknown cmd %d\n", __func__, cmd);
		return -EINVAL;
	}

	err = recv(sockfd, &ident_size, 4, 0);
	if (err != 4) {
		RTE_LOG(ERR, PMD, "%s: read error from socket\n", __func__);
		return -EIO;
	}

	tmp = nfp_cpp_model(cpp);

	PMD_CPP_LOG(DEBUG, "%s: sending NFP model %08x\n", __func__, tmp);

	err = send(sockfd, &tmp, 4, 0);
	if (err != 4) {
		RTE_LOG(ERR, PMD, "%s: error writing to socket\n", __func__);
		return -EIO;
	}

	tmp = cpp->interface;

	PMD_CPP_LOG(DEBUG, "%s: sending NFP interface %08x\n", __func__, tmp);

	err = send(sockfd, &tmp, 4, 0);
	if (err != 4) {
		RTE_LOG(ERR, PMD, "%s: error writing to socket\n", __func__);
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
int32_t
nfp_cpp_bridge_service_func(void *args)
{
	struct sockaddr address;
	struct nfp_cpp *cpp = args;
	int sockfd, datafd, op, ret;

	unlink("/tmp/nfp_cpp");
	sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sockfd < 0) {
		RTE_LOG(ERR, PMD, "%s: socket creation error. Service failed\n",
			__func__);
		return -EIO;
	}

	memset(&address, 0, sizeof(struct sockaddr));

	address.sa_family = AF_UNIX;
	strcpy(address.sa_data, "/tmp/nfp_cpp");

	ret = bind(sockfd, (const struct sockaddr *)&address,
		   sizeof(struct sockaddr));
	if (ret < 0) {
		RTE_LOG(ERR, PMD, "%s: bind error (%d). Service failed\n",
				  __func__, errno);
		close(sockfd);
		return ret;
	}

	ret = listen(sockfd, 20);
	if (ret < 0) {
		RTE_LOG(ERR, PMD, "%s: listen error(%d). Service failed\n",
				  __func__, errno);
		close(sockfd);
		return ret;
	}

	for (;;) {
		datafd = accept(sockfd, NULL, NULL);
		if (datafd < 0) {
			RTE_LOG(ERR, PMD, "%s: accept call error (%d)\n",
					  __func__, errno);
			RTE_LOG(ERR, PMD, "%s: service failed\n", __func__);
			close(sockfd);
			return -EIO;
		}

		while (1) {
			ret = recv(datafd, &op, 4, 0);
			if (ret <= 0) {
				PMD_CPP_LOG(DEBUG, "%s: socket close\n",
						   __func__);
				break;
			}

			PMD_CPP_LOG(DEBUG, "%s: getting op %u\n", __func__, op);

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
/*
 * Local variables:
 * c-file-style: "Linux"
 * indent-tabs-mode: t
 * End:
 */
