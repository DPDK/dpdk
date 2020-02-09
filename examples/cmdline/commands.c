/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright (c) 2009, Olivier MATZ <zer0@droids-corp.org>
 * All rights reserved.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <netinet/in.h>
#include <termios.h>
#ifdef RTE_EXEC_ENV_FREEBSD
#include <sys/socket.h>
#endif

#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_ipaddr.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>
#include <cmdline.h>

#include <rte_string_fns.h>

#include "parse_obj_list.h"

struct object_list global_obj_list;

/* not defined under linux */
#ifndef NIPQUAD
#define NIPQUAD_FMT "%u.%u.%u.%u"
#define NIPQUAD(addr)				\
	(unsigned)((unsigned char *)&addr)[0],	\
	(unsigned)((unsigned char *)&addr)[1],	\
	(unsigned)((unsigned char *)&addr)[2],	\
	(unsigned)((unsigned char *)&addr)[3]
#endif

#ifndef NIP6
#define NIP6_FMT "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define NIP6(addr)					\
	(unsigned)((addr).s6_addr[0]),			\
	(unsigned)((addr).s6_addr[1]),			\
	(unsigned)((addr).s6_addr[2]),			\
	(unsigned)((addr).s6_addr[3]),			\
	(unsigned)((addr).s6_addr[4]),			\
	(unsigned)((addr).s6_addr[5]),			\
	(unsigned)((addr).s6_addr[6]),			\
	(unsigned)((addr).s6_addr[7]),			\
	(unsigned)((addr).s6_addr[8]),			\
	(unsigned)((addr).s6_addr[9]),			\
	(unsigned)((addr).s6_addr[10]),			\
	(unsigned)((addr).s6_addr[11]),			\
	(unsigned)((addr).s6_addr[12]),			\
	(unsigned)((addr).s6_addr[13]),			\
	(unsigned)((addr).s6_addr[14]),			\
	(unsigned)((addr).s6_addr[15])
#endif


/**********************************************************/

struct cmd_obj_del_show_result {
	cmdline_fixed_string_t action;
	struct object *obj;
};

static void cmd_obj_del_show_parsed(void *parsed_result,
				    struct cmdline *cl,
				    __rte_unused void *data)
{
	struct cmd_obj_del_show_result *res = parsed_result;
	char ip_str[INET6_ADDRSTRLEN];

	if (res->obj->ip.family == AF_INET)
		snprintf(ip_str, sizeof(ip_str), NIPQUAD_FMT,
			 NIPQUAD(res->obj->ip.addr.ipv4));
	else
		snprintf(ip_str, sizeof(ip_str), NIP6_FMT,
			 NIP6(res->obj->ip.addr.ipv6));

	if (strcmp(res->action, "del") == 0) {
		SLIST_REMOVE(&global_obj_list, res->obj, object, next);
		cmdline_printf(cl, "Object %s removed, ip=%s\n",
			       res->obj->name, ip_str);
		free(res->obj);
	}
	else if (strcmp(res->action, "show") == 0) {
		cmdline_printf(cl, "Object %s, ip=%s\n",
			       res->obj->name, ip_str);
	}
}

cmdline_parse_token_string_t cmd_obj_action =
	TOKEN_STRING_INITIALIZER(struct cmd_obj_del_show_result,
				 action, "show#del");
parse_token_obj_list_t cmd_obj_obj =
	TOKEN_OBJ_LIST_INITIALIZER(struct cmd_obj_del_show_result, obj,
				   &global_obj_list);

cmdline_parse_inst_t cmd_obj_del_show = {
	.f = cmd_obj_del_show_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "Show/del an object",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_obj_action,
		(void *)&cmd_obj_obj,
		NULL,
	},
};

/**********************************************************/

struct cmd_obj_add_result {
	cmdline_fixed_string_t action;
	cmdline_fixed_string_t name;
	cmdline_ipaddr_t ip;
};

static void cmd_obj_add_parsed(void *parsed_result,
			       struct cmdline *cl,
			       __rte_unused void *data)
{
	struct cmd_obj_add_result *res = parsed_result;
	struct object *o;
	char ip_str[INET6_ADDRSTRLEN];

	SLIST_FOREACH(o, &global_obj_list, next) {
		if (!strcmp(res->name, o->name)) {
			cmdline_printf(cl, "Object %s already exist\n", res->name);
			return;
		}
		break;
	}

	o = malloc(sizeof(*o));
	if (!o) {
		cmdline_printf(cl, "mem error\n");
		return;
	}
	strlcpy(o->name, res->name, sizeof(o->name));
	o->ip = res->ip;
	SLIST_INSERT_HEAD(&global_obj_list, o, next);

	if (o->ip.family == AF_INET)
		snprintf(ip_str, sizeof(ip_str), NIPQUAD_FMT,
			 NIPQUAD(o->ip.addr.ipv4));
	else
		snprintf(ip_str, sizeof(ip_str), NIP6_FMT,
			 NIP6(o->ip.addr.ipv6));

	cmdline_printf(cl, "Object %s added, ip=%s\n",
		       o->name, ip_str);
}

cmdline_parse_token_string_t cmd_obj_action_add =
	TOKEN_STRING_INITIALIZER(struct cmd_obj_add_result, action, "add");
cmdline_parse_token_string_t cmd_obj_name =
	TOKEN_STRING_INITIALIZER(struct cmd_obj_add_result, name, NULL);
cmdline_parse_token_ipaddr_t cmd_obj_ip =
	TOKEN_IPADDR_INITIALIZER(struct cmd_obj_add_result, ip);

cmdline_parse_inst_t cmd_obj_add = {
	.f = cmd_obj_add_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "Add an object (name, val)",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_obj_action_add,
		(void *)&cmd_obj_name,
		(void *)&cmd_obj_ip,
		NULL,
	},
};

/**********************************************************/

struct cmd_help_result {
	cmdline_fixed_string_t help;
};

static void cmd_help_parsed(__rte_unused void *parsed_result,
			    struct cmdline *cl,
			    __rte_unused void *data)
{
	cmdline_printf(cl,
		       "Demo example of command line interface in RTE\n\n"
		       "This is a readline-like interface that can be used to\n"
		       "debug your RTE application. It supports some features\n"
		       "of GNU readline like completion, cut/paste, and some\n"
		       "other special bindings.\n\n"
		       "This demo shows how rte_cmdline library can be\n"
		       "extended to handle a list of objects. There are\n"
		       "3 commands:\n"
		       "- add obj_name IP\n"
		       "- del obj_name\n"
		       "- show obj_name\n\n");
}

cmdline_parse_token_string_t cmd_help_help =
	TOKEN_STRING_INITIALIZER(struct cmd_help_result, help, "help");

cmdline_parse_inst_t cmd_help = {
	.f = cmd_help_parsed,  /* function to call */
	.data = NULL,      /* 2nd arg of func */
	.help_str = "show help",
	.tokens = {        /* token list, NULL terminated */
		(void *)&cmd_help_help,
		NULL,
	},
};


/**********************************************************/
/**********************************************************/
/****** CONTEXT (list of instruction) */

cmdline_parse_ctx_t main_ctx[] = {
	(cmdline_parse_inst_t *)&cmd_obj_del_show,
	(cmdline_parse_inst_t *)&cmd_obj_add,
	(cmdline_parse_inst_t *)&cmd_help,
	NULL,
};
