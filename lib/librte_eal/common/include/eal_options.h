/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2014 6WIND S.A.
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

#define OPT_PCI_WHITELIST "pci-whitelist"
#define OPT_PCI_BLACKLIST "pci-blacklist"

#define OPT_HUGE_DIR    "huge-dir"
#define OPT_PROC_TYPE   "proc-type"
#define OPT_NO_SHCONF   "no-shconf"
#define OPT_NO_HPET     "no-hpet"
#define OPT_VMWARE_TSC_MAP   "vmware-tsc-map"
#define OPT_NO_PCI      "no-pci"
#define OPT_NO_HUGE     "no-huge"
#define OPT_FILE_PREFIX "file-prefix"
#define OPT_SOCKET_MEM  "socket-mem"
#define OPT_VDEV        "vdev"
#define OPT_SYSLOG      "syslog"
#define OPT_LOG_LEVEL   "log-level"
#define OPT_BASE_VIRTADDR   "base-virtaddr"
#define OPT_XEN_DOM0    "xen-dom0"
#define OPT_CREATE_UIO_DEV "create-uio-dev"
#define OPT_VFIO_INTR    "vfio-intr"

extern const char eal_short_options[];
extern const struct option eal_long_options[];

int eal_parse_common_option(int opt, const char *argv, int longindex,
			    struct internal_config *conf);
void eal_common_usage(void);
