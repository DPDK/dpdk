/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <fcntl.h>

#include <libelf.h>

#include <rte_common.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_memory.h>
#include <rte_eal.h>
#include <rte_byteorder.h>
#include <rte_errno.h>

#include "bpf_impl.h"

/* To overcome compatibility issue */
#ifndef EM_BPF
#define	EM_BPF	247
#endif

static uint32_t
bpf_find_xsym(const char *sn, enum rte_bpf_xtype type,
	const struct rte_bpf_xsym fp[], uint32_t fn)
{
	uint32_t i;

	if (sn == NULL || fp == NULL)
		return UINT32_MAX;

	for (i = 0; i != fn; i++) {
		if (fp[i].type == type && strcmp(sn, fp[i].name) == 0)
			break;
	}

	return (i != fn) ? i : UINT32_MAX;
}

/*
 * update BPF code at offset *ofs* with a proper address(index) for external
 * symbol *sn*
 */
static int
resolve_xsym(const char *sn, size_t ofs, struct ebpf_insn *ins, size_t ins_sz,
	const struct rte_bpf_prm *prm)
{
	uint32_t idx, fidx;
	enum rte_bpf_xtype type;

	if (ofs % sizeof(ins[0]) != 0 || ofs >= ins_sz)
		return -EINVAL;

	idx = ofs / sizeof(ins[0]);
	if (ins[idx].code == (BPF_JMP | EBPF_CALL))
		type = RTE_BPF_XTYPE_FUNC;
	else if (ins[idx].code == (BPF_LD | BPF_IMM | EBPF_DW) &&
			ofs < ins_sz - sizeof(ins[idx]))
		type = RTE_BPF_XTYPE_VAR;
	else
		return -EINVAL;

	fidx = bpf_find_xsym(sn, type, prm->xsym, prm->nb_xsym);
	if (fidx == UINT32_MAX)
		return -ENOENT;

	/* for function we just need an index in our xsym table */
	if (type == RTE_BPF_XTYPE_FUNC)
		ins[idx].imm = fidx;
	/* for variable we need to store its absolute address */
	else {
		ins[idx].imm = (uintptr_t)prm->xsym[fidx].var.val;
		ins[idx + 1].imm =
			(uint64_t)(uintptr_t)prm->xsym[fidx].var.val >> 32;
	}

	return 0;
}

static int
check_elf_header(const Elf64_Ehdr *eh)
{
	const char *err;

	err = NULL;

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	if (eh->e_ident[EI_DATA] != ELFDATA2LSB)
#else
	if (eh->e_ident[EI_DATA] != ELFDATA2MSB)
#endif
		err = "not native byte order";
	else if (eh->e_ident[EI_OSABI] != ELFOSABI_NONE)
		err = "unexpected OS ABI";
	else if (eh->e_type != ET_REL)
		err = "unexpected ELF type";
	else if (eh->e_machine != EM_NONE && eh->e_machine != EM_BPF)
		err = "unexpected machine type";

	if (err != NULL) {
		RTE_BPF_LOG(ERR, "%s(): %s\n", __func__, err);
		return -EINVAL;
	}

	return 0;
}

/*
 * helper function, find executable section by name.
 */
static int
find_elf_code(Elf *elf, const char *section, Elf_Data **psd, size_t *pidx)
{
	Elf_Scn *sc;
	const Elf64_Ehdr *eh;
	const Elf64_Shdr *sh;
	Elf_Data *sd;
	const char *sn;
	int32_t rc;

	eh = elf64_getehdr(elf);
	if (eh == NULL) {
		rc = elf_errno();
		RTE_BPF_LOG(ERR, "%s(%p, %s) error code: %d(%s)\n",
			__func__, elf, section, rc, elf_errmsg(rc));
		return -EINVAL;
	}

	if (check_elf_header(eh) != 0)
		return -EINVAL;

	/* find given section by name */
	for (sc = elf_nextscn(elf, NULL); sc != NULL;
			sc = elf_nextscn(elf, sc)) {
		sh = elf64_getshdr(sc);
		sn = elf_strptr(elf, eh->e_shstrndx, sh->sh_name);
		if (sn != NULL && strcmp(section, sn) == 0 &&
				sh->sh_type == SHT_PROGBITS &&
				sh->sh_flags == (SHF_ALLOC | SHF_EXECINSTR))
			break;
	}

	sd = elf_getdata(sc, NULL);
	if (sd == NULL || sd->d_size == 0 ||
			sd->d_size % sizeof(struct ebpf_insn) != 0) {
		rc = elf_errno();
		RTE_BPF_LOG(ERR, "%s(%p, %s) error code: %d(%s)\n",
			__func__, elf, section, rc, elf_errmsg(rc));
		return -EINVAL;
	}

	*psd = sd;
	*pidx = elf_ndxscn(sc);
	return 0;
}

/*
 * helper function to process data from relocation table.
 */
static int
process_reloc(Elf *elf, size_t sym_idx, Elf64_Rel *re, size_t re_sz,
	struct ebpf_insn *ins, size_t ins_sz, const struct rte_bpf_prm *prm)
{
	int32_t rc;
	uint32_t i, n;
	size_t ofs, sym;
	const char *sn;
	const Elf64_Ehdr *eh;
	Elf_Scn *sc;
	const Elf_Data *sd;
	Elf64_Sym *sm;

	eh = elf64_getehdr(elf);

	/* get symtable by section index */
	sc = elf_getscn(elf, sym_idx);
	sd = elf_getdata(sc, NULL);
	if (sd == NULL)
		return -EINVAL;
	sm = sd->d_buf;

	n = re_sz / sizeof(re[0]);
	for (i = 0; i != n; i++) {

		ofs = re[i].r_offset;

		/* retrieve index in the symtable */
		sym = ELF64_R_SYM(re[i].r_info);
		if (sym * sizeof(sm[0]) >= sd->d_size)
			return -EINVAL;

		sn = elf_strptr(elf, eh->e_shstrndx, sm[sym].st_name);

		rc = resolve_xsym(sn, ofs, ins, ins_sz, prm);
		if (rc != 0) {
			RTE_BPF_LOG(ERR,
				"resolve_xsym(%s, %zu) error code: %d\n",
				sn, ofs, rc);
			return rc;
		}
	}

	return 0;
}

/*
 * helper function, find relocation information (if any)
 * and update bpf code.
 */
static int
elf_reloc_code(Elf *elf, Elf_Data *ed, size_t sidx,
	const struct rte_bpf_prm *prm)
{
	Elf64_Rel *re;
	Elf_Scn *sc;
	const Elf64_Shdr *sh;
	const Elf_Data *sd;
	int32_t rc;

	rc = 0;

	/* walk through all sections */
	for (sc = elf_nextscn(elf, NULL); sc != NULL && rc == 0;
			sc = elf_nextscn(elf, sc)) {

		sh = elf64_getshdr(sc);

		/* relocation data for our code section */
		if (sh->sh_type == SHT_REL && sh->sh_info == sidx) {
			sd = elf_getdata(sc, NULL);
			if (sd == NULL || sd->d_size == 0 ||
					sd->d_size % sizeof(re[0]) != 0)
				return -EINVAL;
			rc = process_reloc(elf, sh->sh_link,
				sd->d_buf, sd->d_size, ed->d_buf, ed->d_size,
				prm);
		}
	}

	return rc;
}

static struct rte_bpf *
bpf_load_elf(const struct rte_bpf_prm *prm, int32_t fd, const char *section)
{
	Elf *elf;
	Elf_Data *sd;
	size_t sidx;
	int32_t rc;
	struct rte_bpf *bpf;
	struct rte_bpf_prm np;

	elf_version(EV_CURRENT);
	elf = elf_begin(fd, ELF_C_READ, NULL);

	rc = find_elf_code(elf, section, &sd, &sidx);
	if (rc == 0)
		rc = elf_reloc_code(elf, sd, sidx, prm);

	if (rc == 0) {
		np = prm[0];
		np.ins = sd->d_buf;
		np.nb_ins = sd->d_size / sizeof(struct ebpf_insn);
		bpf = rte_bpf_load(&np);
	} else {
		bpf = NULL;
		rte_errno = -rc;
	}

	elf_end(elf);
	return bpf;
}

__rte_experimental struct rte_bpf *
rte_bpf_elf_load(const struct rte_bpf_prm *prm, const char *fname,
	const char *sname)
{
	int32_t fd, rc;
	struct rte_bpf *bpf;

	if (prm == NULL || fname == NULL || sname == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	fd = open(fname, O_RDONLY);
	if (fd < 0) {
		rc = errno;
		RTE_BPF_LOG(ERR, "%s(%s) error code: %d(%s)\n",
			__func__, fname, rc, strerror(rc));
		rte_errno = EINVAL;
		return NULL;
	}

	bpf = bpf_load_elf(prm, fd, sname);
	close(fd);

	if (bpf == NULL) {
		RTE_BPF_LOG(ERR,
			"%s(fname=\"%s\", sname=\"%s\") failed, "
			"error code: %d\n",
			__func__, fname, sname, rte_errno);
		return NULL;
	}

	RTE_BPF_LOG(INFO, "%s(fname=\"%s\", sname=\"%s\") "
		"successfully creates %p(jit={.func=%p,.sz=%zu});\n",
		__func__, fname, sname, bpf, bpf->jit.func, bpf->jit.sz);
	return bpf;
}
