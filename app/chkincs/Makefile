#   BSD LICENSE
# 
#   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
# 

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
APP = chkincs

#
# all source are stored in SRCS-y
#

SRCS-$(CONFIG_RTE_APP_CHKINCS) +=  test.c \
	test_alarm.c \
	test_atomic.c \
	test_branch_prediction.c \
	test_byteorder.c \
	test_common.c \
	test_cpuflags.c \
	test_cycles.c \
	test_debug.c \
	test_eal.c \
	test_errno.c \
	test_ethdev.c \
	test_ether.c \
	test_fbk_hash.c \
	test_hash_crc.c \
	test_hash.c \
	test_interrupts.c \
	test_ip.c \
	test_jhash.c \
	test_launch.c \
	test_lcore.c \
	test_log.c \
	test_lpm.c \
	test_malloc.c \
	test_mbuf.c \
	test_memcpy.c \
	test_memory.c \
	test_mempool.c \
	test_memzone.c \
	test_pci_dev_ids.c \
	test_pci.c \
	test_per_lcore.c \
	test_prefetch.c \
	test_random.c \
	test_ring.c \
	test_rwlock.c \
	test_sctp.c \
	test_spinlock.c \
	test_string_fns.c \
	test_tailq.c \
	test_tcp.c \
	test_timer.c \
	test_udp.c \
	test_version.c

CFLAGS += -O0 -fno-inline
CFLAGS += $(WERROR_FLAGS)

# this application needs libraries first
DEPDIRS-$(CONFIG_RTE_APP_CHKINCS) += lib

include $(RTE_SDK)/mk/rte.app.mk
