#!/usr/bin/python

#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Test data for autotests

from glob import glob
from autotest_test_funcs import *

# quick and dirty function to find out number of sockets
def num_sockets():
	result = len(glob("/sys/devices/system/node/node*"))
	if result == 0:
		return 1
	return result

# spread given number for all sockets
# e.g. 32 becomes 16,16 or 8,8,8,8 etc.
def all_sockets(num):
	mem_per_socket = num / num_sockets()
	return ",".join([str(mem_per_socket)] * num_sockets())

# groups of tests that can be run in parallel
# the grouping has been found largely empirically
parallel_test_group_list = [

{
	"Prefix":	"group_1",
	"Memory" :	all_sockets(8),
	"Tests" :	
	[
		{
		 "Name" :	"Timer autotest",
		 "Command" : 	"timer_autotest",
		 "Func" :	timer_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Debug autotest",
		 "Command" : 	"debug_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},	
		{
		 "Name" :	"Errno autotest",
		 "Command" : 	"errno_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Meter autotest",
		 "Command" : 	"meter_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Common autotest",
		 "Command" : 	"common_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},	
		{
		 "Name" :	"Dump log history",
		 "Command" :	"dump_log_history",
		 "Func" :	dump_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Dump rings",
		 "Command" :	"dump_ring",
		 "Func" :	dump_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Dump mempools",
		 "Command" :	"dump_mempool",
		 "Func" :	dump_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"group_2",
	"Memory" :	"32",
	"Tests" :	
	[
		{
		 "Name" :	"Memory autotest",
		 "Command" :	"memory_autotest",
		 "Func" :	memory_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Read/write lock autotest",
		 "Command" : 	"rwlock_autotest",
		 "Func" :	rwlock_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Logs autotest",
		 "Command" : 	"logs_autotest",
		 "Func" :	logs_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"CPU flags autotest",
		 "Command" : 	"cpuflags_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Version autotest",
		 "Command" : 	"version_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"EAL filesystem autotest",
		 "Command" : 	"eal_fs_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"EAL flags autotest",
		 "Command" : 	"eal_flags_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Hash autotest",
		 "Command" : 	"hash_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	],
},
{
	"Prefix":	"group_3",
	"Memory" :	all_sockets(1024),
	"Tests" :	
	[
		{
		 "Name" :	"LPM autotest",
		 "Command" : 	"lpm_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"IVSHMEM autotest",
		 "Command" : 	"ivshmem_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Memcpy autotest",
		 "Command" : 	"memcpy_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Memzone autotest",
		 "Command" : 	"memzone_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"String autotest",
		 "Command" : 	"string_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Alarm autotest",
		 "Command" :	"alarm_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"group_4",
	"Memory" :	all_sockets(128),
	"Tests" :	
	[
		{
		 "Name" :	"PCI autotest",
		 "Command" :	"pci_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Malloc autotest",
		 "Command" : 	"malloc_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Multi-process autotest",
		 "Command" : 	"multiprocess_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Mbuf autotest",
		 "Command" : 	"mbuf_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Per-lcore autotest",
		 "Command" : 	"per_lcore_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Ring autotest",
		 "Command" : 	"ring_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"group_5",
	"Memory" :	"16",
	"Tests" :	
	[
		{
		 "Name" :	"Spinlock autotest",
		 "Command" : 	"spinlock_autotest",
		 "Func" :	spinlock_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Byte order autotest",
		 "Command" : 	"byteorder_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"TAILQ autotest",
		 "Command" : 	"tailq_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Command-line autotest",
		 "Command" : 	"cmdline_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Interrupts autotest",
		 "Command" : 	"interrupt_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"group_6",
	"Memory" :	all_sockets(620),
	"Tests" :	
	[
		{
		 "Name" :	"Function reentrancy autotest",
		 "Command" : 	"func_reentrancy_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Mempool autotest",
		 "Command" : 	"mempool_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Atomics autotest",
		 "Command" : 	"atomic_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Prefetch autotest",
		 "Command" : 	"prefetch_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		 },
		{
		 "Name" :"Red autotest",
		 "Command" : "red_autotest",
		 "Func" :default_autotest,
		 "Report" :None,
		 },
	]
},
{
	"Prefix" :	"group_7",
	"Memory" :	"400",
	"Tests" :
	[
		{
		 "Name" :	"PMD ring autotest",
		 "Command" :	"ring_pmd_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Access list control autotest",
		 "Command" : 	"acl_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Pattern match autotest",
		 "Command" : 	"pm_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		 {
		 "Name" :"Sched autotest",
		 "Command" : "sched_autotest",
		 "Func" :default_autotest,
		 "Report" :None,
		 },
	]
},
]

# tests that should not be run when any other tests are running
non_parallel_test_group_list = [

{
	"Prefix" :	"kni",
	"Memory" :	"512",
	"Tests" :
	[
		{
		 "Name" :	"KNI autotest",
		 "Command" :	"kni_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"mempool_perf",
	"Memory" :	all_sockets(256),
	"Tests" :	
	[
		{
		 "Name" :	"Cycles autotest",
		 "Command" : 	"cycles_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
		{
		 "Name" :	"Mempool performance autotest",
		 "Command" : 	"mempool_perf_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"memcpy_perf",
	"Memory" :	all_sockets(512),
	"Tests" :	
	[
		{
		 "Name" :	"Memcpy performance autotest",
		 "Command" : 	"memcpy_perf_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix":	"hash_perf",
	"Memory" :	all_sockets(512),
	"Tests" :	
	[
		{
		 "Name" :	"Hash performance autotest",
		 "Command" : 	"hash_perf_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
{
	"Prefix" :      "power",
	"Memory" :      all_sockets(512),
	"Tests" :
	[
		{
		 "Name" :       "Power autotest",
		 "Command" :    "power_autotest",
		 "Func" :       default_autotest,
		 "Report" :     None,
		},
	]
},
{
	"Prefix" :	"lpm6",
	"Memory" :	"512",
	"Tests" :
	[
		{
                 "Name" :       "LPM6 autotest",
                 "Command" :    "lpm6_autotest",
                 "Func" :       default_autotest,
                 "Report" :     None,
                },
	]
},
{
	"Prefix":	"timer_perf",
	"Memory" :	all_sockets(512),
	"Tests" :	
	[
		{
		 "Name" :	"Timer performance autotest",
		 "Command" : 	"timer_perf_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
							
#
# Please always make sure that ring_perf is the last test!
#
{
	"Prefix":	"ring_perf",
	"Memory" :	all_sockets(512),
	"Tests" :	
	[
		{
		 "Name" :	"Ring performance autotest",
		 "Command" : 	"ring_perf_autotest",
		 "Func" :	default_autotest,
		 "Report" :	None,
		},
	]
},
]
