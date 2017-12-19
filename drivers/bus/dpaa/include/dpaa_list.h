/* SPDX-License-Identifier: BSD-3-Clause
 *
 *   Copyright 2017 NXP
 *
 */

#ifndef __DPAA_LIST_H
#define __DPAA_LIST_H

/****************/
/* Linked-lists */
/****************/

struct list_head {
	struct list_head *prev;
	struct list_head *next;
};

#define COMPAT_LIST_HEAD(n) \
struct list_head n = { \
	.prev = &n, \
	.next = &n \
}

#define INIT_LIST_HEAD(p) \
do { \
	struct list_head *__p298 = (p); \
	__p298->next = __p298; \
	__p298->prev = __p298->next; \
} while (0)
#define list_entry(node, type, member) \
	(type *)((void *)node - offsetof(type, member))
#define list_empty(p) \
({ \
	const struct list_head *__p298 = (p); \
	((__p298->next == __p298) && (__p298->prev == __p298)); \
})
#define list_add(p, l) \
do { \
	struct list_head *__p298 = (p); \
	struct list_head *__l298 = (l); \
	__p298->next = __l298->next; \
	__p298->prev = __l298; \
	__l298->next->prev = __p298; \
	__l298->next = __p298; \
} while (0)
#define list_add_tail(p, l) \
do { \
	struct list_head *__p298 = (p); \
	struct list_head *__l298 = (l); \
	__p298->prev = __l298->prev; \
	__p298->next = __l298; \
	__l298->prev->next = __p298; \
	__l298->prev = __p298; \
} while (0)
#define list_for_each(i, l)				\
	for (i = (l)->next; i != (l); i = i->next)
#define list_for_each_safe(i, j, l)			\
	for (i = (l)->next, j = i->next; i != (l);	\
	     i = j, j = i->next)
#define list_for_each_entry(i, l, name) \
	for (i = list_entry((l)->next, typeof(*i), name); &i->name != (l); \
		i = list_entry(i->name.next, typeof(*i), name))
#define list_for_each_entry_safe(i, j, l, name) \
	for (i = list_entry((l)->next, typeof(*i), name), \
		j = list_entry(i->name.next, typeof(*j), name); \
		&i->name != (l); \
		i = j, j = list_entry(j->name.next, typeof(*j), name))
#define list_del(i) \
do { \
	(i)->next->prev = (i)->prev; \
	(i)->prev->next = (i)->next; \
} while (0)

#endif /* __DPAA_LIST_H */
