/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Dmitry Kozlyuk
 */

#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>

#include "eal_private.h"
#include "eal_windows.h"

int
eal_file_open(const char *path, int flags)
{
	static const int MODE_MASK = EAL_OPEN_READONLY | EAL_OPEN_READWRITE;

	int fd, ret, sys_flags;

	switch (flags & MODE_MASK) {
	case EAL_OPEN_READONLY:
		sys_flags = _O_RDONLY;
		break;
	case EAL_OPEN_READWRITE:
		sys_flags = _O_RDWR;
		break;
	default:
		rte_errno = ENOTSUP;
		return -1;
	}

	if (flags & EAL_OPEN_CREATE)
		sys_flags |= _O_CREAT;

	ret = _sopen_s(&fd, path, sys_flags, _SH_DENYNO, _S_IWRITE);
	if (ret < 0) {
		rte_errno = errno;
		return -1;
	}

	return fd;
}

int
eal_file_truncate(int fd, ssize_t size)
{
	HANDLE handle;
	DWORD ret;
	LONG low = (LONG)((size_t)size);
	LONG high = (LONG)((size_t)size >> 32);

	handle = (HANDLE)_get_osfhandle(fd);
	if (handle == INVALID_HANDLE_VALUE) {
		rte_errno = EBADF;
		return -1;
	}

	ret = SetFilePointer(handle, low, &high, FILE_BEGIN);
	if (ret == INVALID_SET_FILE_POINTER) {
		RTE_LOG_WIN32_ERR("SetFilePointer()");
		rte_errno = EINVAL;
		return -1;
	}

	return 0;
}

static int
lock_file(HANDLE handle, enum eal_flock_op op, enum eal_flock_mode mode)
{
	DWORD sys_flags = 0;
	OVERLAPPED overlapped;

	if (op == EAL_FLOCK_EXCLUSIVE)
		sys_flags |= LOCKFILE_EXCLUSIVE_LOCK;
	if (mode == EAL_FLOCK_RETURN)
		sys_flags |= LOCKFILE_FAIL_IMMEDIATELY;

	memset(&overlapped, 0, sizeof(overlapped));
	if (!LockFileEx(handle, sys_flags, 0, 0, 0, &overlapped)) {
		if ((sys_flags & LOCKFILE_FAIL_IMMEDIATELY) &&
			(GetLastError() == ERROR_IO_PENDING)) {
			rte_errno = EWOULDBLOCK;
		} else {
			RTE_LOG_WIN32_ERR("LockFileEx()");
			rte_errno = EINVAL;
		}
		return -1;
	}

	return 0;
}

static int
unlock_file(HANDLE handle)
{
	if (!UnlockFileEx(handle, 0, 0, 0, NULL)) {
		RTE_LOG_WIN32_ERR("UnlockFileEx()");
		rte_errno = EINVAL;
		return -1;
	}
	return 0;
}

int
eal_file_lock(int fd, enum eal_flock_op op, enum eal_flock_mode mode)
{
	HANDLE handle = (HANDLE)_get_osfhandle(fd);

	if (handle == INVALID_HANDLE_VALUE) {
		rte_errno = EBADF;
		return -1;
	}

	switch (op) {
	case EAL_FLOCK_EXCLUSIVE:
	case EAL_FLOCK_SHARED:
		return lock_file(handle, op, mode);
	case EAL_FLOCK_UNLOCK:
		return unlock_file(handle);
	default:
		rte_errno = EINVAL;
		return -1;
	}
}
