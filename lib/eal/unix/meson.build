# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Dmitry Kozlyuk

sources += files(
        'eal_debug.c',
        'eal_file.c',
        'eal_filesystem.c',
        'eal_firmware.c',
        'eal_unix_memory.c',
        'eal_unix_thread.c',
        'eal_unix_timer.c',
        'rte_basename.c',
        'rte_thread.c',
)

if is_freebsd or cc.has_function('pthread_attr_setaffinity_np', args: '-D_GNU_SOURCE',
                                 prefix : '#include <pthread.h>')
    cflags += '-DRTE_EAL_PTHREAD_ATTR_SETAFFINITY_NP'
endif
