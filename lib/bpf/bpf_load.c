/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <eal_export.h>
#include <rte_log.h>
#include <rte_errno.h>

#include "bpf_impl.h"

static struct rte_bpf *
bpf_load(const struct rte_bpf_prm_ex *prm)
{
	uint8_t *buf;
	struct rte_bpf *bpf;
	size_t sz, bsz, insz, xsz;

	xsz =  prm->nb_xsym * sizeof(prm->xsym[0]);
	insz = prm->raw.nb_ins * sizeof(prm->raw.ins[0]);
	bsz = sizeof(bpf[0]);
	sz = insz + xsz + bsz;

	buf = mmap(NULL, sz, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (buf == MAP_FAILED)
		return NULL;

	bpf = (void *)buf;
	bpf->sz = sz;

	memcpy(&bpf->prm, prm, sizeof(bpf->prm));

	if (xsz > 0)
		memcpy(buf + bsz, prm->xsym, xsz);
	memcpy(buf + bsz + xsz, prm->raw.ins, insz);

	bpf->prm.xsym = (void *)(buf + bsz);
	bpf->prm.raw.ins = (void *)(buf + bsz + xsz);

	return bpf;
}

/*
 * Check that user provided external symbol.
 */
static int
bpf_check_xsym(const struct rte_bpf_xsym *xsym)
{
	uint32_t i;

	if (xsym->name == NULL)
		return -EINVAL;

	if (xsym->type == RTE_BPF_XTYPE_VAR) {
		if (xsym->var.desc.type == RTE_BPF_ARG_UNDEF)
			return -EINVAL;
	} else if (xsym->type == RTE_BPF_XTYPE_FUNC) {

		if (xsym->func.nb_args > EBPF_FUNC_MAX_ARGS)
			return -EINVAL;

		/* check function arguments */
		for (i = 0; i != xsym->func.nb_args; i++) {
			if (xsym->func.args[i].type == RTE_BPF_ARG_UNDEF)
				return -EINVAL;
		}

		/* check return value info */
		if (xsym->func.ret.type != RTE_BPF_ARG_UNDEF &&
				xsym->func.ret.size == 0)
			return -EINVAL;
	} else
		return -EINVAL;

	return 0;
}

static int
bpf_check_xsyms(const struct rte_bpf_xsym *xsym, uint32_t nb_xsym)
{
	int32_t rc;
	uint32_t i;

	if (nb_xsym != 0 && xsym == NULL)
		return -EINVAL;

	rc = 0;
	for (i = 0; i != nb_xsym && rc == 0; i++)
		rc = bpf_check_xsym(xsym + i);

	if (rc != 0) {
		RTE_BPF_LOG_FUNC_LINE(ERR, "%d-th xsym is invalid", i);
		return rc;
	}

	return 0;
}

static int
bpf_load_raw(struct __rte_bpf_load *load)
{
	const struct rte_bpf_prm_ex *const prm = &load->prm;
	struct rte_bpf *bpf;
	int32_t rc;

	RTE_ASSERT(prm->origin == RTE_BPF_ORIGIN_RAW);

	if (prm->raw.ins == NULL || prm->raw.nb_ins == 0)
		return -EINVAL;

	bpf = bpf_load(prm);
	if (bpf == NULL)
		return -ENOMEM;

	rc = __rte_bpf_validate(&load->prm, &bpf->stack_sz);
	if (rc == 0) {
		__rte_bpf_jit(bpf);
		if (mprotect(bpf, bpf->sz, PROT_READ) != 0)
			rc = -ENOMEM;
	}

	if (rc != 0) {
		rte_bpf_destroy(bpf);
		return rc;
	}

	load->bpf = bpf;
	return 0;
}

RTE_EXPORT_SYMBOL(rte_bpf_load)
struct rte_bpf *
rte_bpf_load(const struct rte_bpf_prm *prm)
{
	if (prm == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	return rte_bpf_load_ex(&(struct rte_bpf_prm_ex){
			.sz = sizeof(struct rte_bpf_prm_ex),
			.origin = RTE_BPF_ORIGIN_RAW,
			.raw.ins = prm->ins,
			.raw.nb_ins = prm->nb_ins,
			.xsym = prm->xsym,
			.nb_xsym = prm->nb_xsym,
			.prog_arg[0] = prm->prog_arg,
			.nb_prog_arg = 1,
		});
}

RTE_EXPORT_SYMBOL(rte_bpf_elf_load)
struct rte_bpf *
rte_bpf_elf_load(const struct rte_bpf_prm *prm, const char *fname,
	const char *sname)
{
	if (prm == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	return rte_bpf_load_ex(&(struct rte_bpf_prm_ex){
			.sz = sizeof(struct rte_bpf_prm_ex),
			.origin = RTE_BPF_ORIGIN_ELF_FILE,
			.elf_file.path = fname,
			.elf_file.section = sname,
			.xsym = prm->xsym,
			.nb_xsym = prm->nb_xsym,
			.prog_arg[0] = prm->prog_arg,
			.nb_prog_arg = 1,
		});
}

/*
 * Check extensible opts for invalid size or non-zero unsupported members.
 *
 * This code provides forward compatibility with applications compiled against
 * newer version of this library. `opts_sz` is the size of struct `opts` in the
 * version used for compiling the application, read from the member `sz`;
 * `type_sz` is the size of the same struct in the version used for compiling
 * the library.
 *
 * If new fields were added to the struct in the application version, `opts_sz`
 * will be greater than `type_sz`. In this case we are making sure all bytes we
 * don't know how to interpret are zeroes, that is any new features that are
 * there are not being used.
 *
 * This function can be used to check any struct following this convention.
 */
static bool
opts_valid(const void *opts, size_t opts_sz, size_t type_sz)
{
	if (opts == NULL)
		return true;

	if (opts_sz < sizeof(opts_sz))
		/* Size of the struct is too small even for sz member. */
		return false;

	/* Verify that all extra bytes are zeroed. */
	for (size_t offset = type_sz; offset < opts_sz; ++offset)
		if (((const char *)opts)[offset] != 0)
			return false;

	return true;
}

static int
load_try(struct __rte_bpf_load *load, const struct rte_bpf_prm_ex *app_prm)
{
	int rc;

	if (app_prm == NULL || !opts_valid(app_prm, app_prm->sz, sizeof(load->prm)))
		return -EINVAL;

	/*
	 * Convert extensible prm of application size to the size known to us.
	 *
	 * This code provides compatibility with applications compiled against
	 * different version of this library. `app_prm->sz` is the size of
	 * struct `rte_bpf_prm_ex` in the version used for compiling the
	 * application; `sizeof(load->prm)` is the size of the same struct in
	 * the version used for compiling the library.
	 *
	 * We are copying only the fields known to the application and leave
	 * the rest filled with zeroes. Any features not known to the
	 * application will have backward-compatible default behaviour.
	 */
	memcpy(&load->prm, app_prm, RTE_MIN(app_prm->sz, sizeof(load->prm)));
	load->prm.sz = sizeof(load->prm);

	rc = bpf_check_xsyms(load->prm.xsym, load->prm.nb_xsym);

	/* Convert prm origin to raw unless it already is. */
	switch (load->prm.origin) {
	case RTE_BPF_ORIGIN_RAW:
		break;
	case RTE_BPF_ORIGIN_ELF_FILE:
		rc = rc < 0 ? rc : __rte_bpf_load_elf_file(load);
		rc = rc < 0 ? rc : __rte_bpf_load_elf_code(load);
		break;
	default:
		rc = rc < 0 ? rc : -EINVAL;
	}

	/* Now that it is raw load it as such. */
	rc = rc < 0 ? rc : bpf_load_raw(load);

	return rc;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bpf_load_ex, 26.07)
struct rte_bpf *
rte_bpf_load_ex(const struct rte_bpf_prm_ex *prm)
{
	struct __rte_bpf_load load = { .elf_fd = -1 };

	const int rc = load_try(&load, prm);

	__rte_bpf_load_elf_cleanup(&load);

	RTE_ASSERT((rc < 0) == (load.bpf == NULL));

	if (rc < 0) {
		RTE_BPF_LOG_FUNC_LINE(ERR, "failed, error code: %d", -rc);
		rte_errno = -rc;
		return NULL;
	}

	RTE_BPF_LOG_FUNC_LINE(INFO, "successfully creates %p(jit={.func=%p,.sz=%zu});",
		load.bpf, load.bpf->jit.raw, load.bpf->jit.sz);
	return load.bpf;
}
