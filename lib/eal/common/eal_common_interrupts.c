/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <stdlib.h>
#include <string.h>

#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_malloc.h>

#include "eal_interrupts.h"

/* Macros to check for valid interrupt handle */
#define CHECK_VALID_INTR_HANDLE(intr_handle) do { \
	if (intr_handle == NULL) { \
		RTE_LOG(DEBUG, EAL, "Interrupt instance unallocated\n"); \
		rte_errno = EINVAL; \
		goto fail; \
	} \
} while (0)

#define RTE_INTR_INSTANCE_KNOWN_FLAGS (RTE_INTR_INSTANCE_F_PRIVATE \
	| RTE_INTR_INSTANCE_F_SHARED \
	)

#define RTE_INTR_INSTANCE_USES_RTE_MEMORY(flags) \
	(!!(flags & RTE_INTR_INSTANCE_F_SHARED))

struct rte_intr_handle *rte_intr_instance_alloc(uint32_t flags)
{
	struct rte_intr_handle *intr_handle;
	bool uses_rte_memory;

	/* Check the flag passed by user, it should be part of the
	 * defined flags.
	 */
	if ((flags & ~RTE_INTR_INSTANCE_KNOWN_FLAGS) != 0) {
		RTE_LOG(DEBUG, EAL, "Invalid alloc flag passed 0x%x\n", flags);
		rte_errno = EINVAL;
		return NULL;
	}

	uses_rte_memory = RTE_INTR_INSTANCE_USES_RTE_MEMORY(flags);
	if (uses_rte_memory)
		intr_handle = rte_zmalloc(NULL, sizeof(*intr_handle), 0);
	else
		intr_handle = calloc(1, sizeof(*intr_handle));
	if (intr_handle == NULL) {
		RTE_LOG(ERR, EAL, "Failed to allocate intr_handle\n");
		rte_errno = ENOMEM;
		return NULL;
	}

	if (uses_rte_memory) {
		intr_handle->efds = rte_zmalloc(NULL,
			RTE_MAX_RXTX_INTR_VEC_ID * sizeof(int), 0);
	} else {
		intr_handle->efds = calloc(RTE_MAX_RXTX_INTR_VEC_ID,
			sizeof(int));
	}
	if (intr_handle->efds == NULL) {
		RTE_LOG(ERR, EAL, "Fail to allocate event fd list\n");
		rte_errno = ENOMEM;
		goto fail;
	}

	if (uses_rte_memory) {
		intr_handle->elist = rte_zmalloc(NULL,
			RTE_MAX_RXTX_INTR_VEC_ID * sizeof(struct rte_epoll_event),
			0);
	} else {
		intr_handle->elist = calloc(RTE_MAX_RXTX_INTR_VEC_ID,
			sizeof(struct rte_epoll_event));
	}
	if (intr_handle->elist == NULL) {
		RTE_LOG(ERR, EAL, "fail to allocate event fd list\n");
		rte_errno = ENOMEM;
		goto fail;
	}

	intr_handle->alloc_flags = flags;
	intr_handle->nb_intr = RTE_MAX_RXTX_INTR_VEC_ID;

	return intr_handle;
fail:
	if (uses_rte_memory) {
		rte_free(intr_handle->efds);
		rte_free(intr_handle);
	} else {
		free(intr_handle->efds);
		free(intr_handle);
	}
	return NULL;
}

struct rte_intr_handle *rte_intr_instance_dup(const struct rte_intr_handle *src)
{
	struct rte_intr_handle *intr_handle;

	if (src == NULL) {
		RTE_LOG(DEBUG, EAL, "Source interrupt instance unallocated\n");
		rte_errno = EINVAL;
		return NULL;
	}

	intr_handle = rte_intr_instance_alloc(src->alloc_flags);
	if (intr_handle != NULL) {
		intr_handle->fd = src->fd;
		intr_handle->dev_fd = src->dev_fd;
		intr_handle->type = src->type;
		intr_handle->max_intr = src->max_intr;
		intr_handle->nb_efd = src->nb_efd;
		intr_handle->efd_counter_size = src->efd_counter_size;
		memcpy(intr_handle->efds, src->efds, src->nb_intr);
		memcpy(intr_handle->elist, src->elist, src->nb_intr);
	}

	return intr_handle;
}

int rte_intr_event_list_update(struct rte_intr_handle *intr_handle, int size)
{
	struct rte_epoll_event *tmp_elist;
	bool uses_rte_memory;
	int *tmp_efds;

	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (size == 0) {
		RTE_LOG(DEBUG, EAL, "Size can't be zero\n");
		rte_errno = EINVAL;
		goto fail;
	}

	uses_rte_memory =
		RTE_INTR_INSTANCE_USES_RTE_MEMORY(intr_handle->alloc_flags);
	if (uses_rte_memory) {
		tmp_efds = rte_realloc(intr_handle->efds, size * sizeof(int),
			0);
	} else {
		tmp_efds = realloc(intr_handle->efds, size * sizeof(int));
	}
	if (tmp_efds == NULL) {
		RTE_LOG(ERR, EAL, "Failed to realloc the efds list\n");
		rte_errno = ENOMEM;
		goto fail;
	}
	intr_handle->efds = tmp_efds;

	if (uses_rte_memory) {
		tmp_elist = rte_realloc(intr_handle->elist,
			size * sizeof(struct rte_epoll_event), 0);
	} else {
		tmp_elist = realloc(intr_handle->elist,
			size * sizeof(struct rte_epoll_event));
	}
	if (tmp_elist == NULL) {
		RTE_LOG(ERR, EAL, "Failed to realloc the event list\n");
		rte_errno = ENOMEM;
		goto fail;
	}
	intr_handle->elist = tmp_elist;

	intr_handle->nb_intr = size;

	return 0;
fail:
	return -rte_errno;
}

void rte_intr_instance_free(struct rte_intr_handle *intr_handle)
{
	if (intr_handle == NULL)
		return;
	if (RTE_INTR_INSTANCE_USES_RTE_MEMORY(intr_handle->alloc_flags)) {
		rte_free(intr_handle->efds);
		rte_free(intr_handle->elist);
		rte_free(intr_handle);
	} else {
		free(intr_handle->efds);
		free(intr_handle->elist);
		free(intr_handle);
	}
}

int rte_intr_fd_set(struct rte_intr_handle *intr_handle, int fd)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	intr_handle->fd = fd;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_fd_get(const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->fd;
fail:
	return -1;
}

int rte_intr_type_set(struct rte_intr_handle *intr_handle,
	enum rte_intr_handle_type type)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	intr_handle->type = type;

	return 0;
fail:
	return -rte_errno;
}

enum rte_intr_handle_type rte_intr_type_get(
	const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->type;
fail:
	return RTE_INTR_HANDLE_UNKNOWN;
}

int rte_intr_dev_fd_set(struct rte_intr_handle *intr_handle, int fd)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	intr_handle->dev_fd = fd;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_dev_fd_get(const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->dev_fd;
fail:
	return -1;
}

int rte_intr_max_intr_set(struct rte_intr_handle *intr_handle,
				 int max_intr)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (max_intr > intr_handle->nb_intr) {
		RTE_LOG(DEBUG, EAL, "Maximum interrupt vector ID (%d) exceeds "
			"the number of available events (%d)\n", max_intr,
			intr_handle->nb_intr);
		rte_errno = ERANGE;
		goto fail;
	}

	intr_handle->max_intr = max_intr;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_max_intr_get(const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->max_intr;
fail:
	return -rte_errno;
}

int rte_intr_nb_efd_set(struct rte_intr_handle *intr_handle, int nb_efd)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	intr_handle->nb_efd = nb_efd;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_nb_efd_get(const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->nb_efd;
fail:
	return -rte_errno;
}

int rte_intr_nb_intr_get(const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->nb_intr;
fail:
	return -rte_errno;
}

int rte_intr_efd_counter_size_set(struct rte_intr_handle *intr_handle,
	uint8_t efd_counter_size)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	intr_handle->efd_counter_size = efd_counter_size;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_efd_counter_size_get(const struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->efd_counter_size;
fail:
	return -rte_errno;
}

int rte_intr_efds_index_get(const struct rte_intr_handle *intr_handle,
	int index)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (index >= intr_handle->nb_intr) {
		RTE_LOG(DEBUG, EAL, "Invalid index %d, max limit %d\n", index,
			intr_handle->nb_intr);
		rte_errno = EINVAL;
		goto fail;
	}

	return intr_handle->efds[index];
fail:
	return -rte_errno;
}

int rte_intr_efds_index_set(struct rte_intr_handle *intr_handle,
	int index, int fd)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (index >= intr_handle->nb_intr) {
		RTE_LOG(DEBUG, EAL, "Invalid index %d, max limit %d\n", index,
			intr_handle->nb_intr);
		rte_errno = ERANGE;
		goto fail;
	}

	intr_handle->efds[index] = fd;

	return 0;
fail:
	return -rte_errno;
}

struct rte_epoll_event *rte_intr_elist_index_get(
	struct rte_intr_handle *intr_handle, int index)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (index >= intr_handle->nb_intr) {
		RTE_LOG(DEBUG, EAL, "Invalid index %d, max limit %d\n", index,
			intr_handle->nb_intr);
		rte_errno = ERANGE;
		goto fail;
	}

	return &intr_handle->elist[index];
fail:
	return NULL;
}

int rte_intr_elist_index_set(struct rte_intr_handle *intr_handle,
	int index, struct rte_epoll_event elist)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (index >= intr_handle->nb_intr) {
		RTE_LOG(DEBUG, EAL, "Invalid index %d, max limit %d\n", index,
			intr_handle->nb_intr);
		rte_errno = ERANGE;
		goto fail;
	}

	intr_handle->elist[index] = elist;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_vec_list_alloc(struct rte_intr_handle *intr_handle,
	const char *name, int size)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	/* Vector list already allocated */
	if (intr_handle->intr_vec != NULL)
		return 0;

	if (size > intr_handle->nb_intr) {
		RTE_LOG(DEBUG, EAL, "Invalid size %d, max limit %d\n", size,
			intr_handle->nb_intr);
		rte_errno = ERANGE;
		goto fail;
	}

	if (RTE_INTR_INSTANCE_USES_RTE_MEMORY(intr_handle->alloc_flags))
		intr_handle->intr_vec = rte_zmalloc(name, size * sizeof(int), 0);
	else
		intr_handle->intr_vec = calloc(size, sizeof(int));
	if (intr_handle->intr_vec == NULL) {
		RTE_LOG(ERR, EAL, "Failed to allocate %d intr_vec\n", size);
		rte_errno = ENOMEM;
		goto fail;
	}

	intr_handle->vec_list_size = size;

	return 0;
fail:
	return -rte_errno;
}

int rte_intr_vec_list_index_get(const struct rte_intr_handle *intr_handle,
				int index)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (index >= intr_handle->vec_list_size) {
		RTE_LOG(DEBUG, EAL, "Index %d greater than vec list size %d\n",
			index, intr_handle->vec_list_size);
		rte_errno = ERANGE;
		goto fail;
	}

	return intr_handle->intr_vec[index];
fail:
	return -rte_errno;
}

int rte_intr_vec_list_index_set(struct rte_intr_handle *intr_handle,
				int index, int vec)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	if (index >= intr_handle->vec_list_size) {
		RTE_LOG(DEBUG, EAL, "Index %d greater than vec list size %d\n",
			index, intr_handle->vec_list_size);
		rte_errno = ERANGE;
		goto fail;
	}

	intr_handle->intr_vec[index] = vec;

	return 0;
fail:
	return -rte_errno;
}

void rte_intr_vec_list_free(struct rte_intr_handle *intr_handle)
{
	if (intr_handle == NULL)
		return;
	if (RTE_INTR_INSTANCE_USES_RTE_MEMORY(intr_handle->alloc_flags))
		rte_free(intr_handle->intr_vec);
	else
		free(intr_handle->intr_vec);
	intr_handle->intr_vec = NULL;
	intr_handle->vec_list_size = 0;
}

void *rte_intr_instance_windows_handle_get(struct rte_intr_handle *intr_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	return intr_handle->windows_handle;
fail:
	return NULL;
}

int rte_intr_instance_windows_handle_set(struct rte_intr_handle *intr_handle,
	void *windows_handle)
{
	CHECK_VALID_INTR_HANDLE(intr_handle);

	intr_handle->windows_handle = windows_handle;

	return 0;
fail:
	return -rte_errno;
}
