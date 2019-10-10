/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2010-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include <dpaa_of.h>
#include <assert.h>
#include <rte_string_fns.h>
#include <dpaax_logs.h>

static int alive;
static struct dt_dir root_dir;
static const char *base_dir;
static COMPAT_LIST_HEAD(linear);

static int
of_open_dir(const char *relative_path, struct dirent ***d)
{
	int ret;
	char full_path[PATH_MAX];

	snprintf(full_path, PATH_MAX, "%s/%s", base_dir, relative_path);
	ret = scandir(full_path, d, 0, versionsort);
	if (ret < 0)
		DPAAX_LOG(ERR, "Failed to open directory %s",
			     full_path);
	return ret;
}

static void
of_close_dir(struct dirent **d, int num)
{
	while (num--)
		free(d[num]);
	free(d);
}

static int
of_open_file(const char *relative_path)
{
	int ret;
	char full_path[PATH_MAX];

	snprintf(full_path, PATH_MAX, "%s/%s", base_dir, relative_path);
	ret = open(full_path, O_RDONLY);
	if (ret < 0)
		DPAAX_LOG(ERR, "Failed to open directory %s",
			     full_path);
	return ret;
}

static void
process_file(struct dirent *dent, struct dt_dir *parent)
{
	int fd;
	struct dt_file *f = malloc(sizeof(*f));

	if (!f) {
		DPAAX_LOG(DEBUG, "Unable to allocate memory for file node");
		return;
	}
	f->node.is_file = 1;
	strlcpy(f->node.node.name, dent->d_name, NAME_MAX);
	snprintf(f->node.node.full_name, PATH_MAX, "%s/%s",
		 parent->node.node.full_name, dent->d_name);
	f->parent = parent;
	fd = of_open_file(f->node.node.full_name);
	if (fd < 0) {
		DPAAX_LOG(DEBUG, "Unable to open file node");
		free(f);
		return;
	}
	f->len = read(fd, f->buf, OF_FILE_BUF_MAX);
	close(fd);
	if (f->len < 0) {
		DPAAX_LOG(DEBUG, "Unable to read file node");
		free(f);
		return;
	}
	list_add_tail(&f->node.list, &parent->files);
}

static const struct dt_dir *
node2dir(const struct device_node *n)
{
	struct dt_node *dn = container_of((struct device_node *)n,
					  struct dt_node, node);
	const struct dt_dir *d = container_of(dn, struct dt_dir, node);

	assert(!dn->is_file);
	return d;
}

/* process_dir() calls iterate_dir(), but the latter will also call the former
 * when recursing into sub-directories, so a predeclaration is needed.
 */
static int process_dir(const char *relative_path, struct dt_dir *dt);

static int
iterate_dir(struct dirent **d, int num, struct dt_dir *dt)
{
	int loop;
	/* Iterate the directory contents */
	for (loop = 0; loop < num; loop++) {
		struct dt_dir *subdir;
		int ret;
		/* Ignore dot files of all types (especially "..") */
		if (d[loop]->d_name[0] == '.')
			continue;
		switch (d[loop]->d_type) {
		case DT_REG:
			process_file(d[loop], dt);
			break;
		case DT_DIR:
			subdir = malloc(sizeof(*subdir));
			if (!subdir) {
				perror("malloc");
				return -ENOMEM;
			}
			strlcpy(subdir->node.node.name, d[loop]->d_name,
				NAME_MAX);
			snprintf(subdir->node.node.full_name, PATH_MAX,
				 "%s/%s", dt->node.node.full_name,
				 d[loop]->d_name);
			subdir->parent = dt;
			ret = process_dir(subdir->node.node.full_name, subdir);
			if (ret)
				return ret;
			list_add_tail(&subdir->node.list, &dt->subdirs);
			break;
		default:
			DPAAX_LOG(DEBUG, "Ignoring invalid dt entry %s/%s",
				     dt->node.node.full_name, d[loop]->d_name);
		}
	}
	return 0;
}

static int
process_dir(const char *relative_path, struct dt_dir *dt)
{
	struct dirent **d;
	int ret, num;

	dt->node.is_file = 0;
	INIT_LIST_HEAD(&dt->subdirs);
	INIT_LIST_HEAD(&dt->files);
	ret = of_open_dir(relative_path, &d);
	if (ret < 0)
		return ret;
	num = ret;
	ret = iterate_dir(d, num, dt);
	of_close_dir(d, num);
	return (ret < 0) ? ret : 0;
}

static void
linear_dir(struct dt_dir *d)
{
	struct dt_file *f;
	struct dt_dir *dd;

	d->compatible = NULL;
	d->status = NULL;
	d->lphandle = NULL;
	d->a_cells = NULL;
	d->s_cells = NULL;
	d->reg = NULL;
	list_for_each_entry(f, &d->files, node.list) {
		if (!strcmp(f->node.node.name, "compatible")) {
			if (d->compatible)
				DPAAX_LOG(DEBUG, "Duplicate compatible in"
					     " %s", d->node.node.full_name);
			d->compatible = f;
		} else if (!strcmp(f->node.node.name, "status")) {
			if (d->status)
				DPAAX_LOG(DEBUG, "Duplicate status in %s",
					     d->node.node.full_name);
			d->status = f;
		} else if (!strcmp(f->node.node.name, "linux,phandle")) {
			if (d->lphandle)
				DPAAX_LOG(DEBUG, "Duplicate lphandle in %s",
					     d->node.node.full_name);
			d->lphandle = f;
		} else if (!strcmp(f->node.node.name, "phandle")) {
			if (d->lphandle)
				DPAAX_LOG(DEBUG, "Duplicate lphandle in %s",
					     d->node.node.full_name);
			d->lphandle = f;
		} else if (!strcmp(f->node.node.name, "#address-cells")) {
			if (d->a_cells)
				DPAAX_LOG(DEBUG, "Duplicate a_cells in %s",
					     d->node.node.full_name);
			d->a_cells = f;
		} else if (!strcmp(f->node.node.name, "#size-cells")) {
			if (d->s_cells)
				DPAAX_LOG(DEBUG, "Duplicate s_cells in %s",
					     d->node.node.full_name);
			d->s_cells = f;
		} else if (!strcmp(f->node.node.name, "reg")) {
			if (d->reg)
				DPAAX_LOG(DEBUG, "Duplicate reg in %s",
					     d->node.node.full_name);
			d->reg = f;
		}
	}

	list_for_each_entry(dd, &d->subdirs, node.list) {
		list_add_tail(&dd->linear, &linear);
		linear_dir(dd);
	}
}

int
of_init_path(const char *dt_path)
{
	int ret;

	base_dir = dt_path;

	/* This needs to be singleton initialization */
	DPAAX_HWWARN(alive, "Double-init of device-tree driver!");

	/* Prepare root node (the remaining fields are set in process_dir()) */
	root_dir.node.node.name[0] = '\0';
	root_dir.node.node.full_name[0] = '\0';
	INIT_LIST_HEAD(&root_dir.node.list);
	root_dir.parent = NULL;

	/* Kick things off... */
	ret = process_dir("", &root_dir);
	if (ret) {
		DPAAX_LOG(ERR, "Unable to parse device tree");
		return ret;
	}

	/* Now make a flat, linear list of directories */
	linear_dir(&root_dir);
	alive = 1;
	return 0;
}

static void
destroy_dir(struct dt_dir *d)
{
	struct dt_file *f, *tmpf;
	struct dt_dir *dd, *tmpd;

	list_for_each_entry_safe(f, tmpf, &d->files, node.list) {
		list_del(&f->node.list);
		free(f);
	}
	list_for_each_entry_safe(dd, tmpd, &d->subdirs, node.list) {
		destroy_dir(dd);
		list_del(&dd->node.list);
		free(dd);
	}
}

void
of_finish(void)
{
	DPAAX_HWWARN(!alive, "Double-finish of device-tree driver!");

	destroy_dir(&root_dir);
	INIT_LIST_HEAD(&linear);
	alive = 0;
}

static const struct dt_dir *
next_linear(const struct dt_dir *f)
{
	if (f->linear.next == &linear)
		return NULL;
	return list_entry(f->linear.next, struct dt_dir, linear);
}

static int
check_compatible(const struct dt_file *f, const char *compatible)
{
	const char *c = (char *)f->buf;
	unsigned int len, remains = f->len;

	while (remains) {
		len = strlen(c);
		if (!strcmp(c, compatible))
			return 1;

		if (remains < len + 1)
			break;

		c += (len + 1);
		remains -= (len + 1);
	}
	return 0;
}

const struct device_node *
of_find_compatible_node(const struct device_node *from,
			const char *type __rte_unused,
			const char *compatible)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");

	if (list_empty(&linear))
		return NULL;
	if (!from)
		d = list_entry(linear.next, struct dt_dir, linear);
	else
		d = node2dir(from);
	for (d = next_linear(d); d && (!d->compatible ||
				       !check_compatible(d->compatible,
				       compatible));
			d = next_linear(d))
		;
	if (d)
		return &d->node.node;
	return NULL;
}

const void *
of_get_property(const struct device_node *from, const char *name,
		size_t *lenp)
{
	const struct dt_dir *d;
	const struct dt_file *f;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");

	d = node2dir(from);
	list_for_each_entry(f, &d->files, node.list)
		if (!strcmp(f->node.node.name, name)) {
			if (lenp)
				*lenp = f->len;
			return f->buf;
		}
	return NULL;
}

bool
of_device_is_available(const struct device_node *dev_node)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");
	d = node2dir(dev_node);
	if (!d->status)
		return true;
	if (!strcmp((char *)d->status->buf, "okay"))
		return true;
	if (!strcmp((char *)d->status->buf, "ok"))
		return true;
	return false;
}

const struct device_node *
of_find_node_by_phandle(uint64_t ph)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");
	list_for_each_entry(d, &linear, linear)
		if (d->lphandle && (d->lphandle->len == 4) &&
		    !memcmp(d->lphandle->buf, &ph, 4))
			return &d->node.node;
	return NULL;
}

const struct device_node *
of_get_parent(const struct device_node *dev_node)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");

	if (!dev_node)
		return NULL;
	d = node2dir(dev_node);
	if (!d->parent)
		return NULL;
	return &d->parent->node.node;
}

const struct device_node *
of_get_next_child(const struct device_node *dev_node,
		  const struct device_node *prev)
{
	const struct dt_dir *p, *c;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");

	if (!dev_node)
		return NULL;
	p = node2dir(dev_node);
	if (prev) {
		c = node2dir(prev);
		DPAAX_HWWARN((c->parent != p), "Parent/child mismatch");
		if (c->parent != p)
			return NULL;
		if (c->node.list.next == &p->subdirs)
			/* prev was the last child */
			return NULL;
		c = list_entry(c->node.list.next, struct dt_dir, node.list);
		return &c->node.node;
	}
	/* Return first child */
	if (list_empty(&p->subdirs))
		return NULL;
	c = list_entry(p->subdirs.next, struct dt_dir, node.list);
	return &c->node.node;
}

uint32_t
of_n_addr_cells(const struct device_node *dev_node)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised");
	if (!dev_node)
		return OF_DEFAULT_NA;
	d = node2dir(dev_node);
	while ((d = d->parent))
		if (d->a_cells) {
			unsigned char *buf =
				(unsigned char *)&d->a_cells->buf[0];
			assert(d->a_cells->len == 4);
			return ((uint32_t)buf[0] << 24) |
				((uint32_t)buf[1] << 16) |
				((uint32_t)buf[2] << 8) |
				(uint32_t)buf[3];
		}
	return OF_DEFAULT_NA;
}

uint32_t
of_n_size_cells(const struct device_node *dev_node)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");
	if (!dev_node)
		return OF_DEFAULT_NA;
	d = node2dir(dev_node);
	while ((d = d->parent))
		if (d->s_cells) {
			unsigned char *buf =
				(unsigned char *)&d->s_cells->buf[0];
			assert(d->s_cells->len == 4);
			return ((uint32_t)buf[0] << 24) |
				((uint32_t)buf[1] << 16) |
				((uint32_t)buf[2] << 8) |
				(uint32_t)buf[3];
		}
	return OF_DEFAULT_NS;
}

const uint32_t *
of_get_address(const struct device_node *dev_node, size_t idx,
	       uint64_t *size, uint32_t *flags __rte_unused)
{
	const struct dt_dir *d;
	const unsigned char *buf;
	uint32_t na = of_n_addr_cells(dev_node);
	uint32_t ns = of_n_size_cells(dev_node);

	if (!dev_node)
		d = &root_dir;
	else
		d = node2dir(dev_node);
	if (!d->reg)
		return NULL;
	assert(d->reg->len % ((na + ns) * 4) == 0);
	assert(d->reg->len / ((na + ns) * 4) > (unsigned int) idx);
	buf = (const unsigned char *)&d->reg->buf[0];
	buf += (na + ns) * idx * 4;
	if (size)
		for (*size = 0; ns > 0; ns--, na++)
			*size = (*size << 32) +
				(((uint32_t)buf[4 * na] << 24) |
				((uint32_t)buf[4 * na + 1] << 16) |
				((uint32_t)buf[4 * na + 2] << 8) |
				(uint32_t)buf[4 * na + 3]);
	return (const uint32_t *)buf;
}

uint64_t
of_translate_address(const struct device_node *dev_node,
		     const uint32_t *addr)
{
	uint64_t phys_addr, tmp_addr;
	const struct device_node *parent;
	const uint32_t *ranges;
	size_t rlen;
	uint32_t na, pna;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");
	assert(dev_node != NULL);

	na = of_n_addr_cells(dev_node);
	phys_addr = of_read_number(addr, na);

	dev_node = of_get_parent(dev_node);
	if (!dev_node)
		return 0;
	else if (node2dir(dev_node) == &root_dir)
		return phys_addr;

	do {
		pna = of_n_addr_cells(dev_node);
		parent = of_get_parent(dev_node);
		if (!parent)
			return 0;

		ranges = of_get_property(dev_node, "ranges", &rlen);
		/* "ranges" property is missing. Translation breaks */
		if (!ranges)
			return 0;
		/* "ranges" property is empty. Do 1:1 translation */
		else if (rlen == 0)
			continue;
		else
			tmp_addr = of_read_number(ranges + na, pna);

		na = pna;
		dev_node = parent;
		phys_addr += tmp_addr;
	} while (node2dir(parent) != &root_dir);

	return phys_addr;
}

bool
of_device_is_compatible(const struct device_node *dev_node,
			const char *compatible)
{
	const struct dt_dir *d;

	DPAAX_HWWARN(!alive, "Device-tree driver not initialised!");
	if (!dev_node)
		d = &root_dir;
	else
		d = node2dir(dev_node);
	if (d->compatible && check_compatible(d->compatible, compatible))
		return true;
	return false;
}

static const void *of_get_mac_addr(const struct device_node *np,
		const char *name)
{
	return of_get_property(np, name, NULL);
}

/**
 * Search the device tree for the best MAC address to use.  'mac-address' is
 * checked first, because that is supposed to contain to "most recent" MAC
 * address. If that isn't set, then 'local-mac-address' is checked next,
 * because that is the default address.  If that isn't set, then the obsolete
 * 'address' is checked, just in case we're using an old device tree.
 *
 * Note that the 'address' property is supposed to contain a virtual address of
 * the register set, but some DTS files have redefined that property to be the
 * MAC address.
 *
 * All-zero MAC addresses are rejected, because those could be properties that
 * exist in the device tree, but were not set by U-Boot.  For example, the
 * DTS could define 'mac-address' and 'local-mac-address', with zero MAC
 * addresses.  Some older U-Boots only initialized 'local-mac-address'.  In
 * this case, the real MAC is in 'local-mac-address', and 'mac-address' exists
 * but is all zeros.
 */
const void *of_get_mac_address(const struct device_node *np)
{
	const void *addr;

	addr = of_get_mac_addr(np, "mac-address");
	if (addr)
		return addr;

	addr = of_get_mac_addr(np, "local-mac-address");
	if (addr)
		return addr;

	return of_get_mac_addr(np, "address");
}
