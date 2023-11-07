/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

#include <bus_vdev_driver.h>
#include <rte_eal.h>
#include <rte_kvargs.h>
#include <rte_lcore.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_gpio.h"
#include "rte_pmd_cnxk_gpio.h"

#define CNXK_GPIO_BUFSZ 128
#define CNXK_GPIO_CLASS_PATH "/sys/class/gpio"
#define CNXK_GPIO_PARAMS_MZ_NAME "cnxk_gpio_params_mz"

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

	n = scandir(CNXK_GPIO_CLASS_PATH, &namelist, cnxk_gpio_filter_gpiochip,
		    alphasort);
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
	size_t len = sizeof(**params);
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
	if (ret < 0)
		goto out;

	if (allowlist)
		len += strlen(allowlist) + 1;

	*params = cnxk_gpio_params_reserve(len);
	if (!(*params)) {
		ret = -ENOMEM;
		goto out;
	}

	strlcpy((*params)->allowlist, allowlist, strlen(allowlist) + 1);

	ret = cnxk_gpio_parse_arg(kvlist, CNXK_GPIO_ARG_GPIOCHIP, cnxk_gpio_parse_arg_gpiochip,
				  &(*params)->num);
	if (ret == 0)
		ret = cnxk_gpio_set_defaults(*params);

out:
	rte_kvargs_free(kvlist);

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
			RTE_LOG(ERR, PMD, "failed to parse %s\n", token);
			ret = -errno;
			goto out;
		}

		if (val < 0 || val >= gpiochip->num_gpios) {
			RTE_LOG(ERR, PMD, "gpio%d out of 0-%d range\n", val,
				gpiochip->num_gpios - 1);
			ret = -EINVAL;
			goto out;
		}

		for (i = 0; i < queue; i++) {
			if (list[i] != val)
				continue;

			RTE_LOG(WARNING, PMD, "gpio%d already allowed\n", val);
			break;
		}
		if (i == queue)
			list[queue++] = val;
	} while ((token = strtok(NULL, ",")));

	free(allowlist);
	gpiochip->allowlist = list;
	gpiochip->num_queues = queue;

	return 0;
out:
	free(allowlist);
	rte_free(list);

	return ret;
}

static int
cnxk_gpio_read_attr(char *attr, char *val)
{
	int ret, ret2;
	FILE *fp;

	fp = fopen(attr, "r");
	if (!fp)
		return -errno;

	ret = fscanf(fp, "%s", val);
	if (ret < 0) {
		ret = -errno;
		goto out;
	}
	if (ret != 1) {
		ret = -EIO;
		goto out;
	}

	ret = 0;
out:
	ret2 = fclose(fp);
	if (!ret)
		ret = ret2;

	return ret;
}

static int
cnxk_gpio_read_attr_int(char *attr, int *val)
{
	char buf[CNXK_GPIO_BUFSZ];
	int ret;

	ret = cnxk_gpio_read_attr(attr, buf);
	if (ret)
		return ret;

	ret = sscanf(buf, "%d", val);
	if (ret < 0)
		return -errno;

	return 0;
}

static int
cnxk_gpio_write_attr(const char *attr, const char *val)
{
	FILE *fp;
	int ret;

	if (!val)
		return -EINVAL;

	fp = fopen(attr, "w");
	if (!fp)
		return -errno;

	ret = fprintf(fp, "%s", val);
	if (ret < 0) {
		fclose(fp);
		return ret;
	}

	ret = fclose(fp);
	if (ret)
		return -errno;

	return 0;
}

static int
cnxk_gpio_write_attr_int(const char *attr, int val)
{
	char buf[CNXK_GPIO_BUFSZ];

	snprintf(buf, sizeof(buf), "%d", val);

	return cnxk_gpio_write_attr(attr, buf);
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
cnxk_gpio_exists(int num)
{
	char buf[CNXK_GPIO_BUFSZ];
	struct stat st;

	snprintf(buf, sizeof(buf), "%s/gpio%d", CNXK_GPIO_CLASS_PATH, num);

	return !stat(buf, &st);
}

static int
cnxk_gpio_queue_setup(struct rte_rawdev *dev, uint16_t queue_id,
		      rte_rawdev_obj_t queue_conf, size_t queue_conf_size)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	char buf[CNXK_GPIO_BUFSZ];
	struct cnxk_gpio *gpio;
	int num, ret;

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

	num = cnxk_queue_to_gpio(gpiochip, queue_id);
	gpio->num = num + gpiochip->base;
	gpio->gpiochip = gpiochip;

	if (!cnxk_gpio_exists(gpio->num)) {
		snprintf(buf, sizeof(buf), "%s/export", CNXK_GPIO_CLASS_PATH);
		ret = cnxk_gpio_write_attr_int(buf, gpio->num);
		if (ret) {
			rte_free(gpio);
			return ret;
		}
	} else {
		RTE_LOG(WARNING, PMD, "using existing gpio%d\n", gpio->num);
	}

	gpiochip->gpios[num] = gpio;

	return 0;
}

static int
cnxk_gpio_queue_release(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	char buf[CNXK_GPIO_BUFSZ];
	struct cnxk_gpio *gpio;
	int num, ret;

	if (!cnxk_gpio_queue_valid(gpiochip, queue_id))
		return -EINVAL;

	gpio = cnxk_gpio_lookup(gpiochip, queue_id);
	if (!gpio)
		return -ENODEV;

	snprintf(buf, sizeof(buf), "%s/unexport", CNXK_GPIO_CLASS_PATH);
	ret = cnxk_gpio_write_attr_int(buf, gpio->num);
	if (ret)
		return ret;

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
	const char *name;
} cnxk_gpio_edge_name[] = {
	{ CNXK_GPIO_PIN_EDGE_NONE, "none" },
	{ CNXK_GPIO_PIN_EDGE_FALLING, "falling" },
	{ CNXK_GPIO_PIN_EDGE_RISING, "rising" },
	{ CNXK_GPIO_PIN_EDGE_BOTH, "both" },
};

static const char *
cnxk_gpio_edge_to_name(enum cnxk_gpio_pin_edge edge)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_edge_name); i++) {
		if (cnxk_gpio_edge_name[i].edge == edge)
			return cnxk_gpio_edge_name[i].name;
	}

	return NULL;
}

static enum cnxk_gpio_pin_edge
cnxk_gpio_name_to_edge(const char *name)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_edge_name); i++) {
		if (!strcmp(cnxk_gpio_edge_name[i].name, name))
			break;
	}

	return cnxk_gpio_edge_name[i].edge;
}

static const struct {
	enum cnxk_gpio_pin_dir dir;
	const char *name;
} cnxk_gpio_dir_name[] = {
	{ CNXK_GPIO_PIN_DIR_IN, "in" },
	{ CNXK_GPIO_PIN_DIR_OUT, "out" },
	{ CNXK_GPIO_PIN_DIR_HIGH, "high" },
	{ CNXK_GPIO_PIN_DIR_LOW, "low" },
};

static const char *
cnxk_gpio_dir_to_name(enum cnxk_gpio_pin_dir dir)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_dir_name); i++) {
		if (cnxk_gpio_dir_name[i].dir == dir)
			return cnxk_gpio_dir_name[i].name;
	}

	return NULL;
}

static enum cnxk_gpio_pin_dir
cnxk_gpio_name_to_dir(const char *name)
{
	unsigned int i;

	for (i = 0; i < RTE_DIM(cnxk_gpio_dir_name); i++) {
		if (!strcmp(cnxk_gpio_dir_name[i].name, name))
			break;
	}

	return cnxk_gpio_dir_name[i].dir;
}

static int
cnxk_gpio_register_irq(struct cnxk_gpio *gpio, struct cnxk_gpio_irq *irq)
{
	int ret;

	ret = cnxk_gpio_irq_request(gpio->num - gpio->gpiochip->base, irq->cpu);
	if (ret)
		return ret;

	gpio->handler = irq->handler;
	gpio->data = irq->data;
	gpio->cpu = irq->cpu;

	return 0;
}

static int
cnxk_gpio_unregister_irq(struct cnxk_gpio *gpio)
{
	return cnxk_gpio_irq_free(gpio->num - gpio->gpiochip->base);
}

static int
cnxk_gpio_process_buf(struct cnxk_gpio *gpio, struct rte_rawdev_buf *rbuf)
{
	struct cnxk_gpio_msg *msg = rbuf->buf_addr;
	enum cnxk_gpio_pin_edge edge;
	enum cnxk_gpio_pin_dir dir;
	char buf[CNXK_GPIO_BUFSZ];
	void *rsp = NULL;
	int ret, val, n;

	n = snprintf(buf, sizeof(buf), "%s/gpio%d", CNXK_GPIO_CLASS_PATH,
		     gpio->num);

	switch (msg->type) {
	case CNXK_GPIO_MSG_TYPE_SET_PIN_VALUE:
		snprintf(buf + n, sizeof(buf) - n, "/value");
		ret = cnxk_gpio_write_attr_int(buf, !!*(int *)msg->data);
		break;
	case CNXK_GPIO_MSG_TYPE_SET_PIN_EDGE:
		snprintf(buf + n, sizeof(buf) - n, "/edge");
		edge = *(enum cnxk_gpio_pin_edge *)msg->data;
		ret = cnxk_gpio_write_attr(buf, cnxk_gpio_edge_to_name(edge));
		break;
	case CNXK_GPIO_MSG_TYPE_SET_PIN_DIR:
		snprintf(buf + n, sizeof(buf) - n, "/direction");
		dir = *(enum cnxk_gpio_pin_dir *)msg->data;
		ret = cnxk_gpio_write_attr(buf, cnxk_gpio_dir_to_name(dir));
		break;
	case CNXK_GPIO_MSG_TYPE_SET_PIN_ACTIVE_LOW:
		snprintf(buf + n, sizeof(buf) - n, "/active_low");
		val = *(int *)msg->data;
		ret = cnxk_gpio_write_attr_int(buf, val);
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_VALUE:
		snprintf(buf + n, sizeof(buf) - n, "/value");
		ret = cnxk_gpio_read_attr_int(buf, &val);
		if (ret)
			break;

		rsp = rte_zmalloc(NULL, sizeof(int), 0);
		if (!rsp)
			return -ENOMEM;

		*(int *)rsp = val;
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_EDGE:
		snprintf(buf + n, sizeof(buf) - n, "/edge");
		ret = cnxk_gpio_read_attr(buf, buf);
		if (ret)
			break;

		rsp = rte_zmalloc(NULL, sizeof(enum cnxk_gpio_pin_edge), 0);
		if (!rsp)
			return -ENOMEM;

		*(enum cnxk_gpio_pin_edge *)rsp = cnxk_gpio_name_to_edge(buf);
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_DIR:
		snprintf(buf + n, sizeof(buf) - n, "/direction");
		ret = cnxk_gpio_read_attr(buf, buf);
		if (ret)
			break;

		rsp = rte_zmalloc(NULL, sizeof(enum cnxk_gpio_pin_dir), 0);
		if (!rsp)
			return -ENOMEM;

		*(enum cnxk_gpio_pin_dir *)rsp = cnxk_gpio_name_to_dir(buf);
		break;
	case CNXK_GPIO_MSG_TYPE_GET_PIN_ACTIVE_LOW:
		snprintf(buf + n, sizeof(buf) - n, "/active_low");
		ret = cnxk_gpio_read_attr_int(buf, &val);
		if (ret)
			break;

		rsp = rte_zmalloc(NULL, sizeof(int), 0);
		if (!rsp)
			return -ENOMEM;

		*(int *)rsp = val;
		break;
	case CNXK_GPIO_MSG_TYPE_REGISTER_IRQ:
		ret = cnxk_gpio_register_irq(gpio, (struct cnxk_gpio_irq *)msg->data);
		break;
	case CNXK_GPIO_MSG_TYPE_UNREGISTER_IRQ:
		ret = cnxk_gpio_unregister_irq(gpio);
		break;
	default:
		return -EINVAL;
	}

	/* get rid of last response if any */
	if (gpio->rsp) {
		RTE_LOG(WARNING, PMD, "previous response got overwritten\n");
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
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct cnxk_gpio_params *params;
	struct cnxk_gpiochip *gpiochip;
	struct rte_rawdev *rawdev;
	char buf[CNXK_GPIO_BUFSZ];
	int ret;

	cnxk_gpio_format_name(name, sizeof(name));
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(*gpiochip), rte_socket_id());
	if (!rawdev) {
		RTE_LOG(ERR, PMD, "failed to allocate %s rawdev\n", name);
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

	ret = cnxk_gpio_irq_init(gpiochip);
	if (ret)
		goto out;

	/* read gpio base */
	snprintf(buf, sizeof(buf), "%s/gpiochip%d/base", CNXK_GPIO_CLASS_PATH, gpiochip->num);
	ret = cnxk_gpio_read_attr_int(buf, &gpiochip->base);
	if (ret) {
		RTE_LOG(ERR, PMD, "failed to read %s\n", buf);
		goto out;
	}

	/* read number of available gpios */
	snprintf(buf, sizeof(buf), "%s/gpiochip%d/ngpio", CNXK_GPIO_CLASS_PATH, gpiochip->num);
	ret = cnxk_gpio_read_attr_int(buf, &gpiochip->num_gpios);
	if (ret) {
		RTE_LOG(ERR, PMD, "failed to read %s\n", buf);
		goto out;
	}
	gpiochip->num_queues = gpiochip->num_gpios;

	ret = cnxk_gpio_parse_allowlist(gpiochip, params->allowlist);
	if (ret) {
		RTE_LOG(ERR, PMD, "failed to parse allowed gpios\n");
		goto out;
	}

	gpiochip->gpios = rte_calloc(NULL, gpiochip->num_gpios, sizeof(struct cnxk_gpio *), 0);
	if (!gpiochip->gpios) {
		RTE_LOG(ERR, PMD, "failed to allocate gpios memory\n");
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

		if (gpio->handler)
			cnxk_gpio_unregister_irq(gpio);

		cnxk_gpio_queue_release(rawdev, gpio->num);
	}

	rte_free(gpiochip->allowlist);
	rte_free(gpiochip->gpios);
	cnxk_gpio_irq_fini();
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
