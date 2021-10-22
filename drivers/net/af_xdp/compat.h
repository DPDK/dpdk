/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation.
 */

#include <bpf/bpf.h>
#include <bpf/xsk.h>
#include <linux/version.h>
#include <poll.h>

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE && \
	defined(RTE_LIBRTE_AF_XDP_PMD_SHARED_UMEM)
#define ETH_AF_XDP_SHARED_UMEM 1
#endif

#ifdef ETH_AF_XDP_SHARED_UMEM
static __rte_always_inline int
create_shared_socket(struct xsk_socket **xsk_ptr,
			  const char *ifname,
			  __u32 queue_id, struct xsk_umem *umem,
			  struct xsk_ring_cons *rx,
			  struct xsk_ring_prod *tx,
			  struct xsk_ring_prod *fill,
			  struct xsk_ring_cons *comp,
			  const struct xsk_socket_config *config)
{
	return xsk_socket__create_shared(xsk_ptr, ifname, queue_id, umem, rx,
						tx, fill, comp, config);
}
#else
static __rte_always_inline int
create_shared_socket(struct xsk_socket **xsk_ptr __rte_unused,
			  const char *ifname __rte_unused,
			  __u32 queue_id __rte_unused,
			  struct xsk_umem *umem __rte_unused,
			  struct xsk_ring_cons *rx __rte_unused,
			  struct xsk_ring_prod *tx __rte_unused,
			  struct xsk_ring_prod *fill __rte_unused,
			  struct xsk_ring_cons *comp __rte_unused,
			  const struct xsk_socket_config *config __rte_unused)
{
	return -1;
}
#endif

#ifdef XDP_USE_NEED_WAKEUP
static int
tx_syscall_needed(struct xsk_ring_prod *q)
{
	return xsk_ring_prod__needs_wakeup(q);
}
#else
static int
tx_syscall_needed(struct xsk_ring_prod *q __rte_unused)
{
	return 1;
}
#endif

#ifdef RTE_LIBRTE_AF_XDP_PMD_BPF_LINK
static int link_lookup(int ifindex, int *link_fd)
{
	struct bpf_link_info link_info;
	__u32 link_len;
	__u32 id = 0;
	int err;
	int fd;

	while (true) {
		err = bpf_link_get_next_id(id, &id);
		if (err) {
			if (errno == ENOENT) {
				err = 0;
				break;
			}
			break;
		}

		fd = bpf_link_get_fd_by_id(id);
		if (fd < 0) {
			if (errno == ENOENT)
				continue;
			err = -errno;
			break;
		}

		link_len = sizeof(struct bpf_link_info);
		memset(&link_info, 0, link_len);
		err = bpf_obj_get_info_by_fd(fd, &link_info, &link_len);
		if (err) {
			close(fd);
			break;
		}
		if (link_info.type == BPF_LINK_TYPE_XDP) {
			if ((int)link_info.xdp.ifindex == ifindex) {
				*link_fd = fd;
				break;
			}
		}
		close(fd);
	}

	return err;
}

static bool probe_bpf_link(void)
{
	DECLARE_LIBBPF_OPTS(bpf_link_create_opts, opts,
			    .flags = XDP_FLAGS_SKB_MODE);
	struct bpf_load_program_attr prog_attr;
	struct bpf_insn insns[2];
	int prog_fd, link_fd = -1;
	int ifindex_lo = 1;
	bool ret = false;
	int err;

	err = link_lookup(ifindex_lo, &link_fd);
	if (err)
		return ret;

	if (link_fd >= 0)
		return true;

	/* BPF_MOV64_IMM(BPF_REG_0, XDP_PASS), */
	insns[0].code = BPF_ALU64 | BPF_MOV | BPF_K;
	insns[0].dst_reg = BPF_REG_0;
	insns[0].imm = XDP_PASS;

	/* BPF_EXIT_INSN() */
	insns[1].code = BPF_JMP | BPF_EXIT;

	memset(&prog_attr, 0, sizeof(prog_attr));
	prog_attr.prog_type = BPF_PROG_TYPE_XDP;
	prog_attr.insns = insns;
	prog_attr.insns_cnt = RTE_DIM(insns);
	prog_attr.license = "GPL";

	prog_fd = bpf_load_program_xattr(&prog_attr, NULL, 0);
	if (prog_fd < 0)
		return ret;

	link_fd = bpf_link_create(prog_fd, ifindex_lo, BPF_XDP, &opts);
	close(prog_fd);

	if (link_fd >= 0) {
		ret = true;
		close(link_fd);
	}

	return ret;
}

static int link_xdp_program(int if_index, int prog_fd, bool use_bpf_link)
{
	DECLARE_LIBBPF_OPTS(bpf_link_create_opts, opts);
	int link_fd, ret = 0;

	if (!use_bpf_link)
		return bpf_set_link_xdp_fd(if_index, prog_fd,
					   XDP_FLAGS_UPDATE_IF_NOEXIST);

	opts.flags = 0;
	link_fd = bpf_link_create(prog_fd, if_index, BPF_XDP, &opts);
	if (link_fd < 0)
		ret = -1;

	return ret;
}
#else
static bool probe_bpf_link(void)
{
	return false;
}

static int link_xdp_program(int if_index, int prog_fd,
			    bool use_bpf_link __rte_unused)
{
	return bpf_set_link_xdp_fd(if_index, prog_fd,
				   XDP_FLAGS_UPDATE_IF_NOEXIST);
}
#endif
