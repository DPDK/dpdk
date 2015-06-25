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

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_vhost.a

EXPORT_MAP := rte_vhost_version.map

LIBABIVER := 1

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3 -D_FILE_OFFSET_BITS=64
ifeq ($(CONFIG_RTE_LIBRTE_VHOST_USER),y)
CFLAGS += -I vhost_user
else
CFLAGS += -I vhost_cuse -lfuse
LDFLAGS += -lfuse
endif

ifeq ($(CONFIG_RTE_LIBRTE_VHOST_NUMA),y)
LDFLAGS += -lnuma
endif

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_VHOST) := virtio-net.c vhost_rxtx.c
ifeq ($(CONFIG_RTE_LIBRTE_VHOST_USER),y)
SRCS-$(CONFIG_RTE_LIBRTE_VHOST) += vhost_user/vhost-net-user.c vhost_user/virtio-net-user.c vhost_user/fd_man.c
else
SRCS-$(CONFIG_RTE_LIBRTE_VHOST) += vhost_cuse/vhost-net-cdev.c vhost_cuse/virtio-net-cdev.c vhost_cuse/eventfd_copy.c
endif

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_VHOST)-include += rte_virtio_net.h

# dependencies
DEPDIRS-$(CONFIG_RTE_LIBRTE_VHOST) += lib/librte_eal
DEPDIRS-$(CONFIG_RTE_LIBRTE_VHOST) += lib/librte_ether
DEPDIRS-$(CONFIG_RTE_LIBRTE_VHOST) += lib/librte_mbuf

include $(RTE_SDK)/mk/rte.lib.mk
