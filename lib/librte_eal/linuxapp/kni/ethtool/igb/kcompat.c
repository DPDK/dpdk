/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2013 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include "igb.h"
#include "kcompat.h"

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,8) )
/* From lib/vsprintf.c */
#include <asm/div64.h>

static int skip_atoi(const char **s)
{
	int i=0;

	while (isdigit(**s))
		i = i*10 + *((*s)++) - '0';
	return i;
}

#define _kc_ZEROPAD	1		/* pad with zero */
#define _kc_SIGN	2		/* unsigned/signed long */
#define _kc_PLUS	4		/* show plus */
#define _kc_SPACE	8		/* space if plus */
#define _kc_LEFT	16		/* left justified */
#define _kc_SPECIAL	32		/* 0x */
#define _kc_LARGE	64		/* use 'ABCDEF' instead of 'abcdef' */

static char * number(char * buf, char * end, long long num, int base, int size, int precision, int type)
{
	char c,sign,tmp[66];
	const char *digits;
	const char small_digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	const char large_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int i;

	digits = (type & _kc_LARGE) ? large_digits : small_digits;
	if (type & _kc_LEFT)
		type &= ~_kc_ZEROPAD;
	if (base < 2 || base > 36)
		return 0;
	c = (type & _kc_ZEROPAD) ? '0' : ' ';
	sign = 0;
	if (type & _kc_SIGN) {
		if (num < 0) {
			sign = '-';
			num = -num;
			size--;
		} else if (type & _kc_PLUS) {
			sign = '+';
			size--;
		} else if (type & _kc_SPACE) {
			sign = ' ';
			size--;
		}
	}
	if (type & _kc_SPECIAL) {
		if (base == 16)
			size -= 2;
		else if (base == 8)
			size--;
	}
	i = 0;
	if (num == 0)
		tmp[i++]='0';
	else while (num != 0)
		tmp[i++] = digits[do_div(num,base)];
	if (i > precision)
		precision = i;
	size -= precision;
	if (!(type&(_kc_ZEROPAD+_kc_LEFT))) {
		while(size-->0) {
			if (buf <= end)
				*buf = ' ';
			++buf;
		}
	}
	if (sign) {
		if (buf <= end)
			*buf = sign;
		++buf;
	}
	if (type & _kc_SPECIAL) {
		if (base==8) {
			if (buf <= end)
				*buf = '0';
			++buf;
		} else if (base==16) {
			if (buf <= end)
				*buf = '0';
			++buf;
			if (buf <= end)
				*buf = digits[33];
			++buf;
		}
	}
	if (!(type & _kc_LEFT)) {
		while (size-- > 0) {
			if (buf <= end)
				*buf = c;
			++buf;
		}
	}
	while (i < precision--) {
		if (buf <= end)
			*buf = '0';
		++buf;
	}
	while (i-- > 0) {
		if (buf <= end)
			*buf = tmp[i];
		++buf;
	}
	while (size-- > 0) {
		if (buf <= end)
			*buf = ' ';
		++buf;
	}
	return buf;
}

int _kc_vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
	int len;
	unsigned long long num;
	int i, base;
	char *str, *end, c;
	const char *s;

	int flags;		/* flags to number() */

	int field_width;	/* width of output field */
	int precision;		/* min. # of digits for integers; max
				   number of chars for from string */
	int qualifier;		/* 'h', 'l', or 'L' for integer fields */
				/* 'z' support added 23/7/1999 S.H.    */
				/* 'z' changed to 'Z' --davidm 1/25/99 */

	str = buf;
	end = buf + size - 1;

	if (end < buf - 1) {
		end = ((void *) -1);
		size = end - buf + 1;
	}

	for (; *fmt ; ++fmt) {
		if (*fmt != '%') {
			if (str <= end)
				*str = *fmt;
			++str;
			continue;
		}

		/* process flags */
		flags = 0;
		repeat:
			++fmt;		/* this also skips first '%' */
			switch (*fmt) {
				case '-': flags |= _kc_LEFT; goto repeat;
				case '+': flags |= _kc_PLUS; goto repeat;
				case ' ': flags |= _kc_SPACE; goto repeat;
				case '#': flags |= _kc_SPECIAL; goto repeat;
				case '0': flags |= _kc_ZEROPAD; goto repeat;
			}

		/* get field width */
		field_width = -1;
		if (isdigit(*fmt))
			field_width = skip_atoi(&fmt);
		else if (*fmt == '*') {
			++fmt;
			/* it's the next argument */
			field_width = va_arg(args, int);
			if (field_width < 0) {
				field_width = -field_width;
				flags |= _kc_LEFT;
			}
		}

		/* get the precision */
		precision = -1;
		if (*fmt == '.') {
			++fmt;
			if (isdigit(*fmt))
				precision = skip_atoi(&fmt);
			else if (*fmt == '*') {
				++fmt;
				/* it's the next argument */
				precision = va_arg(args, int);
			}
			if (precision < 0)
				precision = 0;
		}

		/* get the conversion qualifier */
		qualifier = -1;
		if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt =='Z') {
			qualifier = *fmt;
			++fmt;
		}

		/* default base */
		base = 10;

		switch (*fmt) {
			case 'c':
				if (!(flags & _kc_LEFT)) {
					while (--field_width > 0) {
						if (str <= end)
							*str = ' ';
						++str;
					}
				}
				c = (unsigned char) va_arg(args, int);
				if (str <= end)
					*str = c;
				++str;
				while (--field_width > 0) {
					if (str <= end)
						*str = ' ';
					++str;
				}
				continue;

			case 's':
				s = va_arg(args, char *);
				if (!s)
					s = "<NULL>";

				len = strnlen(s, precision);

				if (!(flags & _kc_LEFT)) {
					while (len < field_width--) {
						if (str <= end)
							*str = ' ';
						++str;
					}
				}
				for (i = 0; i < len; ++i) {
					if (str <= end)
						*str = *s;
					++str; ++s;
				}
				while (len < field_width--) {
					if (str <= end)
						*str = ' ';
					++str;
				}
				continue;

			case 'p':
				if (field_width == -1) {
					field_width = 2*sizeof(void *);
					flags |= _kc_ZEROPAD;
				}
				str = number(str, end,
						(unsigned long) va_arg(args, void *),
						16, field_width, precision, flags);
				continue;


			case 'n':
				/* FIXME:
				* What does C99 say about the overflow case here? */
				if (qualifier == 'l') {
					long * ip = va_arg(args, long *);
					*ip = (str - buf);
				} else if (qualifier == 'Z') {
					size_t * ip = va_arg(args, size_t *);
					*ip = (str - buf);
				} else {
					int * ip = va_arg(args, int *);
					*ip = (str - buf);
				}
				continue;

			case '%':
				if (str <= end)
					*str = '%';
				++str;
				continue;

				/* integer number formats - set up the flags and "break" */
			case 'o':
				base = 8;
				break;

			case 'X':
				flags |= _kc_LARGE;
			case 'x':
				base = 16;
				break;

			case 'd':
			case 'i':
				flags |= _kc_SIGN;
			case 'u':
				break;

			default:
				if (str <= end)
					*str = '%';
				++str;
				if (*fmt) {
					if (str <= end)
						*str = *fmt;
					++str;
				} else {
					--fmt;
				}
				continue;
		}
		if (qualifier == 'L')
			num = va_arg(args, long long);
		else if (qualifier == 'l') {
			num = va_arg(args, unsigned long);
			if (flags & _kc_SIGN)
				num = (signed long) num;
		} else if (qualifier == 'Z') {
			num = va_arg(args, size_t);
		} else if (qualifier == 'h') {
			num = (unsigned short) va_arg(args, int);
			if (flags & _kc_SIGN)
				num = (signed short) num;
		} else {
			num = va_arg(args, unsigned int);
			if (flags & _kc_SIGN)
				num = (signed int) num;
		}
		str = number(str, end, num, base,
				field_width, precision, flags);
	}
	if (str <= end)
		*str = '\0';
	else if (size > 0)
		/* don't write out a null byte if the buf size is zero */
		*end = '\0';
	/* the trailing null byte doesn't count towards the total
	* ++str;
	*/
	return str-buf;
}

int _kc_snprintf(char * buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = _kc_vsnprintf(buf,size,fmt,args);
	va_end(args);
	return i;
}
#endif /* < 2.4.8 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,13) )

/**************************************/
/* PCI DMA MAPPING */

#if defined(CONFIG_HIGHMEM)

#ifndef PCI_DRAM_OFFSET
#define PCI_DRAM_OFFSET 0
#endif

u64
_kc_pci_map_page(struct pci_dev *dev, struct page *page, unsigned long offset,
                 size_t size, int direction)
{
	return (((u64) (page - mem_map) << PAGE_SHIFT) + offset +
		PCI_DRAM_OFFSET);
}

#else /* CONFIG_HIGHMEM */

u64
_kc_pci_map_page(struct pci_dev *dev, struct page *page, unsigned long offset,
                 size_t size, int direction)
{
	return pci_map_single(dev, (void *)page_address(page) + offset, size,
			      direction);
}

#endif /* CONFIG_HIGHMEM */

void
_kc_pci_unmap_page(struct pci_dev *dev, u64 dma_addr, size_t size,
                   int direction)
{
	return pci_unmap_single(dev, dma_addr, size, direction);
}

#endif /* 2.4.13 => 2.4.3 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,3) )

/**************************************/
/* PCI DRIVER API */

int
_kc_pci_set_dma_mask(struct pci_dev *dev, dma_addr_t mask)
{
	if (!pci_dma_supported(dev, mask))
		return -EIO;
	dev->dma_mask = mask;
	return 0;
}

int
_kc_pci_request_regions(struct pci_dev *dev, char *res_name)
{
	int i;

	for (i = 0; i < 6; i++) {
		if (pci_resource_len(dev, i) == 0)
			continue;

		if (pci_resource_flags(dev, i) & IORESOURCE_IO) {
			if (!request_region(pci_resource_start(dev, i), pci_resource_len(dev, i), res_name)) {
				pci_release_regions(dev);
				return -EBUSY;
			}
		} else if (pci_resource_flags(dev, i) & IORESOURCE_MEM) {
			if (!request_mem_region(pci_resource_start(dev, i), pci_resource_len(dev, i), res_name)) {
				pci_release_regions(dev);
				return -EBUSY;
			}
		}
	}
	return 0;
}

void
_kc_pci_release_regions(struct pci_dev *dev)
{
	int i;

	for (i = 0; i < 6; i++) {
		if (pci_resource_len(dev, i) == 0)
			continue;

		if (pci_resource_flags(dev, i) & IORESOURCE_IO)
			release_region(pci_resource_start(dev, i), pci_resource_len(dev, i));

		else if (pci_resource_flags(dev, i) & IORESOURCE_MEM)
			release_mem_region(pci_resource_start(dev, i), pci_resource_len(dev, i));
	}
}

/**************************************/
/* NETWORK DRIVER API */

struct net_device *
_kc_alloc_etherdev(int sizeof_priv)
{
	struct net_device *dev;
	int alloc_size;

	alloc_size = sizeof(*dev) + sizeof_priv + IFNAMSIZ + 31;
	dev = kzalloc(alloc_size, GFP_KERNEL);
	if (!dev)
		return NULL;

	if (sizeof_priv)
		dev->priv = (void *) (((unsigned long)(dev + 1) + 31) & ~31);
	dev->name[0] = '\0';
	ether_setup(dev);

	return dev;
}

int
_kc_is_valid_ether_addr(u8 *addr)
{
	const char zaddr[6] = { 0, };

	return !(addr[0] & 1) && memcmp(addr, zaddr, 6);
}

#endif /* 2.4.3 => 2.4.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,4,6) )

int
_kc_pci_set_power_state(struct pci_dev *dev, int state)
{
	return 0;
}

int
_kc_pci_enable_wake(struct pci_dev *pdev, u32 state, int enable)
{
	return 0;
}

#endif /* 2.4.6 => 2.4.3 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0) )
void _kc_skb_fill_page_desc(struct sk_buff *skb, int i, struct page *page,
                            int off, int size)
{
	skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
	frag->page = page;
	frag->page_offset = off;
	frag->size = size;
	skb_shinfo(skb)->nr_frags = i + 1;
}

/*
 * Original Copyright:
 * find_next_bit.c: fallback find next bit implementation
 *
 * Copyright (C) 2004 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 */

/**
 * find_next_bit - find the next set bit in a memory region
 * @addr: The address to base the search on
 * @offset: The bitnumber to start searching at
 * @size: The maximum size to search
 */
unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
                            unsigned long offset)
{
	const unsigned long *p = addr + BITOP_WORD(offset);
	unsigned long result = offset & ~(BITS_PER_LONG-1);
	unsigned long tmp;

	if (offset >= size)
		return size;
	size -= result;
	offset %= BITS_PER_LONG;
	if (offset) {
		tmp = *(p++);
		tmp &= (~0UL << offset);
		if (size < BITS_PER_LONG)
			goto found_first;
		if (tmp)
			goto found_middle;
		size -= BITS_PER_LONG;
		result += BITS_PER_LONG;
	}
	while (size & ~(BITS_PER_LONG-1)) {
		if ((tmp = *(p++)))
			goto found_middle;
		result += BITS_PER_LONG;
		size -= BITS_PER_LONG;
	}
	if (!size)
		return result;
	tmp = *p;

found_first:
	tmp &= (~0UL >> (BITS_PER_LONG - size));
	if (tmp == 0UL)		/* Are any bits set? */
		return result + size;	/* Nope. */
found_middle:
	return result + ffs(tmp);
}

size_t _kc_strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}

#ifndef do_div
#if BITS_PER_LONG == 32
uint32_t __attribute__((weak)) _kc__div64_32(uint64_t *n, uint32_t base)
{
	uint64_t rem = *n;
	uint64_t b = base;
	uint64_t res, d = 1;
	uint32_t high = rem >> 32;

	/* Reduce the thing a bit first */
	res = 0;
	if (high >= base) {
		high /= base;
		res = (uint64_t) high << 32;
		rem -= (uint64_t) (high*base) << 32;
	}

	while ((int64_t)b > 0 && b < rem) {
		b = b+b;
		d = d+d;
	}

	do {
		if (rem >= b) {
			rem -= b;
			res += d;
		}
		b >>= 1;
		d >>= 1;
	} while (d);

	*n = res;
	return rem;
}
#endif /* BITS_PER_LONG == 32 */
#endif /* do_div */
#endif /* 2.6.0 => 2.4.6 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,4) )
int _kc_scnprintf(char * buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(buf, size, fmt, args);
	va_end(args);
	return (i >= size) ? (size - 1) : i;
}
#endif /* < 2.6.4 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) )
DECLARE_BITMAP(_kcompat_node_online_map, MAX_NUMNODES) = {1};
#endif /* < 2.6.10 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,13) )
char *_kc_kstrdup(const char *s, unsigned int gfp)
{
	size_t len;
	char *buf;

	if (!s)
		return NULL;

	len = strlen(s) + 1;
	buf = kmalloc(len, gfp);
	if (buf)
		memcpy(buf, s, len);
	return buf;
}
#endif /* < 2.6.13 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14) )
void *_kc_kzalloc(size_t size, int flags)
{
	void *ret = kmalloc(size, flags);
	if (ret)
		memset(ret, 0, size);
	return ret;
}
#endif /* <= 2.6.13 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) )
int _kc_skb_pad(struct sk_buff *skb, int pad)
{
	int ntail;

        /* If the skbuff is non linear tailroom is always zero.. */
        if(!skb_cloned(skb) && skb_tailroom(skb) >= pad) {
		memset(skb->data+skb->len, 0, pad);
		return 0;
        }

	ntail = skb->data_len + pad - (skb->end - skb->tail);
	if (likely(skb_cloned(skb) || ntail > 0)) {
		if (pskb_expand_head(skb, 0, ntail, GFP_ATOMIC));
			goto free_skb;
	}

#ifdef MAX_SKB_FRAGS
	if (skb_is_nonlinear(skb) &&
	    !__pskb_pull_tail(skb, skb->data_len))
		goto free_skb;

#endif
	memset(skb->data + skb->len, 0, pad);
        return 0;

free_skb:
	kfree_skb(skb);
	return -ENOMEM;
}

#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(5,4)))
int _kc_pci_save_state(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct adapter_struct *adapter = netdev_priv(netdev);
	int size = PCI_CONFIG_SPACE_LEN, i;
	u16 pcie_cap_offset, pcie_link_status;

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) )
	/* no ->dev for 2.4 kernels */
	WARN_ON(pdev->dev.driver_data == NULL);
#endif
	pcie_cap_offset = pci_find_capability(pdev, PCI_CAP_ID_EXP);
	if (pcie_cap_offset) {
		if (!pci_read_config_word(pdev,
		                          pcie_cap_offset + PCIE_LINK_STATUS,
		                          &pcie_link_status))
		size = PCIE_CONFIG_SPACE_LEN;
	}
	pci_config_space_ich8lan();
#ifdef HAVE_PCI_ERS
	if (adapter->config_space == NULL)
#else
	WARN_ON(adapter->config_space != NULL);
#endif
		adapter->config_space = kmalloc(size, GFP_KERNEL);
	if (!adapter->config_space) {
		printk(KERN_ERR "Out of memory in pci_save_state\n");
		return -ENOMEM;
	}
	for (i = 0; i < (size / 4); i++)
		pci_read_config_dword(pdev, i * 4, &adapter->config_space[i]);
	return 0;
}

void _kc_pci_restore_state(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct adapter_struct *adapter = netdev_priv(netdev);
	int size = PCI_CONFIG_SPACE_LEN, i;
	u16 pcie_cap_offset;
	u16 pcie_link_status;

	if (adapter->config_space != NULL) {
		pcie_cap_offset = pci_find_capability(pdev, PCI_CAP_ID_EXP);
		if (pcie_cap_offset &&
		    !pci_read_config_word(pdev,
		                          pcie_cap_offset + PCIE_LINK_STATUS,
		                          &pcie_link_status))
			size = PCIE_CONFIG_SPACE_LEN;

		pci_config_space_ich8lan();
		for (i = 0; i < (size / 4); i++)
		pci_write_config_dword(pdev, i * 4, adapter->config_space[i]);
#ifndef HAVE_PCI_ERS
		kfree(adapter->config_space);
		adapter->config_space = NULL;
#endif
	}
}
#endif /* !(RHEL_RELEASE_CODE >= RHEL 5.4) */

#ifdef HAVE_PCI_ERS
void _kc_free_netdev(struct net_device *netdev)
{
	struct adapter_struct *adapter = netdev_priv(netdev);

	if (adapter->config_space != NULL)
		kfree(adapter->config_space);
#ifdef CONFIG_SYSFS
	if (netdev->reg_state == NETREG_UNINITIALIZED) {
		kfree((char *)netdev - netdev->padded);
	} else {
		BUG_ON(netdev->reg_state != NETREG_UNREGISTERED);
		netdev->reg_state = NETREG_RELEASED;
		class_device_put(&netdev->class_dev);
	}
#else
	kfree((char *)netdev - netdev->padded);
#endif
}
#endif

void *_kc_kmemdup(const void *src, size_t len, unsigned gfp)
{
	void *p;

	p = kzalloc(len, gfp);
	if (p)
		memcpy(p, src, len);
	return p;
}
#endif /* <= 2.6.19 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21) )
struct pci_dev *_kc_netdev_to_pdev(struct net_device *netdev)
{
	return ((struct adapter_struct *)netdev_priv(netdev))->pdev;
}
#endif /* < 2.6.21 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) )
/* hexdump code taken from lib/hexdump.c */
static void _kc_hex_dump_to_buffer(const void *buf, size_t len, int rowsize,
			int groupsize, unsigned char *linebuf,
			size_t linebuflen, bool ascii)
{
	const u8 *ptr = buf;
	u8 ch;
	int j, lx = 0;
	int ascii_column;

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	if (!len)
		goto nil;
	if (len > rowsize)		/* limit to one line at a time */
		len = rowsize;
	if ((len % groupsize) != 0)	/* no mixed size output */
		groupsize = 1;

	switch (groupsize) {
	case 8: {
		const u64 *ptr8 = buf;
		int ngroups = len / groupsize;

		for (j = 0; j < ngroups; j++)
			lx += scnprintf((char *)(linebuf + lx), linebuflen - lx,
				"%s%16.16llx", j ? " " : "",
				(unsigned long long)*(ptr8 + j));
		ascii_column = 17 * ngroups + 2;
		break;
	}

	case 4: {
		const u32 *ptr4 = buf;
		int ngroups = len / groupsize;

		for (j = 0; j < ngroups; j++)
			lx += scnprintf((char *)(linebuf + lx), linebuflen - lx,
				"%s%8.8x", j ? " " : "", *(ptr4 + j));
		ascii_column = 9 * ngroups + 2;
		break;
	}

	case 2: {
		const u16 *ptr2 = buf;
		int ngroups = len / groupsize;

		for (j = 0; j < ngroups; j++)
			lx += scnprintf((char *)(linebuf + lx), linebuflen - lx,
				"%s%4.4x", j ? " " : "", *(ptr2 + j));
		ascii_column = 5 * ngroups + 2;
		break;
	}

	default:
		for (j = 0; (j < len) && (lx + 3) <= linebuflen; j++) {
			ch = ptr[j];
			linebuf[lx++] = hex_asc(ch >> 4);
			linebuf[lx++] = hex_asc(ch & 0x0f);
			linebuf[lx++] = ' ';
		}
		if (j)
			lx--;

		ascii_column = 3 * rowsize + 2;
		break;
	}
	if (!ascii)
		goto nil;

	while (lx < (linebuflen - 1) && lx < (ascii_column - 1))
		linebuf[lx++] = ' ';
	for (j = 0; (j < len) && (lx + 2) < linebuflen; j++)
		linebuf[lx++] = (isascii(ptr[j]) && isprint(ptr[j])) ? ptr[j]
				: '.';
nil:
	linebuf[lx++] = '\0';
}

void _kc_print_hex_dump(const char *level,
			const char *prefix_str, int prefix_type,
			int rowsize, int groupsize,
			const void *buf, size_t len, bool ascii)
{
	const u8 *ptr = buf;
	int i, linelen, remaining = len;
	unsigned char linebuf[200];

	if (rowsize != 16 && rowsize != 32)
		rowsize = 16;

	for (i = 0; i < len; i += rowsize) {
		linelen = min(remaining, rowsize);
		remaining -= rowsize;
		_kc_hex_dump_to_buffer(ptr + i, linelen, rowsize, groupsize,
				linebuf, sizeof(linebuf), ascii);

		switch (prefix_type) {
		case DUMP_PREFIX_ADDRESS:
			printk("%s%s%*p: %s\n", level, prefix_str,
				(int)(2 * sizeof(void *)), ptr + i, linebuf);
			break;
		case DUMP_PREFIX_OFFSET:
			printk("%s%s%.8x: %s\n", level, prefix_str, i, linebuf);
			break;
		default:
			printk("%s%s%s\n", level, prefix_str, linebuf);
			break;
		}
	}
}

#ifdef HAVE_I2C_SUPPORT
struct i2c_client *
_kc_i2c_new_device(struct i2c_adapter *adap, struct i2c_board_info const *info)
{
	struct i2c_client	*client;
	int			status;

	client = kzalloc(sizeof *client, GFP_KERNEL);
	if (!client)
		return NULL;

	client->adapter = adap;

	client->dev.platform_data = info->platform_data;

	client->flags = info->flags;
	client->addr = info->addr;

	strlcpy(client->name, info->type, sizeof(client->name));

	/* Check for address business */
	status = i2c_check_addr(adap, client->addr);
	if (status)
		goto out_err;

	client->dev.parent = &client->adapter->dev;
	client->dev.bus = &i2c_bus_type;

	status = i2c_attach_client(client);
	if (status)
		goto out_err;

	dev_dbg(&adap->dev, "client [%s] registered with bus id %s\n",
		client->name, dev_name(&client->dev));

	return client;

out_err:
	dev_err(&adap->dev, "Failed to register i2c client %s at 0x%02x "
		"(%d)\n", client->name, client->addr, status);
	kfree(client);
	return NULL;
}
#endif /* HAVE_I2C_SUPPORT */
#endif /* < 2.6.22 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24) )
#ifdef NAPI
struct net_device *napi_to_poll_dev(const struct napi_struct *napi)
{
	struct adapter_q_vector *q_vector = container_of(napi,
	                                                struct adapter_q_vector,
	                                                napi);
	return &q_vector->poll_dev;
}

int __kc_adapter_clean(struct net_device *netdev, int *budget)
{
	int work_done;
	int work_to_do = min(*budget, netdev->quota);
	/* kcompat.h netif_napi_add puts napi struct in "fake netdev->priv" */
	struct napi_struct *napi = netdev->priv;
	work_done = napi->poll(napi, work_to_do);
	*budget -= work_done;
	netdev->quota -= work_done;
	return (work_done >= work_to_do) ? 1 : 0;
}
#endif /* NAPI */
#endif /* <= 2.6.24 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26) )
void _kc_pci_disable_link_state(struct pci_dev *pdev, int state)
{
	struct pci_dev *parent = pdev->bus->self;
	u16 link_state;
	int pos;

	if (!parent)
		return;

	pos = pci_find_capability(parent, PCI_CAP_ID_EXP);
	if (pos) {
		pci_read_config_word(parent, pos + PCI_EXP_LNKCTL, &link_state);
		link_state &= ~state;
		pci_write_config_word(parent, pos + PCI_EXP_LNKCTL, link_state);
	}
}
#endif /* < 2.6.26 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) )
#ifdef HAVE_TX_MQ
void _kc_netif_tx_stop_all_queues(struct net_device *netdev)
{
	struct adapter_struct *adapter = netdev_priv(netdev);
	int i;

	netif_stop_queue(netdev);
	if (netif_is_multiqueue(netdev))
		for (i = 0; i < adapter->num_tx_queues; i++)
			netif_stop_subqueue(netdev, i);
}
void _kc_netif_tx_wake_all_queues(struct net_device *netdev)
{
	struct adapter_struct *adapter = netdev_priv(netdev);
	int i;

	netif_wake_queue(netdev);
	if (netif_is_multiqueue(netdev))
		for (i = 0; i < adapter->num_tx_queues; i++)
			netif_wake_subqueue(netdev, i);
}
void _kc_netif_tx_start_all_queues(struct net_device *netdev)
{
	struct adapter_struct *adapter = netdev_priv(netdev);
	int i;

	netif_start_queue(netdev);
	if (netif_is_multiqueue(netdev))
		for (i = 0; i < adapter->num_tx_queues; i++)
			netif_start_subqueue(netdev, i);
}
#endif /* HAVE_TX_MQ */

#ifndef __WARN_printf
void __kc_warn_slowpath(const char *file, int line, const char *fmt, ...)
{
	va_list args;

	printk(KERN_WARNING "------------[ cut here ]------------\n");
	printk(KERN_WARNING "WARNING: at %s:%d %s()\n", file, line);
	va_start(args, fmt);
	vprintk(fmt, args);
	va_end(args);

	dump_stack();
}
#endif /* __WARN_printf */
#endif /* < 2.6.27 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,28) )

int
_kc_pci_prepare_to_sleep(struct pci_dev *dev)
{
	pci_power_t target_state;
	int error;

	target_state = pci_choose_state(dev, PMSG_SUSPEND);

	pci_enable_wake(dev, target_state, true);

	error = pci_set_power_state(dev, target_state);

	if (error)
		pci_enable_wake(dev, target_state, false);

	return error;
}

int
_kc_pci_wake_from_d3(struct pci_dev *dev, bool enable)
{
	int err;

	err = pci_enable_wake(dev, PCI_D3cold, enable);
	if (err)
		goto out;

	err = pci_enable_wake(dev, PCI_D3hot, enable);

out:
	return err;
}
#endif /* < 2.6.28 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29) )
static void __kc_pci_set_master(struct pci_dev *pdev, bool enable)
{
	u16 old_cmd, cmd;

	pci_read_config_word(pdev, PCI_COMMAND, &old_cmd);
	if (enable)
		cmd = old_cmd | PCI_COMMAND_MASTER;
	else
		cmd = old_cmd & ~PCI_COMMAND_MASTER;
	if (cmd != old_cmd) {
		dev_dbg(pci_dev_to_dev(pdev), "%s bus mastering\n",
			enable ? "enabling" : "disabling");
		pci_write_config_word(pdev, PCI_COMMAND, cmd);
	}
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7) )
	pdev->is_busmaster = enable;
#endif
}

void _kc_pci_clear_master(struct pci_dev *dev)
{
	__kc_pci_set_master(dev, false);
}
#endif /* < 2.6.29 */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,34) )
#if (RHEL_RELEASE_CODE < RHEL_RELEASE_VERSION(6,0))
int _kc_pci_num_vf(struct pci_dev *dev)
{
	int num_vf = 0;
#ifdef CONFIG_PCI_IOV
	struct pci_dev *vfdev;

	/* loop through all ethernet devices starting at PF dev */
	vfdev = pci_get_class(PCI_CLASS_NETWORK_ETHERNET << 8, NULL);
	while (vfdev) {
		if (vfdev->is_virtfn && vfdev->physfn == dev)
			num_vf++;

		vfdev = pci_get_class(PCI_CLASS_NETWORK_ETHERNET << 8, vfdev);
	}

#endif
	return num_vf;
}
#endif /* RHEL_RELEASE_CODE */
#endif /* < 2.6.34 */

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,35) )
#ifdef HAVE_TX_MQ
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,0)))
#ifndef CONFIG_NETDEVICES_MULTIQUEUE
void _kc_netif_set_real_num_tx_queues(struct net_device *dev, unsigned int txq)
{
	unsigned int real_num = dev->real_num_tx_queues;
	struct Qdisc *qdisc;
	int i;

	if (unlikely(txq > dev->num_tx_queues))
		;
	else if (txq > real_num)
		dev->real_num_tx_queues = txq;
	else if ( txq < real_num) {
		dev->real_num_tx_queues = txq;
		for (i = txq; i < dev->num_tx_queues; i++) {
			qdisc = netdev_get_tx_queue(dev, i)->qdisc;
			if (qdisc) {
				spin_lock_bh(qdisc_lock(qdisc));
				qdisc_reset(qdisc);
				spin_unlock_bh(qdisc_lock(qdisc));
			}
		}
	}
}
#endif /* CONFIG_NETDEVICES_MULTIQUEUE */
#endif /* !(RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,0)) */
#endif /* HAVE_TX_MQ */

ssize_t _kc_simple_write_to_buffer(void *to, size_t available, loff_t *ppos,
				   const void __user *from, size_t count)
{
        loff_t pos = *ppos;
        size_t res;

        if (pos < 0)
                return -EINVAL;
        if (pos >= available || !count)
                return 0;
        if (count > available - pos)
                count = available - pos;
        res = copy_from_user(to + pos, from, count);
        if (res == count)
                return -EFAULT;
        count -= res;
        *ppos = pos + count;
        return count;
}

#endif /* < 2.6.35 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,36) )
static const u32 _kc_flags_dup_features =
	(ETH_FLAG_LRO | ETH_FLAG_NTUPLE | ETH_FLAG_RXHASH);

u32 _kc_ethtool_op_get_flags(struct net_device *dev)
{
	return dev->features & _kc_flags_dup_features;
}

int _kc_ethtool_op_set_flags(struct net_device *dev, u32 data, u32 supported)
{
	if (data & ~supported)
		return -EINVAL;

	dev->features = ((dev->features & ~_kc_flags_dup_features) |
			 (data & _kc_flags_dup_features));
	return 0;
}
#endif /* < 2.6.36 */

/******************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(2,6,39) )
#if (!(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(6,0)))



#endif /* !(RHEL_RELEASE_CODE > RHEL_RELEASE_VERSION(6,0)) */
#endif /* < 2.6.39 */

/******************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0) )
void _kc_skb_add_rx_frag(struct sk_buff *skb, int i, struct page *page,
			 int off, int size, unsigned int truesize)
{
	skb_fill_page_desc(skb, i, page, off, size);
	skb->len += size;
	skb->data_len += size;
	skb->truesize += truesize;
}

int _kc_simple_open(struct inode *inode, struct file *file)
{
        if (inode->i_private)
                file->private_data = inode->i_private;

        return 0;
}

#endif /* < 3.4.0 */

/******************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0) )
#if !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0)) && \
    !(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,5))
static inline int __kc_pcie_cap_version(struct pci_dev *dev)
{
	int pos;
	u16 reg16;

	pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (!pos)
		return 0;
	pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &reg16);
	return reg16 & PCI_EXP_FLAGS_VERS;
}

static inline bool __kc_pcie_cap_has_devctl(const struct pci_dev __always_unused *dev)
{
	return true;
}

static inline bool __kc_pcie_cap_has_lnkctl(struct pci_dev *dev)
{
	int type = pci_pcie_type(dev);

	return __kc_pcie_cap_version(dev) > 1 ||
	       type == PCI_EXP_TYPE_ROOT_PORT ||
	       type == PCI_EXP_TYPE_ENDPOINT ||
	       type == PCI_EXP_TYPE_LEG_END;
}

static inline bool __kc_pcie_cap_has_sltctl(struct pci_dev *dev)
{
	int type = pci_pcie_type(dev);
	int pos;
	u16 pcie_flags_reg;

	pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
	if (!pos)
		return 0;
	pci_read_config_word(dev, pos + PCI_EXP_FLAGS, &pcie_flags_reg);

	return __kc_pcie_cap_version(dev) > 1 ||
	       type == PCI_EXP_TYPE_ROOT_PORT ||
	       (type == PCI_EXP_TYPE_DOWNSTREAM &&
		pcie_flags_reg & PCI_EXP_FLAGS_SLOT);
}

static inline bool __kc_pcie_cap_has_rtctl(struct pci_dev *dev)
{
	int type = pci_pcie_type(dev);

	return __kc_pcie_cap_version(dev) > 1 ||
	       type == PCI_EXP_TYPE_ROOT_PORT ||
	       type == PCI_EXP_TYPE_RC_EC;
}

static bool __kc_pcie_capability_reg_implemented(struct pci_dev *dev, int pos)
{
	if (!pci_is_pcie(dev))
		return false;

	switch (pos) {
	case PCI_EXP_FLAGS_TYPE:
		return true;
	case PCI_EXP_DEVCAP:
	case PCI_EXP_DEVCTL:
	case PCI_EXP_DEVSTA:
		return __kc_pcie_cap_has_devctl(dev);
	case PCI_EXP_LNKCAP:
	case PCI_EXP_LNKCTL:
	case PCI_EXP_LNKSTA:
		return __kc_pcie_cap_has_lnkctl(dev);
	case PCI_EXP_SLTCAP:
	case PCI_EXP_SLTCTL:
	case PCI_EXP_SLTSTA:
		return __kc_pcie_cap_has_sltctl(dev);
	case PCI_EXP_RTCTL:
	case PCI_EXP_RTCAP:
	case PCI_EXP_RTSTA:
		return __kc_pcie_cap_has_rtctl(dev);
	case PCI_EXP_DEVCAP2:
	case PCI_EXP_DEVCTL2:
	case PCI_EXP_LNKCAP2:
	case PCI_EXP_LNKCTL2:
	case PCI_EXP_LNKSTA2:
		return __kc_pcie_cap_version(dev) > 1;
	default:
		return false;
	}
}

/*
 * Note that these accessor functions are only for the "PCI Express
 * Capability" (see PCIe spec r3.0, sec 7.8).  They do not apply to the
 * other "PCI Express Extended Capabilities" (AER, VC, ACS, MFVC, etc.)
 */
int __kc_pcie_capability_read_word(struct pci_dev *dev, int pos, u16 *val)
{
	int ret;

	*val = 0;
	if (pos & 1)
		return -EINVAL;

	if (__kc_pcie_capability_reg_implemented(dev, pos)) {
		ret = pci_read_config_word(dev, pci_pcie_cap(dev) + pos, val);
		/*
		 * Reset *val to 0 if pci_read_config_word() fails, it may
		 * have been written as 0xFFFF if hardware error happens
		 * during pci_read_config_word().
		 */
		if (ret)
			*val = 0;
		return ret;
	}

	/*
	 * For Functions that do not implement the Slot Capabilities,
	 * Slot Status, and Slot Control registers, these spaces must
	 * be hardwired to 0b, with the exception of the Presence Detect
	 * State bit in the Slot Status register of Downstream Ports,
	 * which must be hardwired to 1b.  (PCIe Base Spec 3.0, sec 7.8)
	 */
	if (pci_is_pcie(dev) && pos == PCI_EXP_SLTSTA &&
	    pci_pcie_type(dev) == PCI_EXP_TYPE_DOWNSTREAM) {
		*val = PCI_EXP_SLTSTA_PDS;
	}

	return 0;
}

int __kc_pcie_capability_write_word(struct pci_dev *dev, int pos, u16 val)
{
	if (pos & 1)
		return -EINVAL;

	if (!__kc_pcie_capability_reg_implemented(dev, pos))
		return 0;

	return pci_write_config_word(dev, pci_pcie_cap(dev) + pos, val);
}

int __kc_pcie_capability_clear_and_set_word(struct pci_dev *dev, int pos,
					    u16 clear, u16 set)
{
	int ret;
	u16 val;

	ret = __kc_pcie_capability_read_word(dev, pos, &val);
	if (!ret) {
		val &= ~clear;
		val |= set;
		ret = __kc_pcie_capability_write_word(dev, pos, val);
	}

	return ret;
}
#endif /* !(SLE_VERSION_CODE && SLE_VERSION_CODE >= SLE_VERSION(11,3,0)) && \
          !(RHEL_RELEASE_CODE && RHEL_RELEASE_CODE >= RHEL_RELEASE_VERSION(6,5)) */
#endif /* < 3.7.0 */

/******************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,9,0) )
#endif /* 3.9.0 */

/*****************************************************************************/
#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0) )
#ifdef CONFIG_PCI_IOV
int __kc_pci_vfs_assigned(struct pci_dev *dev)
{
	unsigned int vfs_assigned = 0;
#ifdef HAVE_PCI_DEV_FLAGS_ASSIGNED
	int pos;
	struct pci_dev *vfdev;
	unsigned short dev_id;

	/* only search if we are a PF */
	if (!dev->is_physfn)
		return 0;

	/* find SR-IOV capability */
	pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_SRIOV);
	if (!pos)
		return 0;

	/*
	 * determine the device ID for the VFs, the vendor ID will be the
	 * same as the PF so there is no need to check for that one
	 */
	pci_read_config_word(dev, pos + PCI_SRIOV_VF_DID, &dev_id);

	/* loop through all the VFs to see if we own any that are assigned */
	vfdev = pci_get_device(dev->vendor, dev_id, NULL);
	while (vfdev) {
		/*
		 * It is considered assigned if it is a virtual function with
		 * our dev as the physical function and the assigned bit is set
		 */
		if (vfdev->is_virtfn && (vfdev->physfn == dev) &&
		    (vfdev->dev_flags & PCI_DEV_FLAGS_ASSIGNED))
			vfs_assigned++;

		vfdev = pci_get_device(dev->vendor, dev_id, vfdev);
	}

#endif /* HAVE_PCI_DEV_FLAGS_ASSIGNED */
	return vfs_assigned;
}

#endif /* CONFIG_PCI_IOV */
#endif /* 3.10.0 */
