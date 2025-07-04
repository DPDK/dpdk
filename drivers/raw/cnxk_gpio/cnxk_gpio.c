/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/gpio.h>
#include <regex.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <bus_vdev_driver.h>
#include <rte_eal.h>
#include <rte_errno.h>
#include <rte_interrupts.h>
#include <rte_kvargs.h>
#include <rte_lcore.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_gpio.h"
#include "rte_pmd_cnxk_gpio.h"

#define CNXK_GPIO_PARAMS_MZ_NAME "cnxk_gpio_params_mz"
#define CNXK_GPIO_INVALID_FD (-1)

RTE_LOG_REGISTER_SUFFIX(cnxk_logtype_gpio, gpio, INFO);

#ifdef CNXK_GPIO_V2_PRESENT

struct cnxk_gpio_params {
	unsigned int num;
	char allowlist[];
};

static const char *const cnxk_gpio_args[] = {
#define CNXK_GPIO_ARG_GPIOCHIP "gpiochip"
	CNXK_GPIO_ARG_GPIOCHIP,
#define CNXK_GPIO_ARG_ALLOWLIST "allowlist"
	CNXK_GPIO_ARG_ALLOWLIST,
	NULL
};

static void
cnxk_gpio_format_name(char *name, size_t len)
{
	snprintf(name, len, "cnxk_gpio");
}

static int
cnxk_gpio_ioctl(struct cnxk_gpio *gpio, unsigned long cmd, void *arg)
{
	return ioctl(gpio->fd, cmd, arg) ? -errno : 0;
}

static int
cnxk_gpio_gpiochip_ioctl(struct cnxk_gpiochip *gpiochip, unsigned long cmd, void *arg)
{
	char path[PATH_MAX];
	int ret = 0, fd;

	snprintf(path, sizeof(path), "/dev/gpiochip%d", gpiochip->num);
	fd = open(path, O_RDONLY);
	if (fd == -1)
		return -errno;

	if (ioctl(fd, cmd, arg))
		ret = -errno;

	close(fd);

	return ret;
}

static int
cnxk_gpio_filter_gpiochip(const struct dirent *dirent)
{
	const char *pattern = "gpiochip";

	return !strncmp(dirent->d_name, pattern, strlen(pattern));
}

static int
cnxk_gpio_set_defaults(struct cnxk_gpio_params *params)
{
	struct dirent **namelist;
	int ret = 0, n;

	n = scandir("/dev", &namelist, cnxk_gpio_filter_gpiochip, alphasort);
	if (n < 0 || n == 0)
		return -ENODEV;

	if (sscanf(namelist[0]->d_name, "gpiochip%d", &params->num) != 1)
		ret = -EINVAL;

	while (n--)
		free(namelist[n]);
	free(namelist);

	return ret;
}

static int
cnxk_gpio_parse_arg_gpiochip(const char *key __rte_unused, const char *value,
			     void *extra_args)
{
	unsigned long val;

	errno = 0;
	val = strtoul(value, NULL, 10);
	if (errno)
		return -errno;

	*(unsigned int *)extra_args = val;

	return 0;
}

static int
cnxk_gpio_parse_arg_allowlist(const char *key __rte_unused, const char *value, void *extra_args)
{
	*(const char **)extra_args = value;

	return 0;
}

static int
cnxk_gpio_params_restore(struct cnxk_gpio_params **params)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(CNXK_GPIO_PARAMS_MZ_NAME);
	if (!mz)
		return -ENODEV;

	*params = mz->addr;

	return 0;
}

static struct cnxk_gpio_params *
cnxk_gpio_params_reserve(size_t len)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_reserve(CNXK_GPIO_PARAMS_MZ_NAME, len, rte_socket_id(), 0);
	if (!mz)
		return NULL;

	return mz->addr;
}

static void
cnxk_gpio_params_release(void)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_memzone_free(rte_memzone_lookup(CNXK_GPIO_PARAMS_MZ_NAME));
}

static int
cnxk_gpio_parse_arg(struct rte_kvargs *kvlist, const char *arg, arg_handler_t handler, void *data)
{
	int ret;

	ret = rte_kvargs_count(kvlist, arg);
	if (ret == 0)
		return 0;
	if (ret > 1)
		return -EINVAL;

	return rte_kvargs_process(kvlist, arg, handler, data) ? -EIO : 1;
}

static int
cnxk_gpio_parse_store_args(struct cnxk_gpio_params **params, const char *args)
{
	size_t len = sizeof(**params) + 1;
	const char *allowlist = NULL;
	struct rte_kvargs *kvlist;
	int ret;

	kvlist = rte_kvargs_parse(args, cnxk_gpio_args);
	if (!kvlist) {
		*params = cnxk_gpio_params_reserve(len);
		if (!*params)
			return -ENOMEM;

		ret = cnxk_gpio_set_defaults(*params);
		if (ret)
			goto out;

		return 0;
	}

	ret = cnxk_gpio_parse_arg(kvlist, CNXK_GPIO_ARG_ALLOWLIST, cnxk_gpio_parse_arg_allowlist,
				  &allowlist);
	if (ret < 0) {
		ret = -EINVAL;
		goto out;
	}

	if (allowlist)
		len += strlen(allowlist);

	*params = cnxk_gpio_params_reserve(len);
	if (!(*params)) {
		ret = -ENOMEM;
		goto out;
	}

	if (allowlist)
		strlcpy((*params)->allowlist, allowlist, strlen(allowlist) + 1);

	ret = cnxk_gpio_parse_arg(kvlist, CNXK_GPIO_ARG_GPIOCHIP, cnxk_gpio_parse_arg_gpiochip,
				  &(*params)->num);
	if (ret == 0)
		ret = cnxk_gpio_set_defaults(*params);

out:
	rte_kvargs_free(kvlist);

	return ret;
}

static bool
cnxk_gpio_allowlist_valid(const char *allowlist)
{
	bool ret = false;
	regex_t regex;

	/* [gpio0<,gpio1,...,gpioN>], where '<...>' is optional part */
	if (regcomp(&regex, "^\\[[0-9]+(,[0-9]+)*\\]$", REG_EXTENDED))
		return ret;

	if (!regexec(&regex, allowlist, 0, NULL, 0))
		ret = true;

	regfree(&regex);

	return ret;
}

static int
cnxk_gpio_parse_allowlist(struct cnxk_gpiochip *gpiochip, char *allowlist)
{
	int i, ret, val, queue = 0;
	char *token;
	int *list;

	list = rte_calloc(NULL, gpiochip->num_gpios, sizeof(*list), 0);
	if (!list)
		return -ENOMEM;

	/* no gpios provided so allow all */
	if (!*allowlist) {
		for (i = 0; i < gpiochip->num_gpios; i++)
			list[queue++] = i;

		goto out_done;
	}

	if (!cnxk_gpio_allowlist_valid(allowlist))
		return -EINVAL;

	allowlist = strdup(allowlist);
	if (!allowlist) {
		ret = -ENOMEM;
		goto out;
	}

	/* replace brackets with something meaningless for strtol() */
	allowlist[0] = ' ';
	allowlist[strlen(allowlist) - 1] = ' ';

	/* quiesce -Wcast-qual */
	token = strtok((char *)(uintptr_t)allowlist, ",");
	do {
		errno = 0;
		val = strtol(token, NULL, 10);
		if (errno) {
			CNXK_GPIO_LOG(ERR, "failed to parse %s", token);
			ret = -errno;
			goto out;
		}

		if (val < 0 || val >= gpiochip->num_gpios) {
			CNXK_GPIO_LOG(ERR, "gpio%d out of 0-%d range", val,
				gpiochip->num_gpios - 1);
			ret = -EINVAL;
			goto out;
		}

		for (i = 0; i < queue; i++) {
			if (list[i] != val)
				continue;

			CNXK_GPIO_LOG(WARNING, "gpio%d already allowed", val);
			break;
		}
		if (i == queue)
			list[queue++] = val;
	} while ((token = strtok(NULL, ",")));

	free(allowlist);
out_done:
	gpiochip->allowlist = list;
	gpiochip->num_queues = queue;

	return 0;
out:
	free(allowlist);
	rte_free(list);

	return ret;
}

static bool
cnxk_gpio_queue_valid(struct cnxk_gpiochip *gpiochip, uint16_t queue)
{
	return queue < gpiochip->num_queues;
}

static int
cnxk_queue_to_gpio(struct cnxk_gpiochip *gpiochip, uint16_t queue)
{
	return gpiochip->allowlist ? gpiochip->allowlist[queue] : queue;
}

static struct cnxk_gpio *
cnxk_gpio_lookup(struct cnxk_gpiochip *gpiochip, uint16_t queue)
{
	int gpio = cnxk_queue_to_gpio(gpiochip, queue);

	return gpiochip->gpios[gpio];
}

static bool
cnxk_gpio_available(struct cnxk_gpio *gpio)
{
	struct gpio_v2_line_info info = { .offset = gpio->num };
	int ret;

	ret = cnxk_gpio_gpiochip_ioctl(gpio->gpiochip, GPIO_V2_GET_LINEINFO_IOCTL, &info);
	if (ret)
		return false;

	return !(info.flags & GPIO_V2_LINE_FLAG_USED);
}

static int
cnxk_gpio_queue_setup(struct rte_rawdev *dev, uint16_t queue_id,
		      rte_rawdev_obj_t queue_conf, size_t queue_conf_size)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	struct gpio_v2_line_request req = {0};
	struct cnxk_gpio *gpio;
	int ret;

	RTE_SET_USED(queue_conf);
	RTE_SET_USED(queue_conf_size);

	if (!cnxk_gpio_queue_valid(gpiochip, queue_id))
		return -EINVAL;

	gpio = cnxk_gpio_lookup(gpiochip, queue_id);
	if (gpio)
		return -EEXIST;

	gpio = rte_zmalloc(NULL, sizeof(*gpio), 0);
	if (!gpio)
		return -ENOMEM;

	gpio->num = cnxk_queue_to_gpio(gpiochip, queue_id);
	gpio->fd = CNXK_GPIO_INVALID_FD;
	gpio->gpiochip = gpiochip;

	if (!cnxk_gpio_available(gpio)) {
		rte_free(gpio);
		return -EBUSY;
	}

	cnxk_gpio_format_name(req.consumer, sizeof(req.consumer));
	req.offsets[req.num_lines] = gpio->num;
	req.num_lines = 1;

	ret = cnxk_gpio_gpiochip_ioctl(gpio->gpiochip, GPIO_V2_GET_LINE_IOCTL, &req);
	if (ret) {
		rte_free(gpio);
		return ret;
	}

	gpio->fd = req.fd;
	gpiochip->gpios[gpio->num] = gpio;

	return 0;
}

static void
cnxk_gpio_intr_handler(void *data)
{
	struct gpio_v2_line_event event;
	struct cnxk_gpio *gpio = data;
	int ret;

	ret = read(gpio->fd, &event, sizeof(event));
	if (ret != sizeof(event)) {
		CNXK_GPIO_LOG(ERR, "failed to read gpio%d event data", gpio->num);
		goto out;
	}
	if ((unsigned int)gpio->num != event.offset) {
		CNXK_GPIO_LOG(ERR, "expected event from gpio%d, received from gpio%d",
			      gpio->num, event.offset);
		goto out;
	}

	if (gpio->intr.handler2)
		(gpio->intr.handler2)(&event, gpio->intr.data);
	else if (gpio->intr.handler)
		(gpio->intr.handler)(gpio->num, gpio->intr.data);
out:
	rte_intr_ack(gpio->intr.intr_handle);
}

static int
cnxk_gpio_unregister_irq(struct cnxk_gpio *gpio)
{
	int ret;

	if (!gpio->intr.intr_handle)
		return 0;

	ret = rte_intr_disable(gpio->intr.intr_handle);
	if (ret)
		return ret;

	ret = rte_intr_callback_unregister_sync(gpio->intr.intr_handle, cnxk_gpio_intr_handler,
						(void *)-1);
	if (ret)
		return ret;

	rte_intr_instance_free(gpio->intr.intr_handle);
	gpio->intr.intr_handle = NULL;

	return 0;
}


static int
cnxk_gpio_queue_release(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	struct cnxk_gpio *gpio;
	int num;

	if (!cnxk_gpio_queue_valid(gpiochip, queue_id))
		return -EINVAL;

	gpio = cnxk_gpio_lookup(gpiochip, queue_id);
	if (!gpio)
		return -ENODEV;

	if (gpio->intr.intr_handle)
		cnxk_gpio_unregister_irq(gpio);

	if (gpio->fd != CNXK_GPIO_INVALID_FD)
		close(gpio->fd);

	num = cnxk_queue_to_gpio(gpiochip, queue_id);
	gpiochip->gpios[num] = NULL;
	rte_free(gpio);

	return 0;
}

static int
cnxk_gpio_queue_def_conf(struct rte_rawdev *dev, uint16_t queue_id,
			 rte_rawdev_obj_t queue_conf, size_t queue_conf_size)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	struct cnxk_gpio_queue_conf *conf = queue_conf;

	if (!cnxk_gpio_queue_valid(gpiochip, queue_id))
		return -EINVAL;

	if (queue_conf_size != sizeof(*conf))
		return -EINVAL;

	conf->size = 1;
	conf->gpio = cnxk_queue_to_gpio(gpiochip, queue_id);

	return 0;
}

static uint16_t
cnxk_gpio_queue_count(struct rte_rawdev *dev)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;

	return gpiochip->num_queues;
}

static const struct {
	enum cnxk_gpio_pin_edge edge;
	enum gpio_v2_line_flag flag;
} cnxk_gpio_edge_flag[] = {
	{ CNXK_GPIO_PIN_EDGE_NONE, 0 },
	{ CNXK_GPIO_PIN_EDGE_FALLING, GPIO_V2_LINE_FLAG_EDGE_FALLING },
	{ CNXK_GPIO_PIN_EDGE_RISING, GPIO_V2_LINE_FLAG_EDGE_RISING },
	{ CNXK_GPIO_PIN_EDGE_BOTH, GPIO_V2_LINE_FLAG_EDGE_FALLING | GPIO_V2_LINE_FLAG_EDGE_RISING },
};

static int
cnxk_gpio_edge_to_flag(enum cnxk_gpio_pin_edge edge)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_edge_flag); i++) {
		if (cnxk_gpio_edge_flag[i].edge == edge)
			break;
	}
	if (i == RTE_DIM(cnxk_gpio_edge_flag))
		return -EINVAL;

	return cnxk_gpio_edge_flag[i].flag;
}

static int
cnxk_gpio_flag_to_edge(enum gpio_v2_line_flag flag)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_edge_flag); i++) {
		if ((cnxk_gpio_edge_flag[i].flag & flag) == cnxk_gpio_edge_flag[i].flag)
			break;
	}
	if (i == RTE_DIM(cnxk_gpio_edge_flag))
		return -EINVAL;

	return cnxk_gpio_edge_flag[i].edge;
}

static const struct {
	enum cnxk_gpio_pin_dir dir;
	enum gpio_v2_line_flag flag;
} cnxk_gpio_dir_flag[] = {
	{ CNXK_GPIO_PIN_DIR_IN, GPIO_V2_LINE_FLAG_INPUT },
	{ CNXK_GPIO_PIN_DIR_OUT, GPIO_V2_LINE_FLAG_OUTPUT },
	{ CNXK_GPIO_PIN_DIR_HIGH, GPIO_V2_LINE_FLAG_OUTPUT },
	{ CNXK_GPIO_PIN_DIR_LOW, GPIO_V2_LINE_FLAG_OUTPUT },
};

static int
cnxk_gpio_dir_to_flag(enum cnxk_gpio_pin_dir dir)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_dir_flag); i++) {
		if (cnxk_gpio_dir_flag[i].dir == dir)
			break;
	}
	if (i == RTE_DIM(cnxk_gpio_dir_flag))
		return -EINVAL;

	return cnxk_gpio_dir_flag[i].flag;
}

static int
cnxk_gpio_flag_to_dir(enum gpio_v2_line_flag flag)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_dir_flag); i++) {
		if ((cnxk_gpio_dir_flag[i].flag & flag) == cnxk_gpio_dir_flag[i].flag)
			break;
	}
	if (i == RTE_DIM(cnxk_gpio_dir_flag))
		return -EINVAL;

	return cnxk_gpio_dir_flag[i].dir;
}

static int
cnxk_gpio_register_irq_compat(struct cnxk_gpio *gpio, struct cnxk_gpio_irq *irq,
			      struct cnxk_gpio_irq2 *irq2)
{
	struct rte_intr_handle *intr_handle;
	int ret;

	if (!irq && !irq2)
		return -EINVAL;

	if ((irq && !irq->handler) || (irq2 && !irq2->handler))
		return -EINVAL;

	if (gpio->intr.intr_handle)
		return -EEXIST;

	intr_handle = rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_PRIVATE);
	if (!intr_handle)
		return -ENOMEM;

	if (rte_intr_type_set(intr_handle, RTE_INTR_HANDLE_VDEV)) {
		ret = -rte_errno;
		goto out;
	}

	if (rte_intr_fd_set(intr_handle, gpio->fd)) {
		ret = -rte_errno;
		goto out;
	}

	if (rte_intr_callback_register(intr_handle, cnxk_gpio_intr_handler, gpio)) {
		ret = -rte_errno;
		goto out;
	}

	gpio->intr.intr_handle = intr_handle;

	if (irq) {
		gpio->intr.data = irq->data;
		gpio->intr.handler = irq->handler;
	} else {
		gpio->intr.data = irq2->data;
		gpio->intr.handler2 = irq2->handler;
	}

	if (rte_intr_enable(gpio->intr.intr_handle)) {
		ret = -EINVAL;
		goto out;
	}

	return 0;
out:
	rte_intr_instance_free(intr_handle);

	return ret;
}

static int
cnxk_gpio_register_irq(struct cnxk_gpio *gpio, struct cnxk_gpio_irq *irq)
{
	CNXK_GPIO_LOG(WARNING, "using deprecated interrupt registration api");

	return cnxk_gpio_register_irq_compat(gpio, irq, NULL);
}

static int
cnxk_gpio_register_irq2(struct cnxk_gpio *gpio, struct cnxk_gpio_irq2 *irq)
{
	return cnxk_gpio_register_irq_compat(gpio, NULL, irq);
}

static int
cnxk_gpio_process_buf(struct cnxk_gpio *gpio, struct rte_rawdev_buf *rbuf)
{
	struct cnxk_gpio_msg *msg = rbuf->buf_addr;
	struct gpio_v2_line_values values = {0};
	struct gpio_v2_line_config config = {0};
	struct gpio_v2_line_info info = {0};
	enum cnxk_gpio_pin_edge edge;
	enum cnxk_gpio_pin_dir dir;
	void *rsp = NULL;
	int ret;

	info.offset = gpio->num;
	ret = cnxk_gpio_gpiochip_ioctl(gpio->gpiochip, GPIO_V2_GET_LINEINFO_IOCTL, &info);
	if (ret)
		return ret;

	info.flags &= ~GPIO_V2_LINE_FLAG_USED;

	switch (msg->type) {
	case CNXK_GPIO_MSG_TYPE_SET_PIN_VALUE:
		values.bits = *(int *)msg->data ?  RTE_BIT64(gpio->num) : 0;
		values.mask = RTE_BIT64(gpio->num);

		ret = cnxk_gpio_ioctl(gpio, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
		break;
	case CNXK_GPIO_MSG_TYPE_SET_PIN_EDGE:
		edge = *(enum cnxk_gpio_pin_edge *)msg->data;
		info.flags &= ~(GPIO_V2_LINE_FLAG_EDGE_RISING | GPIO_V2_LINE_FLAG_EDGE_FALLING);
		ret = cnxk_gpio_edge_to_flag(edge);
		if (ret < 0)
			break;
		info.flags |= ret;

		config.attrs[config.num_attrs].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
		config.attrs[config.num_attrs].attr.flags = info.flags;
		config.attrs[config.num_attrs].mask = RTE_BIT64(gpio->num);
		config.num_attrs++;

		ret = cnxk_gpio_ioctl(gpio, GPIO_V2_LINE_SET_CONFIG_IOCTL, &config);
		break;
	case CNXK_GPIO_MSG_TYPE_SET_PIN_DIR:
		dir = *(enum cnxk_gpio_pin_dir *)msg->data;
		config.attrs[config.num_attrs].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
		ret = cnxk_gpio_dir_to_flag(dir);
		if (ret < 0)
			break;
		config.attrs[config.num_attrs].attr.flags = ret;
		config.attrs[config.num_attrs].mask = RTE_BIT64(gpio->num);
		config.num_attrs++;

		if (dir == CNXK_GPIO_PIN_DIR_HIGH || dir == CNXK_GPIO_PIN_DIR_LOW) {
			config.attrs[config.num_attrs].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
			config.attrs[config.num_attrs].attr.values = dir == CNXK_GPIO_PIN_DIR_HIGH ?
								     RTE_BIT64(gpio->num) : 0;
			config.attrs[config.num_attrs].mask = RTE_BIT64(gpio->num);
			config.num_attrs++;
		}

		ret = cnxk_gpio_ioctl(gpio, GPIO_V2_LINE_SET_CONFIG_IOCTL, &config);
		break;
	case CNXK_GPIO_MSG_TYPE_SET_PIN_ACTIVE_LOW:
		if (*(int *)msg->data)
			info.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
		else
			info.flags &= ~GPIO_V2_LINE_FLAG_ACTIVE_LOW;

		config.attrs[config.num_attrs].attr.id = GPIO_V2_LINE_ATTR_ID_FLAGS;
		config.attrs[config.num_attrs].attr.flags = info.flags;
		config.attrs[config.num_attrs].mask = RTE_BIT64(gpio->num);
		config.num_attrs++;

		ret = cnxk_gpio_ioctl(gpio, GPIO_V2_LINE_SET_CONFIG_IOCTL, &config);
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_VALUE:
		values.mask = RTE_BIT64(gpio->num);
		ret = cnxk_gpio_ioctl(gpio, GPIO_V2_LINE_GET_VALUES_IOCTL, &values);
		if (ret)
			break;

		rsp = rte_zmalloc(NULL, sizeof(int), 0);
		if (!rsp)
			return -ENOMEM;

		*(int *)rsp = !!(values.bits & RTE_BIT64(gpio->num));
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_EDGE:
		ret = cnxk_gpio_flag_to_edge(info.flags);
		if (ret < 0)
			return ret;

		rsp = rte_zmalloc(NULL, sizeof(enum cnxk_gpio_pin_edge), 0);
		if (!rsp)
			return -ENOMEM;

		*(enum cnxk_gpio_pin_edge *)rsp = ret;
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_DIR:
		ret = cnxk_gpio_flag_to_dir(info.flags);
		if (ret < 0)
			return ret;

		rsp = rte_zmalloc(NULL, sizeof(enum cnxk_gpio_pin_edge), 0);
		if (!rsp)
			return -ENOMEM;

		*(enum cnxk_gpio_pin_dir *)rsp = ret;
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_ACTIVE_LOW:
		rsp = rte_zmalloc(NULL, sizeof(int), 0);
		if (!rsp)
			return -ENOMEM;

		*(int *)rsp = !!(info.flags & GPIO_V2_LINE_FLAG_ACTIVE_LOW);
		break;
	case CNXK_GPIO_MSG_TYPE_REGISTER_IRQ:
		ret = cnxk_gpio_register_irq(gpio, (struct cnxk_gpio_irq *)msg->data);
		break;
	case CNXK_GPIO_MSG_TYPE_REGISTER_IRQ2:
		ret = cnxk_gpio_register_irq2(gpio, (struct cnxk_gpio_irq2 *)msg->data);
		break;
	case CNXK_GPIO_MSG_TYPE_UNREGISTER_IRQ:
		ret = cnxk_gpio_unregister_irq(gpio);
		break;
	default:
		return -EINVAL;
	}

	/* get rid of last response if any */
	if (gpio->rsp) {
		CNXK_GPIO_LOG(WARNING, "previous response got overwritten");
		rte_free(gpio->rsp);
	}
	gpio->rsp = rsp;

	return ret;
}

static bool
cnxk_gpio_valid(struct cnxk_gpiochip *gpiochip, int gpio)
{
	return gpio < gpiochip->num_gpios && gpiochip->gpios[gpio];
}

static int
cnxk_gpio_enqueue_bufs(struct rte_rawdev *dev, struct rte_rawdev_buf **buffers,
		       unsigned int count, rte_rawdev_obj_t context)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	unsigned int gpio_num = (size_t)context;
	struct cnxk_gpio *gpio;
	int ret;

	if (count == 0)
		return 0;

	if (!cnxk_gpio_valid(gpiochip, gpio_num))
		return -EINVAL;
	gpio = gpiochip->gpios[gpio_num];

	ret = cnxk_gpio_process_buf(gpio, buffers[0]);
	if (ret)
		return ret;

	return 1;
}

static int
cnxk_gpio_dequeue_bufs(struct rte_rawdev *dev, struct rte_rawdev_buf **buffers,
		       unsigned int count, rte_rawdev_obj_t context)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	unsigned int gpio_num = (size_t)context;
	struct cnxk_gpio *gpio;

	if (count == 0)
		return 0;

	if (!cnxk_gpio_valid(gpiochip, gpio_num))
		return -EINVAL;
	gpio = gpiochip->gpios[gpio_num];

	if (gpio->rsp) {
		buffers[0]->buf_addr = gpio->rsp;
		gpio->rsp = NULL;

		return 1;
	}

	return 0;
}

static int
cnxk_gpio_dev_close(struct rte_rawdev *dev)
{
	RTE_SET_USED(dev);

	return 0;
}

static const struct rte_rawdev_ops cnxk_gpio_rawdev_ops = {
	.dev_close = cnxk_gpio_dev_close,
	.enqueue_bufs = cnxk_gpio_enqueue_bufs,
	.dequeue_bufs = cnxk_gpio_dequeue_bufs,
	.queue_def_conf = cnxk_gpio_queue_def_conf,
	.queue_count = cnxk_gpio_queue_count,
	.queue_setup = cnxk_gpio_queue_setup,
	.queue_release = cnxk_gpio_queue_release,
	.dev_selftest = cnxk_gpio_selftest,
};

static int
cnxk_gpio_probe(struct rte_vdev_device *dev)
{
	struct gpiochip_info gpiochip_info;
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct cnxk_gpio_params *params;
	struct cnxk_gpiochip *gpiochip;
	struct rte_rawdev *rawdev;
	int ret;

	cnxk_gpio_format_name(name, sizeof(name));
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(*gpiochip), rte_socket_id());
	if (!rawdev) {
		CNXK_GPIO_LOG(ERR, "failed to allocate %s rawdev", name);
		return -ENOMEM;
	}

	rawdev->dev_ops = &cnxk_gpio_rawdev_ops;
	rawdev->device = &dev->device;
	rawdev->driver_name = dev->device.name;
	gpiochip = rawdev->dev_private;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		ret = cnxk_gpio_parse_store_args(&params, rte_vdev_device_args(dev));
		if (ret < 0)
			goto out;
	} else {
		ret = cnxk_gpio_params_restore(&params);
		if (ret)
			goto out;
	}

	gpiochip->num = params->num;

	/* read number of available gpios */
	ret = cnxk_gpio_gpiochip_ioctl(gpiochip, GPIO_GET_CHIPINFO_IOCTL, &gpiochip_info);
	if (ret) {
		CNXK_GPIO_LOG(ERR, "failed to read /dev/gpiochip%d info", gpiochip->num);
		goto out;
	}

	gpiochip->num_gpios = gpiochip_info.lines;
	gpiochip->num_queues = gpiochip->num_gpios;

	ret = cnxk_gpio_parse_allowlist(gpiochip, params->allowlist);
	if (ret) {
		CNXK_GPIO_LOG(ERR, "failed to parse allowed gpios");
		goto out;
	}

	gpiochip->gpios = rte_calloc(NULL, gpiochip->num_gpios, sizeof(struct cnxk_gpio *), 0);
	if (!gpiochip->gpios) {
		CNXK_GPIO_LOG(ERR, "failed to allocate gpios memory");
		ret = -ENOMEM;
		goto out;
	}

	return 0;
out:
	rte_free(gpiochip->allowlist);
	cnxk_gpio_params_release();
	rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
cnxk_gpio_remove(struct rte_vdev_device *dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct cnxk_gpiochip *gpiochip;
	struct rte_rawdev *rawdev;
	struct cnxk_gpio *gpio;
	int i;

	RTE_SET_USED(dev);

	cnxk_gpio_format_name(name, sizeof(name));
	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rawdev)
		return -ENODEV;

	gpiochip = rawdev->dev_private;
	for (i = 0; i < gpiochip->num_gpios; i++) {
		gpio = gpiochip->gpios[i];
		if (!gpio)
			continue;

		cnxk_gpio_queue_release(rawdev, gpio->num);
	}

	rte_free(gpiochip->allowlist);
	rte_free(gpiochip->gpios);
	cnxk_gpio_params_release();
	rte_rawdev_pmd_release(rawdev);

	return 0;
}

static struct rte_vdev_driver cnxk_gpio_drv = {
	.probe = cnxk_gpio_probe,
	.remove = cnxk_gpio_remove,
};

RTE_PMD_REGISTER_VDEV(cnxk_gpio, cnxk_gpio_drv);
RTE_PMD_REGISTER_PARAM_STRING(cnxk_gpio,
		"gpiochip=<int> "
		"allowlist=<list>");

#endif /* CNXK_GPIO_V2_PRESENT */
