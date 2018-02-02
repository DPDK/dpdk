/* Postprocess pmd object files to export hw support
 *
 * Copyright 2016 Neil Horman <nhorman@tuxdriver.com>
 * Based in part on modpost.c from the linux kernel
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License V2, incorporated herein by reference.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>

#include <rte_common.h>
#include "pmdinfogen.h"

#ifdef RTE_ARCH_64
#define ADDR_SIZE 64
#else
#define ADDR_SIZE 32
#endif

static int use_stdin, use_stdout;

static const char *sym_name(struct elf_info *elf, Elf_Sym *sym)
{
	if (sym)
		return elf->strtab + sym->st_name;
	else
		return "(unknown)";
}

static void *grab_file(const char *filename, unsigned long *size)
{
	struct stat st;
	void *map = MAP_FAILED;
	int fd = -1;

	if (!use_stdin) {
		fd = open(filename, O_RDONLY);
		if (fd < 0)
			return NULL;
	} else {
		/* from stdin, use a temporary file to mmap */
		FILE *infile;
		char buffer[1024];
		int n;

		infile = tmpfile();
		if (infile == NULL) {
			perror("tmpfile");
			return NULL;
		}
		fd = dup(fileno(infile));
		fclose(infile);
		if (fd < 0)
			return NULL;

		n = read(STDIN_FILENO, buffer, sizeof(buffer));
		while (n > 0) {
			if (write(fd, buffer, n) != n)
				goto failed;
			n = read(STDIN_FILENO, buffer, sizeof(buffer));
		}
	}

	if (fstat(fd, &st))
		goto failed;

	*size = st.st_size;
	map = mmap(NULL, *size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);

failed:
	close(fd);
	if (map == MAP_FAILED)
		return NULL;
	return map;
}

/**
  * Return a copy of the next line in a mmap'ed file.
  * spaces in the beginning of the line is trimmed away.
  * Return a pointer to a static buffer.
  **/
static void release_file(void *file, unsigned long size)
{
	munmap(file, size);
}


static void *get_sym_value(struct elf_info *info, const Elf_Sym *sym)
{
	return RTE_PTR_ADD(info->hdr,
		info->sechdrs[sym->st_shndx].sh_offset + sym->st_value);
}

static Elf_Sym *find_sym_in_symtab(struct elf_info *info,
				   const char *name, Elf_Sym *last)
{
	Elf_Sym *idx;
	if (last)
		idx = last+1;
	else
		idx = info->symtab_start;

	for (; idx < info->symtab_stop; idx++) {
		const char *n = sym_name(info, idx);
		if (!strncmp(n, name, strlen(name)))
			return idx;
	}
	return NULL;
}

static int parse_elf(struct elf_info *info, const char *filename)
{
	unsigned int i;
	Elf_Ehdr *hdr;
	Elf_Shdr *sechdrs;
	Elf_Sym  *sym;
	int endian;
	unsigned int symtab_idx = ~0U, symtab_shndx_idx = ~0U;

	hdr = grab_file(filename, &info->size);
	if (!hdr) {
		perror(filename);
		exit(1);
	}
	info->hdr = hdr;
	if (info->size < sizeof(*hdr)) {
		/* file too small, assume this is an empty .o file */
		return 0;
	}
	/* Is this a valid ELF file? */
	if ((hdr->e_ident[EI_MAG0] != ELFMAG0) ||
	    (hdr->e_ident[EI_MAG1] != ELFMAG1) ||
	    (hdr->e_ident[EI_MAG2] != ELFMAG2) ||
	    (hdr->e_ident[EI_MAG3] != ELFMAG3)) {
		/* Not an ELF file - silently ignore it */
		return 0;
	}

	if (!hdr->e_ident[EI_DATA]) {
		/* Unknown endian */
		return 0;
	}

	endian = hdr->e_ident[EI_DATA];

	/* Fix endianness in ELF header */
	hdr->e_type      = TO_NATIVE(endian, 16, hdr->e_type);
	hdr->e_machine   = TO_NATIVE(endian, 16, hdr->e_machine);
	hdr->e_version   = TO_NATIVE(endian, 32, hdr->e_version);
	hdr->e_entry     = TO_NATIVE(endian, ADDR_SIZE, hdr->e_entry);
	hdr->e_phoff     = TO_NATIVE(endian, ADDR_SIZE, hdr->e_phoff);
	hdr->e_shoff     = TO_NATIVE(endian, ADDR_SIZE, hdr->e_shoff);
	hdr->e_flags     = TO_NATIVE(endian, 32, hdr->e_flags);
	hdr->e_ehsize    = TO_NATIVE(endian, 16, hdr->e_ehsize);
	hdr->e_phentsize = TO_NATIVE(endian, 16, hdr->e_phentsize);
	hdr->e_phnum     = TO_NATIVE(endian, 16, hdr->e_phnum);
	hdr->e_shentsize = TO_NATIVE(endian, 16, hdr->e_shentsize);
	hdr->e_shnum     = TO_NATIVE(endian, 16, hdr->e_shnum);
	hdr->e_shstrndx  = TO_NATIVE(endian, 16, hdr->e_shstrndx);

	sechdrs = RTE_PTR_ADD(hdr, hdr->e_shoff);
	info->sechdrs = sechdrs;

	/* Check if file offset is correct */
	if (hdr->e_shoff > info->size) {
		fprintf(stderr, "section header offset=%lu in file '%s' "
		      "is bigger than filesize=%lu\n",
		      (unsigned long)hdr->e_shoff,
		      filename, info->size);
		return 0;
	}

	if (hdr->e_shnum == SHN_UNDEF) {
		/*
		 * There are more than 64k sections,
		 * read count from .sh_size.
		 */
		info->num_sections =
			TO_NATIVE(endian, ADDR_SIZE, sechdrs[0].sh_size);
	} else {
		info->num_sections = hdr->e_shnum;
	}
	if (hdr->e_shstrndx == SHN_XINDEX)
		info->secindex_strings =
			TO_NATIVE(endian, 32, sechdrs[0].sh_link);
	else
		info->secindex_strings = hdr->e_shstrndx;

	/* Fix endianness in section headers */
	for (i = 0; i < info->num_sections; i++) {
		sechdrs[i].sh_name      =
			TO_NATIVE(endian, 32, sechdrs[i].sh_name);
		sechdrs[i].sh_type      =
			TO_NATIVE(endian, 32, sechdrs[i].sh_type);
		sechdrs[i].sh_flags     =
			TO_NATIVE(endian, 32, sechdrs[i].sh_flags);
		sechdrs[i].sh_addr      =
			TO_NATIVE(endian, ADDR_SIZE, sechdrs[i].sh_addr);
		sechdrs[i].sh_offset    =
			TO_NATIVE(endian, ADDR_SIZE, sechdrs[i].sh_offset);
		sechdrs[i].sh_size      =
			TO_NATIVE(endian, ADDR_SIZE, sechdrs[i].sh_size);
		sechdrs[i].sh_link      =
			TO_NATIVE(endian, 32, sechdrs[i].sh_link);
		sechdrs[i].sh_info      =
			TO_NATIVE(endian, 32, sechdrs[i].sh_info);
		sechdrs[i].sh_addralign =
			TO_NATIVE(endian, ADDR_SIZE, sechdrs[i].sh_addralign);
		sechdrs[i].sh_entsize   =
			TO_NATIVE(endian, ADDR_SIZE, sechdrs[i].sh_entsize);
	}
	/* Find symbol table. */
	for (i = 1; i < info->num_sections; i++) {
		int nobits = sechdrs[i].sh_type == SHT_NOBITS;

		if (!nobits && sechdrs[i].sh_offset > info->size) {
			fprintf(stderr, "%s is truncated. "
			      "sechdrs[i].sh_offset=%lu > sizeof(*hrd)=%zu\n",
			      filename, (unsigned long)sechdrs[i].sh_offset,
			      sizeof(*hdr));
			return 0;
		}

		if (sechdrs[i].sh_type == SHT_SYMTAB) {
			unsigned int sh_link_idx;
			symtab_idx = i;
			info->symtab_start = RTE_PTR_ADD(hdr,
				sechdrs[i].sh_offset);
			info->symtab_stop  = RTE_PTR_ADD(hdr,
				sechdrs[i].sh_offset + sechdrs[i].sh_size);
			sh_link_idx = sechdrs[i].sh_link;
			info->strtab       = RTE_PTR_ADD(hdr,
				sechdrs[sh_link_idx].sh_offset);
		}

		/* 32bit section no. table? ("more than 64k sections") */
		if (sechdrs[i].sh_type == SHT_SYMTAB_SHNDX) {
			symtab_shndx_idx = i;
			info->symtab_shndx_start = RTE_PTR_ADD(hdr,
				sechdrs[i].sh_offset);
			info->symtab_shndx_stop  = RTE_PTR_ADD(hdr,
				sechdrs[i].sh_offset + sechdrs[i].sh_size);
		}
	}
	if (!info->symtab_start)
		fprintf(stderr, "%s has no symtab?\n", filename);
	else {
		/* Fix endianness in symbols */
		for (sym = info->symtab_start; sym < info->symtab_stop; sym++) {
			sym->st_shndx = TO_NATIVE(endian, 16, sym->st_shndx);
			sym->st_name  = TO_NATIVE(endian, 32, sym->st_name);
			sym->st_value = TO_NATIVE(endian, ADDR_SIZE, sym->st_value);
			sym->st_size  = TO_NATIVE(endian, ADDR_SIZE, sym->st_size);
		}
	}

	if (symtab_shndx_idx != ~0U) {
		Elf32_Word *p;
		if (symtab_idx != sechdrs[symtab_shndx_idx].sh_link)
			fprintf(stderr,
			      "%s: SYMTAB_SHNDX has bad sh_link: %u!=%u\n",
			      filename, sechdrs[symtab_shndx_idx].sh_link,
			      symtab_idx);
		/* Fix endianness */
		for (p = info->symtab_shndx_start; p < info->symtab_shndx_stop;
		     p++)
			*p = TO_NATIVE(endian, 32, *p);
	}

	return 1;
}

static void parse_elf_finish(struct elf_info *info)
{
	struct pmd_driver *tmp, *idx = info->drivers;
	release_file(info->hdr, info->size);
	while (idx) {
		tmp = idx->next;
		free(idx);
		idx = tmp;
	}
}

struct opt_tag {
	const char *suffix;
	const char *json_id;
};

static const struct opt_tag opt_tags[] = {
	{"_param_string_export", "params"},
	{"_kmod_dep_export", "kmod"},
};

static int complete_pmd_entry(struct elf_info *info, struct pmd_driver *drv)
{
	const char *tname;
	int i;
	char tmpsymname[128];
	Elf_Sym *tmpsym;

	drv->name = get_sym_value(info, drv->name_sym);

	for (i = 0; i < PMD_OPT_MAX; i++) {
		memset(tmpsymname, 0, 128);
		sprintf(tmpsymname, "__%s%s", drv->name, opt_tags[i].suffix);
		tmpsym = find_sym_in_symtab(info, tmpsymname, NULL);
		if (!tmpsym)
			continue;
		drv->opt_vals[i] = get_sym_value(info, tmpsym);
	}

	memset(tmpsymname, 0, 128);
	sprintf(tmpsymname, "__%s_pci_tbl_export", drv->name);

	tmpsym = find_sym_in_symtab(info, tmpsymname, NULL);


	/*
	 * If this returns NULL, then this is a PMD_VDEV, because
	 * it has no pci table reference
	 */
	if (!tmpsym) {
		drv->pci_tbl = NULL;
		return 0;
	}

	tname = get_sym_value(info, tmpsym);
	tmpsym = find_sym_in_symtab(info, tname, NULL);
	if (!tmpsym)
		return -ENOENT;

	drv->pci_tbl = (struct rte_pci_id *)get_sym_value(info, tmpsym);
	if (!drv->pci_tbl)
		return -ENOENT;

	return 0;
}

static int locate_pmd_entries(struct elf_info *info)
{
	Elf_Sym *last = NULL;
	struct pmd_driver *new;

	info->drivers = NULL;

	do {
		new = calloc(sizeof(struct pmd_driver), 1);
		if (new == NULL) {
			fprintf(stderr, "Failed to calloc memory\n");
			return -1;
		}
		new->name_sym = find_sym_in_symtab(info, "this_pmd_name", last);
		last = new->name_sym;
		if (!new->name_sym)
			free(new);
		else {
			if (complete_pmd_entry(info, new)) {
				fprintf(stderr,
					"Failed to complete pmd entry\n");
				free(new);
			} else {
				new->next = info->drivers;
				info->drivers = new;
			}
		}
	} while (last);

	return 0;
}

static void output_pmd_info_string(struct elf_info *info, char *outfile)
{
	FILE *ofd;
	struct pmd_driver *drv;
	struct rte_pci_id *pci_ids;
	int idx = 0;

	if (use_stdout)
		ofd = stdout;
	else {
		ofd = fopen(outfile, "w+");
		if (!ofd) {
			fprintf(stderr, "Unable to open output file\n");
			return;
		}
	}

	drv = info->drivers;

	while (drv) {
		fprintf(ofd, "const char %s_pmd_info[] __attribute__((used)) = "
			"\"PMD_INFO_STRING= {",
			drv->name);
		fprintf(ofd, "\\\"name\\\" : \\\"%s\\\", ", drv->name);

		for (idx = 0; idx < PMD_OPT_MAX; idx++) {
			if (drv->opt_vals[idx])
				fprintf(ofd, "\\\"%s\\\" : \\\"%s\\\", ",
					opt_tags[idx].json_id,
					drv->opt_vals[idx]);
		}

		pci_ids = drv->pci_tbl;
		fprintf(ofd, "\\\"pci_ids\\\" : [");

		while (pci_ids && pci_ids->device_id) {
			fprintf(ofd, "[%d, %d, %d, %d]",
				pci_ids->vendor_id, pci_ids->device_id,
				pci_ids->subsystem_vendor_id,
				pci_ids->subsystem_device_id);
			pci_ids++;
			if (pci_ids->device_id)
				fprintf(ofd, ",");
			else
				fprintf(ofd, " ");
		}
		fprintf(ofd, "]}\";\n");
		drv = drv->next;
	}

	fclose(ofd);
}

int main(int argc, char **argv)
{
	struct elf_info info = {0};
	int rc = 1;

	if (argc < 3) {
		fprintf(stderr,
			"usage: %s <object file> <c output file>\n",
			basename(argv[0]));
		exit(127);
	}
	use_stdin = !strcmp(argv[1], "-");
	use_stdout = !strcmp(argv[2], "-");
	parse_elf(&info, argv[1]);

	if (locate_pmd_entries(&info) < 0)
		exit(1);

	if (info.drivers) {
		output_pmd_info_string(&info, argv[2]);
		rc = 0;
	} else {
		fprintf(stderr, "No drivers registered\n");
	}

	parse_elf_finish(&info);
	exit(rc);
}
