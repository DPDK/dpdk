/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_elf.h"

#include <malloc.h>
#include <stdbool.h>
#include <ethdev_pci.h>

#include <nfp_platform.h>
#include <rte_common.h>
#include <eal_firmware.h>

#include "nfp_logs.h"
#include "nfp_mip.h"

/*
 * NFP Chip Families.
 *
 * These are not enums, because they need to be microcode compatible.
 * They are also not maskable.
 *
 * Note: The NFP-4xxx family is handled as NFP-6xxx in most software
 * components.
 */
#define NFP_CHIP_FAMILY_NFP3800 0x3800
#define NFP_CHIP_FAMILY_NFP6000 0x6000

/* Standard ELF */
#define NFP_ELF_EI_NIDENT     16
#define NFP_ELF_EI_MAG0       0
#define NFP_ELF_EI_MAG1       1
#define NFP_ELF_EI_MAG2       2
#define NFP_ELF_EI_MAG3       3
#define NFP_ELF_EI_CLASS      4
#define NFP_ELF_EI_DATA       5
#define NFP_ELF_EI_VERSION    6
#define NFP_ELF_EI_PAD        7
#define NFP_ELF_ELFMAG0       0x7f
#define NFP_ELF_ELFMAG1       'E'
#define NFP_ELF_ELFMAG2       'L'
#define NFP_ELF_ELFMAG3       'F'
#define NFP_ELF_ELFCLASSNONE  0
#define NFP_ELF_ELFCLASS32    1
#define NFP_ELF_ELFCLASS64    2
#define NFP_ELF_ELFDATANONE   0
#define NFP_ELF_ELFDATA2LSB   1
#define NFP_ELF_ELFDATA2MSB   2

#define NFP_ELF_ET_NONE       0
#define NFP_ELF_ET_REL        1
#define NFP_ELF_ET_EXEC       2
#define NFP_ELF_ET_DYN        3
#define NFP_ELF_ET_CORE       4
#define NFP_ELF_ET_LOPROC     0xFF00
#define NFP_ELF_ET_HIPROC     0xFFFF
#define NFP_ELF_ET_NFP_PARTIAL_REL   (NFP_ELF_ET_LOPROC + NFP_ELF_ET_REL)
#define NFP_ELF_ET_NFP_PARTIAL_EXEC  (NFP_ELF_ET_LOPROC + NFP_ELF_ET_EXEC)

#define NFP_ELF_EM_NFP        250
#define NFP_ELF_EM_NFP6000    0x6000

#define NFP_ELF_SHT_NULL      0
#define NFP_ELF_SHT_PROGBITS  1
#define NFP_ELF_SHT_SYMTAB    2
#define NFP_ELF_SHT_STRTAB    3
#define NFP_ELF_SHT_RELA      4
#define NFP_ELF_SHT_HASH      5
#define NFP_ELF_SHT_DYNAMIC   6
#define NFP_ELF_SHT_NOTE      7
#define NFP_ELF_SHT_NOBITS    8
#define NFP_ELF_SHT_REL       9
#define NFP_ELF_SHT_SHLIB     10
#define NFP_ELF_SHT_DYNSYM    11
#define NFP_ELF_SHT_LOPROC    0x70000000
#define NFP_ELF_SHT_HIPROC    0x7fffffff
#define NFP_ELF_SHT_LOUSER    0x80000000
#define NFP_ELF_SHT_HIUSER    0x8fffffff

#define NFP_ELF_EV_NONE       0
#define NFP_ELF_EV_CURRENT    1

#define NFP_ELF_SHN_UNDEF     0

/* EM_NFP ELF flags */

/*
 * Valid values for FAMILY are:
 * 0x6000 - NFP-6xxx/NFP-4xxx
 * 0x3800 - NFP-38xx
 */
#define NFP_ELF_EF_NFP_FAMILY_MASK        0xFFFF
#define NFP_ELF_EF_NFP_FAMILY_LSB         8

#define NFP_ELF_SHT_NFP_MECONFIG          (NFP_ELF_SHT_LOPROC + 1)
#define NFP_ELF_SHT_NFP_INITREG           (NFP_ELF_SHT_LOPROC + 2)
#define NFP_ELF_SHT_UOF_DEBUG             (NFP_ELF_SHT_LOUSER)

/* NFP target revision note type */
#define NFP_ELT_NOTE_NAME_NFP             "NFP\0"
#define NFP_ELT_NOTE_NAME_NFP_SZ          4
#define NFP_ELT_NOTE_NAME_NFP_USER        "NFP_USR\0"
#define NFP_ELT_NOTE_NAME_NFP_USER_SZ     8
#define NFP_ELF_NT_NFP_BUILD_INFO         0x100
#define NFP_ELF_NT_NFP_REVS               0x101
#define NFP_ELF_NT_NFP_MIP_LOCATION       0x102
#define NFP_ELF_NT_NFP_USER               0xf0000000


/* Standard ELF structures */
struct nfp_elf_elf64_ehdr {
	uint8_t e_ident[NFP_ELF_EI_NIDENT];
	rte_le16_t e_type;
	rte_le16_t e_machine;
	rte_le32_t e_version;
	rte_le64_t e_entry;
	rte_le64_t e_phoff;
	rte_le64_t e_shoff;
	rte_le32_t e_flags;
	rte_le16_t e_ehsize;
	rte_le16_t e_phentsize;
	rte_le16_t e_phnum;
	rte_le16_t e_shentsize;
	rte_le16_t e_shnum;
	rte_le16_t e_shstrndx;
};

struct nfp_elf_elf64_shdr {
	rte_le32_t sh_name;
	rte_le32_t sh_type;
	rte_le64_t sh_flags;
	rte_le64_t sh_addr;
	rte_le64_t sh_offset;
	rte_le64_t sh_size;
	rte_le32_t sh_link;
	rte_le32_t sh_info;
	rte_le64_t sh_addralign;
	rte_le64_t sh_entsize;
};

struct nfp_elf_elf64_sym {
	rte_le32_t st_name;
	uint8_t st_info;
	uint8_t st_other;
	rte_le16_t st_shndx;
	rte_le64_t st_value;
	rte_le64_t st_size;
};

struct nfp_elf_elf64_rel {
	rte_le64_t r_offset;
	rte_le64_t r_info;
};

struct nfp_elf_elf64_nhdr {
	rte_le32_t n_namesz;
	rte_le32_t n_descsz;
	rte_le32_t n_type;
};

/* NFP specific structures */
struct nfp_elf_elf_meconfig {
	rte_le32_t ctx_enables;
	rte_le32_t entry;
	rte_le32_t misc_control;
	rte_le32_t reserved;
};

struct nfp_elf_elf_initregentry {
	rte_le32_t w0;
	rte_le32_t cpp_offset_lo;
	rte_le32_t val;
	rte_le32_t mask;
};

/* NFP NFFW ELF struct and API */
struct nfp_elf_user_note {
	const char *name;
	uint32_t data_sz;
	void *data;
};

/*
 * nfp_elf_fw_mip contains firmware related fields from the MIP as well as the
 * MIP location in the NFFW file. All fields are only valid if shndx > 0.
 *
 * This struct will only be available if the firmware contains a .note section
 * with a note of type NFP_ELF_NT_NFP_MIP_LOCATION.
 */
struct nfp_elf_fw_mip {
	size_t shndx;
	uint64_t sh_offset;
	rte_le32_t mip_ver;      /**< Version of the format of the MIP itself */

	rte_le32_t fw_version;
	rte_le32_t fw_buildnum;
	rte_le32_t fw_buildtime;
	char fw_name[20];        /**< At most 16 chars, 17 ensures '\0', round up */
	const char *fw_typeid;   /**< NULL if none set */
};

/*
 * It is preferred to access this struct via the nfp_elf functions
 * rather than directly.
 */
struct nfp_elf {
	struct nfp_elf_elf64_ehdr *ehdr;
	struct nfp_elf_elf64_shdr *shdrs;
	size_t shdrs_cnt;
	void **shdrs_data;

	/** True if section data has been endian swapped */
	uint8_t *shdrs_host_endian;

	size_t shdr_idx_symtab;

	struct nfp_elf_elf64_sym *syms;
	size_t syms_cnt;

	char *shstrtab;
	size_t shstrtab_sz;

	char *symstrtab;
	size_t symstrtab_sz;

	struct nfp_elf_elf_meconfig *meconfs;
	size_t meconfs_cnt;

	/* ==== .note data start ==== */

	/**
	 * Following data derived from SHT_NOTE sections for read-only usage.
	 * These fields are not used in nfp_elf_to_buf()
	 */
	int rev_min; /**< -1 if file did not specify */
	int rev_max; /**< -1 if file did not specify */

	/**
	 * If mip_shndx == 0 and mip_sh_off == 0, the .note stated there is no MIP.
	 * If mip_shndx == 0 and mip_sh_off == UINT64_MAX, there was no .note and
	 * a MIP _may_ still be found in the first 256KiB of DRAM/EMEM data.
	 */
	size_t mip_shndx; /**< Section in which MIP resides, 0 if no MIP */
	uint64_t mip_sh_off; /**< Offset within section (not address) */

	struct nfp_elf_fw_mip fw_mip;
	const char *fw_info_strtab;
	size_t fw_info_strtab_sz;

	/* ==== .note.user data start ==== */
	size_t user_note_cnt;
	struct nfp_elf_user_note *user_notes;

	void *dbgdata;

	int family;

	/**
	 * For const entry points in the API, we allocate and keep a buffer
	 * and for mutable entry points we assume the buffer remains valid
	 * and we just set pointers to it.
	 */
	void *_buf;
	size_t _bufsz;
};

static void
nfp_elf_free(struct nfp_elf *ectx)
{
	if (ectx == NULL)
		return;

	free(ectx->shdrs);
	free(ectx->shdrs_data);
	free(ectx->shdrs_host_endian);
	if (ectx->_bufsz != 0)
		free(ectx->_buf);

	free(ectx);
}

static size_t
nfp_elf_get_sec_ent_cnt(struct nfp_elf *ectx,
		size_t idx)
{
	uint64_t sh_size = rte_le_to_cpu_64(ectx->shdrs[idx].sh_size);
	uint64_t sh_entsize = rte_le_to_cpu_64(ectx->shdrs[idx].sh_entsize);

	if (sh_entsize != 0)
		return sh_size / sh_entsize;

	return 0;
}

static bool
nfp_elf_check_sh_size(uint64_t sh_size)
{
	if (sh_size == 0 || sh_size > UINT32_MAX)
		return false;

	return true;
}

static const char *
nfp_elf_fwinfo_next(struct nfp_elf *ectx,
		const char *key_val)
{
	size_t s_len;
	const char *strtab = ectx->fw_info_strtab;
	ssize_t tab_sz = (ssize_t)ectx->fw_info_strtab_sz;

	if (key_val == NULL)
		return strtab;

	s_len = strlen(key_val);
	if (key_val < strtab || ((key_val + s_len + 1) >= (strtab + tab_sz - 1)))
		return NULL;

	key_val += s_len + 1;

	return key_val;
}

static const char *
nfp_elf_fwinfo_lookup(const char *strtab,
		ssize_t tab_sz,
		const char *key)
{
	size_t s_len;
	const char *s;
	size_t key_len = strlen(key);

	if (strtab == NULL)
		return NULL;

	for (s = strtab, s_len = strlen(s) + 1;
			(s[0] != '\0') && (tab_sz > 0);
			s_len = strlen(s) + 1, tab_sz -= s_len, s += s_len) {
		if ((strncmp(s, key, key_len) == 0) && (s[key_len] == '='))
			return &s[key_len + 1];
	}

	return NULL;
}

static bool
nfp_elf_arch_is_thornham(struct nfp_elf *ectx)
{
	if (ectx == NULL)
		return false;

	if (ectx->family == NFP_CHIP_FAMILY_NFP6000 || ectx->family == NFP_CHIP_FAMILY_NFP3800)
		return true;

	return false;
}

static int
nfp_elf_parse_sht_rel(struct nfp_elf_elf64_shdr *sec,
		struct nfp_elf *ectx,
		size_t idx,
		uint8_t *buf8)
{
	uint64_t sh_size = rte_le_to_cpu_64(sec->sh_size);
	uint64_t sh_offset = rte_le_to_cpu_64(sec->sh_offset);
	uint64_t sh_entsize = rte_le_to_cpu_64(sec->sh_entsize);

	if (sh_entsize != sizeof(struct nfp_elf_elf64_rel)) {
		PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
		return -EINVAL;
	}

	if (!nfp_elf_check_sh_size(sh_size)) {
		PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
		return -EINVAL;
	}

	ectx->shdrs_data[idx] = buf8 + sh_offset;
	ectx->shdrs_host_endian[idx] = 1;

	return 0;
}

static int
nfp_elf_parse_note_name_nfp(struct nfp_elf *ectx,
		size_t idx,
		uint32_t ndescsz,
		uint32_t ntype,
		const char *nname,
		rte_le32_t *descword)
{
	if (strncmp(nname, NFP_ELT_NOTE_NAME_NFP, NFP_ELT_NOTE_NAME_NFP_SZ) == 0) {
		switch (ntype) {
		case NFP_ELF_NT_NFP_REVS:
			if (ndescsz != 8) {
				PMD_DRV_LOG(ERR, "Invalid ELF NOTE descsz in section %zu.", idx);
				return -EINVAL;
			}

			ectx->rev_min = (int)rte_le_to_cpu_32(descword[0]);
			ectx->rev_max = (int)rte_le_to_cpu_32(descword[1]);
			break;
		case NFP_ELF_NT_NFP_MIP_LOCATION:
			if (ndescsz != 12) {
				PMD_DRV_LOG(ERR, "Invalid ELF NOTE descsz in section %zu.", idx);
				return -EINVAL;
			}

			ectx->mip_shndx = rte_le_to_cpu_32(descword[0]);
			if (ectx->mip_shndx == 0) {
				ectx->mip_sh_off = 0;
				break;
			}

			if (ectx->mip_shndx >= ectx->shdrs_cnt) {
				PMD_DRV_LOG(ERR, "Invalid ELF NOTE shndx in section %zu.", idx);
				return -EINVAL;
			}

			ectx->mip_sh_off = rte_le_to_cpu_32(descword[1]) |
					(uint64_t)rte_le_to_cpu_32(descword[2]) << 32;
			break;
		default:
			break;
		}
	} else if (strncmp(nname, NFP_ELT_NOTE_NAME_NFP_USER,
			NFP_ELT_NOTE_NAME_NFP_USER_SZ) == 0 && ntype == NFP_ELF_NT_NFP_USER) {
		ectx->user_note_cnt++;
	}

	return 0;
}

static int
nfp_elf_parse_sht_note(struct nfp_elf_elf64_shdr *sec,
		struct nfp_elf *ectx,
		size_t idx,
		uint8_t *buf8)
{
	int err;
	size_t nsz;
	uint8_t *desc;
	uint32_t ntype;
	uint32_t nnamesz;
	uint32_t ndescsz;
	const char *nname;
	uint8_t *shdrs_data;
	rte_le32_t *descword;
	struct nfp_elf_elf64_nhdr *nhdr;
	struct nfp_elf_user_note *unote;
	uint64_t sh_size = rte_le_to_cpu_64(sec->sh_size);
	uint64_t sh_offset = rte_le_to_cpu_64(sec->sh_offset);

	if (!nfp_elf_check_sh_size(sh_size)) {
		PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
		return -EINVAL;
	}

	shdrs_data = buf8 + sh_offset;
	ectx->shdrs_data[idx] = shdrs_data;
	ectx->shdrs_host_endian[idx] = 0;

	/* Extract notes that we recognise */
	nhdr = (struct nfp_elf_elf64_nhdr *)shdrs_data;

	while ((uint8_t *)nhdr < (shdrs_data + sh_size)) {
		nnamesz  = rte_le_to_cpu_32(nhdr->n_namesz);
		ndescsz  = rte_le_to_cpu_32(nhdr->n_descsz);
		ntype    = rte_le_to_cpu_32(nhdr->n_type);
		nname    = (const char *)((uint8_t *)nhdr + sizeof(*nhdr));
		descword = (rte_le32_t *)((uint8_t *)nhdr + sizeof(*nhdr) +
				((nnamesz + UINT32_C(3)) & ~UINT32_C(3)));

		err = nfp_elf_parse_note_name_nfp(ectx, idx, ndescsz, ntype, nname, descword);
		if (err != 0)
			return err;

		nhdr = (struct nfp_elf_elf64_nhdr *)((uint8_t *)descword +
				((ndescsz + UINT32_C(3)) & ~UINT32_C(3)));
	}

	if (ectx->user_note_cnt == 0)
		return 0;

	ectx->user_notes = calloc(ectx->user_note_cnt, sizeof(*ectx->user_notes));
	if (ectx->user_notes == NULL) {
		PMD_DRV_LOG(ERR, "Out of memory.");
		return -ENOMEM;
	}

	nhdr = (struct nfp_elf_elf64_nhdr *)shdrs_data;
	unote = ectx->user_notes;
	while ((uint8_t *)nhdr < (shdrs_data + sh_size)) {
		nnamesz = rte_le_to_cpu_32(nhdr->n_namesz);
		ndescsz = rte_le_to_cpu_32(nhdr->n_descsz);
		ntype   = rte_le_to_cpu_32(nhdr->n_type);
		nname   = (const char *)((uint8_t *)nhdr + sizeof(*nhdr));
		desc    = (uint8_t *)nhdr + sizeof(*nhdr) +
				((nnamesz + UINT32_C(3)) & ~UINT32_C(3));

		if (strncmp(nname, NFP_ELT_NOTE_NAME_NFP_USER,
				NFP_ELT_NOTE_NAME_NFP_USER_SZ) != 0)
			continue;

		if (ntype != NFP_ELF_NT_NFP_USER)
			continue;

		unote->name = (const char *)desc;
		nsz = strlen(unote->name) + 1;
		if (nsz % 4 != 0)
			nsz = ((nsz / 4) + 1) * 4;
		if (nsz > ndescsz) {
			PMD_DRV_LOG(ERR, "Invalid ELF USER NOTE descsz in section %zu.", idx);
			return -EINVAL;
		}

		unote->data_sz = ndescsz - (uint32_t)nsz;
		if (unote->data_sz != 0)
			unote->data = desc + nsz;
		unote++;

		nhdr = (struct nfp_elf_elf64_nhdr *)
				(desc + ((ndescsz + UINT32_C(3)) & ~UINT32_C(3)));
	}

	return 0;
}

static int
nfp_elf_parse_sht_meconfig(struct nfp_elf_elf64_shdr *sec,
		struct nfp_elf *ectx,
		size_t idx,
		uint8_t *buf8)
{
	size_t ent_cnt;
	uint8_t *shdrs_data;
	uint64_t sh_size = rte_le_to_cpu_64(sec->sh_size);
	uint64_t sh_offset = rte_le_to_cpu_64(sec->sh_offset);

	if (!nfp_elf_check_sh_size(sh_size)) {
		PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
		return -EINVAL;
	}

	shdrs_data = buf8 + sh_offset;
	ent_cnt = nfp_elf_get_sec_ent_cnt(ectx, idx);
	ectx->shdrs_data[idx] = shdrs_data;
	ectx->meconfs = (struct nfp_elf_elf_meconfig *)shdrs_data;
	ectx->meconfs_cnt = ent_cnt;
	ectx->shdrs_host_endian[idx] = 1;

	return 0;
}

static int
nfp_elf_parse_sht_initreg(struct nfp_elf_elf64_shdr *sec,
		struct nfp_elf *ectx,
		size_t idx,
		uint8_t *buf8)
{
	uint64_t sh_size = rte_le_to_cpu_64(sec->sh_size);
	uint64_t sh_offset = rte_le_to_cpu_64(sec->sh_offset);
	uint64_t sh_entsize = rte_le_to_cpu_64(sec->sh_entsize);

	if (!nfp_elf_arch_is_thornham(ectx)) {
		PMD_DRV_LOG(ERR, "Section not supported for target arch.");
		return -ENOTSUP;
	}

	if (sh_entsize != sizeof(struct nfp_elf_elf_initregentry) ||
			!nfp_elf_check_sh_size(sh_size)) {
		PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
		return -EINVAL;
	}

	ectx->shdrs_data[idx] = buf8 + sh_offset;
	ectx->shdrs_host_endian[idx] = 1;

	return 0;
}

static int
nfp_elf_parse_sht_symtab(struct nfp_elf_elf64_shdr *sec,
		struct nfp_elf *ectx,
		size_t idx,
		uint8_t *buf8)
{
	uint64_t sh_size = rte_le_to_cpu_64(sec->sh_size);
	uint64_t sh_offset = rte_le_to_cpu_64(sec->sh_offset);
	uint64_t sh_entsize = rte_le_to_cpu_64(sec->sh_entsize);

	if (sh_entsize != sizeof(struct nfp_elf_elf64_sym) ||
			!nfp_elf_check_sh_size(sh_size)) {
		PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
		return -EINVAL;
	}

	ectx->shdrs_data[idx] = buf8 + sh_offset;
	ectx->shdrs_host_endian[ectx->shdr_idx_symtab] = 1;

	return 0;
}

static int
nfp_elf_populate_fw_mip(struct nfp_elf *ectx,
		uint8_t *buf8)
{
	uint8_t *pu8;
	const char *nx;
	ssize_t tab_sz;
	uint64_t sh_size;
	const char *str_tab;
	uint64_t sh_offset;
	uint32_t first_entry;
	const struct nfp_mip *mip;
	struct nfp_elf_elf64_shdr *sec;
	const struct nfp_mip_entry *ent;
	const struct nfp_mip_fwinfo_entry *fwinfo;

	sec = &ectx->shdrs[ectx->mip_shndx];
	sh_size = rte_le_to_cpu_64(sec->sh_size);
	sh_offset = rte_le_to_cpu_64(sec->sh_offset);
	pu8 = buf8 + sh_offset + ectx->mip_sh_off;
	mip = (const struct nfp_mip *)pu8;
	first_entry = rte_le_to_cpu_32(mip->first_entry);

	if (mip->signature != NFP_MIP_SIGNATURE) {
		PMD_DRV_LOG(ERR, "Incorrect MIP signature %#08x.",
				rte_le_to_cpu_32(mip->signature));
		return -EINVAL;
	}

	ectx->fw_mip.shndx = ectx->mip_shndx;
	ectx->fw_mip.sh_offset = ectx->mip_sh_off;
	ectx->fw_mip.mip_ver = mip->mip_version;

	if (ectx->fw_mip.mip_ver != NFP_MIP_VERSION) {
		PMD_DRV_LOG(ERR, "MIP note pointer does not point to recognised version.");
		return -EINVAL;
	}

	ectx->fw_mip.fw_version   = mip->version;
	ectx->fw_mip.fw_buildnum  = mip->buildnum;
	ectx->fw_mip.fw_buildtime = mip->buildtime;
	strncpy(ectx->fw_mip.fw_name, mip->name, 16);

	/*
	 * If there is a FWINFO v1 entry, it will be first and
	 * right after the MIP itself, so in the same section.
	 */
	if (ectx->mip_sh_off + first_entry + sizeof(*ent) < sh_size) {
		pu8 += first_entry;
		ent = (const struct nfp_mip_entry *)pu8;
		if (ent->type == NFP_MIP_TYPE_FWINFO && ent->version == 1) {
			pu8 += sizeof(*ent);
			fwinfo = (const struct nfp_mip_fwinfo_entry *)pu8;
			if (fwinfo->kv_len != 0) {
				ectx->fw_info_strtab_sz = fwinfo->kv_len;
				ectx->fw_info_strtab = fwinfo->key_value_strs;
			}
		}
	}

	str_tab = ectx->fw_info_strtab;
	tab_sz = (ssize_t)ectx->fw_info_strtab_sz;
	ectx->fw_mip.fw_typeid = nfp_elf_fwinfo_lookup(str_tab, tab_sz, "TypeId");

	/*
	 * TypeId will be the last reserved key-value pair, so skip
	 * to the first entry after it for the user values.
	 */
	if (ectx->fw_mip.fw_typeid == NULL)
		return 0;

	nx = nfp_elf_fwinfo_next(ectx, ectx->fw_mip.fw_typeid);
	if (nx == NULL)
		ectx->fw_info_strtab_sz = 0;
	else
		ectx->fw_info_strtab_sz -= (nx - ectx->fw_info_strtab);
	ectx->fw_info_strtab = nx;

	return 0;
}

static int
nfp_elf_read_file_headers(struct nfp_elf *ectx,
		void *buf)
{
	uint16_t e_type;
	uint32_t e_flags;
	uint32_t e_version;
	uint16_t e_machine;

	ectx->ehdr = buf;
	e_type = rte_le_to_cpu_16(ectx->ehdr->e_type);
	e_flags = rte_le_to_cpu_32(ectx->ehdr->e_flags);
	e_version = rte_le_to_cpu_32(ectx->ehdr->e_version);
	e_machine = rte_le_to_cpu_16(ectx->ehdr->e_machine);

	switch (e_machine) {
	case NFP_ELF_EM_NFP:
		ectx->family = (e_flags >> NFP_ELF_EF_NFP_FAMILY_LSB)
				& NFP_ELF_EF_NFP_FAMILY_MASK;
		break;
	case NFP_ELF_EM_NFP6000:
		ectx->family = NFP_CHIP_FAMILY_NFP6000;
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid ELF machine type.");
		return -EINVAL;
	}

	if ((e_type != NFP_ELF_ET_EXEC && e_type != NFP_ELF_ET_REL &&
			e_type != NFP_ELF_ET_NFP_PARTIAL_EXEC &&
			e_type != NFP_ELF_ET_NFP_PARTIAL_REL) ||
			e_version != NFP_ELF_EV_CURRENT ||
			ectx->ehdr->e_ehsize != sizeof(struct nfp_elf_elf64_ehdr) ||
			ectx->ehdr->e_shentsize != sizeof(struct nfp_elf_elf64_shdr)) {
		PMD_DRV_LOG(ERR, "Invalid ELF file header.");
		return -EINVAL;
	}

	if (ectx->ehdr->e_shoff < ectx->ehdr->e_ehsize) {
		PMD_DRV_LOG(ERR, "Invalid ELF header content.");
		return -EINVAL;
	}

	if (ectx->ehdr->e_shstrndx >= ectx->ehdr->e_shnum) {
		PMD_DRV_LOG(ERR, "Invalid ELF header content.");
		return -EINVAL;
	}

	return 0;
}

static int
nfp_elf_read_section_headers(struct nfp_elf *ectx,
		uint8_t *buf8,
		size_t buf_len)
{
	size_t idx;
	int err = 0;
	uint8_t *pu8;
	uint64_t sh_size;
	uint64_t sh_offset;
	uint64_t sh_entsize;
	struct nfp_elf_elf64_shdr *sec;
	uint64_t e_shoff = rte_le_to_cpu_16(ectx->ehdr->e_shoff);
	uint16_t e_shnum = rte_le_to_cpu_16(ectx->ehdr->e_shnum);

	if (buf_len < e_shoff + ((size_t)e_shnum * sizeof(*sec))) {
		PMD_DRV_LOG(ERR, "ELF data too short.");
		return -EINVAL;
	}

	pu8 = buf8 + e_shoff;

	if (e_shnum == 0) {
		ectx->shdrs = NULL;
		ectx->shdrs_data = NULL;
		ectx->shdrs_host_endian = NULL;
		ectx->shdrs_cnt = 0;
		return 0;
	}

	ectx->shdrs = calloc(e_shnum, sizeof(*ectx->shdrs));
	if (ectx->shdrs == NULL) {
		PMD_DRV_LOG(ERR, "Out of memory.");
		return -ENOMEM;
	}

	ectx->shdrs_data = calloc(e_shnum, sizeof(void *));
	if (ectx->shdrs_data == NULL) {
		PMD_DRV_LOG(ERR, "Out of memory.");
		err = -ENOMEM;
		goto free_shdrs;
	}

	ectx->shdrs_host_endian = calloc(e_shnum, sizeof(ectx->shdrs_host_endian[0]));
	if (ectx->shdrs_host_endian == NULL) {
		PMD_DRV_LOG(ERR, "Out of memory.");
		err = -ENOMEM;
		goto free_shdrs_data;
	}

	memcpy(ectx->shdrs, pu8, e_shnum * sizeof(*ectx->shdrs));
	ectx->shdrs_cnt = e_shnum;

	for (idx = 0, sec = ectx->shdrs; idx < ectx->shdrs_cnt; idx++, sec++) {
		sh_size = rte_le_to_cpu_64(sec->sh_size);
		sh_offset = rte_le_to_cpu_64(sec->sh_offset);
		sh_entsize = rte_le_to_cpu_64(sec->sh_entsize);

		if (sh_entsize != 0 && (sh_size % sh_entsize != 0)) {
			PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
			err = -EINVAL;
			goto free_shdrs_host_endian;
		}

		switch (rte_le_to_cpu_32(sec->sh_type)) {
		case NFP_ELF_SHT_REL:
			err = nfp_elf_parse_sht_rel(sec, ectx, idx, buf8);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to parse sht rel.");
				goto free_shdrs_host_endian;
			}
			break;
		case NFP_ELF_SHT_NOTE:
			err = nfp_elf_parse_sht_note(sec, ectx, idx, buf8);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to parse sht note.");
				goto free_shdrs_host_endian;
			}
			break;
		case NFP_ELF_SHT_NFP_MECONFIG:
			err = nfp_elf_parse_sht_meconfig(sec, ectx, idx, buf8);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to parse sht meconfig.");
				goto free_shdrs_host_endian;
			}
			break;
		case NFP_ELF_SHT_NFP_INITREG:
			err = nfp_elf_parse_sht_initreg(sec, ectx, idx, buf8);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to parse sht initregp.");
				goto free_shdrs_host_endian;
			}
			break;
		case NFP_ELF_SHT_SYMTAB:
			err = nfp_elf_parse_sht_symtab(sec, ectx, idx, buf8);
			if (err != 0) {
				PMD_DRV_LOG(ERR, "Failed to parse sht symtab.");
				goto free_shdrs_host_endian;
			}
			break;
		case NFP_ELF_SHT_NOBITS:
		case NFP_ELF_SHT_NULL:
			break;
		default:
			if (sh_offset > 0 && sh_size <= 0)
				break;

			/*
			 * Limit sections to 4GiB, because they won't need to be this large
			 * and this ensures we can handle the file on 32-bit hosts without
			 * unexpected problems.
			 */
			if (sh_size > UINT32_MAX) {
				PMD_DRV_LOG(ERR, "Invalid ELF section header, index %zu.", idx);
				err = -EINVAL;
				goto free_shdrs_host_endian;
			}

			pu8 = buf8 + sh_offset;
			ectx->shdrs_data[idx] = pu8;
			ectx->shdrs_host_endian[idx] = 0;
			break;
		}
	}

	return 0;

free_shdrs_host_endian:
	free(ectx->shdrs_host_endian);
free_shdrs_data:
	free(ectx->shdrs_data);
free_shdrs:
	free(ectx->shdrs);

	return err;
}

static int
nfp_elf_read_shstrtab(struct nfp_elf *ectx)
{
	struct nfp_elf_elf64_shdr *sec;
	uint16_t e_shstrndx = rte_le_to_cpu_16(ectx->ehdr->e_shstrndx);

	if (ectx->ehdr->e_shnum <= ectx->ehdr->e_shstrndx) {
		PMD_DRV_LOG(ERR, "Invalid Index.");
		return -EINVAL;
	}

	sec = &ectx->shdrs[e_shstrndx];
	if (sec == NULL || rte_le_to_cpu_32(sec->sh_type) != NFP_ELF_SHT_STRTAB) {
		PMD_DRV_LOG(ERR, "Invalid ELF shstrtab.");
		return -EINVAL;
	}

	ectx->shstrtab = ectx->shdrs_data[e_shstrndx];
	ectx->shstrtab_sz = rte_le_to_cpu_64(sec->sh_size);

	return 0;
}

static int
nfp_elf_read_first_symtab(struct nfp_elf *ectx)
{
	size_t idx;
	uint32_t sh_type;
	uint64_t sh_size;
	struct nfp_elf_elf64_shdr *sec = NULL;

	for (idx = 0; idx < ectx->shdrs_cnt; idx++) {
		sec = &ectx->shdrs[idx];
		if (sec != NULL) {
			sh_type = rte_le_to_cpu_32(sec->sh_type);
			if (sh_type == NFP_ELF_SHT_SYMTAB)
				break;
		}
	}

	if (sec == NULL)
		return -EINVAL;

	sh_size = rte_le_to_cpu_64(sec->sh_size);

	if (idx < ectx->shdrs_cnt && sh_type == NFP_ELF_SHT_SYMTAB) {
		ectx->shdr_idx_symtab = idx;
		ectx->syms = ectx->shdrs_data[idx];
		ectx->syms_cnt = nfp_elf_get_sec_ent_cnt(ectx, idx);

		/* Load symtab's strtab */
		idx = rte_le_to_cpu_32(sec->sh_link);

		if (idx == NFP_ELF_SHN_UNDEF || idx >= ectx->shdrs_cnt) {
			PMD_DRV_LOG(ERR, "ELF symtab has no strtab.");
			return -EINVAL;
		}

		sec = &ectx->shdrs[idx];
		sh_type = rte_le_to_cpu_32(sec->sh_type);
		if (sh_type != NFP_ELF_SHT_STRTAB) {
			PMD_DRV_LOG(ERR, "ELF symtab has no strtab.");
			return -EINVAL;
		}

		if (!nfp_elf_check_sh_size(sh_size)) {
			PMD_DRV_LOG(ERR, "ELF symtab has invalid strtab.");
			return -EINVAL;
		}

		ectx->symstrtab = ectx->shdrs_data[idx];
		ectx->symstrtab_sz = sh_size;
	}

	return 0;
}

static int
nfp_elf_is_valid_file(uint8_t *buf8)
{
	if (buf8[NFP_ELF_EI_MAG0] != NFP_ELF_ELFMAG0 ||
			buf8[NFP_ELF_EI_MAG1] != NFP_ELF_ELFMAG1 ||
			buf8[NFP_ELF_EI_MAG2] != NFP_ELF_ELFMAG2 ||
			buf8[NFP_ELF_EI_MAG3] != NFP_ELF_ELFMAG3 ||
			buf8[NFP_ELF_EI_VERSION] != NFP_ELF_EV_CURRENT ||
			buf8[NFP_ELF_EI_DATA] != NFP_ELF_ELFDATA2LSB)
		return -EINVAL;

	return 0;
}

static int
nfp_elf_is_valid_class(uint8_t *buf8)
{
	if (buf8[NFP_ELF_EI_CLASS] != NFP_ELF_ELFCLASS64)
		return -EINVAL;

	return 0;
}

static struct nfp_elf *
nfp_elf_mutable_buf(void *buf,
		size_t buf_len)
{
	int err = 0;
	uint8_t *buf8 = buf;
	struct nfp_elf *ectx;

	if (buf == NULL) {
		PMD_DRV_LOG(ERR, "Invalid parameters.");
		return NULL;
	}

	if (buf_len < sizeof(struct nfp_elf_elf64_ehdr)) {
		PMD_DRV_LOG(ERR, "ELF data too short.");
		return NULL;
	}

	ectx = calloc(1, sizeof(struct nfp_elf));
	if (ectx == NULL) {
		PMD_DRV_LOG(ERR, "Out of memory.");
		return NULL;
	}

	ectx->rev_min = -1;
	ectx->rev_max = -1;
	ectx->mip_sh_off = UINT64_MAX;

	err = nfp_elf_is_valid_file(buf8);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Not a valid ELF file.");
		goto elf_free;
	}

	err = nfp_elf_is_valid_class(buf8);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Unknown ELF class.");
		goto elf_free;
	}

	err = nfp_elf_read_file_headers(ectx, buf);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to read file headers.");
		goto elf_free;
	}

	err = nfp_elf_read_section_headers(ectx, buf8, buf_len);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to read section headers.");
		goto elf_free;
	}

	err = nfp_elf_read_shstrtab(ectx);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to read shstrtab.");
		goto elf_free;
	}

	/* Read first symtab if any, assuming it's the primary or only one */
	err = nfp_elf_read_first_symtab(ectx);
	if (err != 0) {
		PMD_DRV_LOG(ERR, "Failed to read first symtab.");
		goto elf_free;
	}

	/* Populate the fw_mip struct if we have a .note for it */
	if (ectx->mip_shndx != 0) {
		err = nfp_elf_populate_fw_mip(ectx, buf8);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Failed to populate the fw mip.");
			goto elf_free;
		}
	}

	ectx->_buf = buf;
	ectx->_bufsz = 0;

	return ectx;

elf_free:
	nfp_elf_free(ectx);

	return NULL;
}

int
nfp_elf_get_fw_version(uint32_t *fw_version,
		char *fw_name)
{
	void *fw_buf;
	size_t fsize;
	struct nfp_elf *elf;

	if (rte_firmware_read(fw_name, &fw_buf, &fsize) != 0) {
		PMD_DRV_LOG(ERR, "Firmware %s not found!", fw_name);
		return -ENOENT;
	}

	elf = nfp_elf_mutable_buf(fw_buf, fsize);
	if (elf == NULL) {
		PMD_DRV_LOG(ERR, "Parse nffw file failed.");
		free(fw_buf);
		return -EIO;
	}

	*fw_version = rte_le_to_cpu_32(elf->fw_mip.fw_version);

	nfp_elf_free(elf);
	free(fw_buf);
	return 0;
}

