/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <dirent.h>
#include <string.h>

#include <rte_bus_vdev.h>
#include <rte_eal.h>
#include <rte_kvargs.h>
#include <rte_lcore.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_gpio.h"

#define CNXK_GPIO_BUFSZ 128
#define CNXK_GPIO_CLASS_PATH "/sys/class/gpio"

static const char *const cnxk_gpio_args[] = {
#define CNXK_GPIO_ARG_GPIOCHIP "gpiochip"
	CNXK_GPIO_ARG_GPIOCHIP,
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

static void
cnxk_gpio_set_defaults(struct cnxk_gpiochip *gpiochip)
{
	struct dirent **namelist;
	int n;

	n = scandir(CNXK_GPIO_CLASS_PATH, &namelist, cnxk_gpio_filter_gpiochip,
		    alphasort);
	if (n < 0 || n == 0)
		return;

	sscanf(namelist[0]->d_name, "gpiochip%d", &gpiochip->num);
	while (n--)
		free(namelist[n]);
	free(namelist);
}

static int
cnxk_gpio_parse_arg_gpiochip(const char *key __rte_unused, const char *value,
			     void *extra_args)
{
	long val;

	errno = 0;
	val = strtol(value, NULL, 10);
	if (errno)
		return -errno;

	*(int *)extra_args = (int)val;

	return 0;
}

static int
cnxk_gpio_parse_args(struct cnxk_gpiochip *gpiochip,
		     struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	int ret;

	kvlist = rte_kvargs_parse(devargs->args, cnxk_gpio_args);
	if (!kvlist)
		return 0;

	ret = rte_kvargs_count(kvlist, CNXK_GPIO_ARG_GPIOCHIP);
	if (ret == 1) {
		ret = rte_kvargs_process(kvlist, CNXK_GPIO_ARG_GPIOCHIP,
					 cnxk_gpio_parse_arg_gpiochip,
					 &gpiochip->num);
		if (ret)
			goto out;
	}

	ret = 0;
out:
	rte_kvargs_free(kvlist);

	return ret;
}

static int
cnxk_gpio_read_attr(char *attr, char *val)
{
	FILE *fp;
	int ret;

	fp = fopen(attr, "r");
	if (!fp)
		return -errno;

	ret = fscanf(fp, "%s", val);
	if (ret < 0)
		return -errno;
	if (ret != 1)
		return -EIO;

	ret = fclose(fp);
	if (ret)
		return -errno;

	return 0;
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

static struct cnxk_gpio *
cnxk_gpio_lookup(struct cnxk_gpiochip *gpiochip, uint16_t queue)
{
	if (queue >= gpiochip->num_gpios)
		return NULL;

	return gpiochip->gpios[queue];
}

static int
cnxk_gpio_queue_setup(struct rte_rawdev *dev, uint16_t queue_id,
		      rte_rawdev_obj_t queue_conf, size_t queue_conf_size)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	char buf[CNXK_GPIO_BUFSZ];
	struct cnxk_gpio *gpio;
	int ret;

	RTE_SET_USED(queue_conf);
	RTE_SET_USED(queue_conf_size);

	gpio = cnxk_gpio_lookup(gpiochip, queue_id);
	if (gpio)
		return -EEXIST;

	gpio = rte_zmalloc(NULL, sizeof(*gpio), 0);
	if (!gpio)
		return -ENOMEM;
	gpio->num = queue_id + gpiochip->base;
	gpio->gpiochip = gpiochip;

	snprintf(buf, sizeof(buf), "%s/export", CNXK_GPIO_CLASS_PATH);
	ret = cnxk_gpio_write_attr_int(buf, gpio->num);
	if (ret) {
		rte_free(gpio);
		return ret;
	}

	gpiochip->gpios[queue_id] = gpio;

	return 0;
}

static int
cnxk_gpio_queue_release(struct rte_rawdev *dev, uint16_t queue_id)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;
	char buf[CNXK_GPIO_BUFSZ];
	struct cnxk_gpio *gpio;
	int ret;

	gpio = cnxk_gpio_lookup(gpiochip, queue_id);
	if (!gpio)
		return -ENODEV;

	snprintf(buf, sizeof(buf), "%s/unexport", CNXK_GPIO_CLASS_PATH);
	ret = cnxk_gpio_write_attr_int(buf, gpiochip->base + queue_id);
	if (ret)
		return ret;

	gpiochip->gpios[queue_id] = NULL;
	rte_free(gpio);

	return 0;
}

static int
cnxk_gpio_queue_def_conf(struct rte_rawdev *dev, uint16_t queue_id,
			 rte_rawdev_obj_t queue_conf, size_t queue_conf_size)
{
	unsigned int *conf;

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	if (queue_conf_size != sizeof(*conf))
		return -EINVAL;

	conf = (unsigned int *)queue_conf;
	*conf = 1;

	return 0;
}

static uint16_t
cnxk_gpio_queue_count(struct rte_rawdev *dev)
{
	struct cnxk_gpiochip *gpiochip = dev->dev_private;

	return gpiochip->num_gpios;
}

static int
cnxk_gpio_dev_close(struct rte_rawdev *dev)
{
	RTE_SET_USED(dev);

	return 0;
}

static const struct rte_rawdev_ops cnxk_gpio_rawdev_ops = {
	.dev_close = cnxk_gpio_dev_close,
	.queue_def_conf = cnxk_gpio_queue_def_conf,
	.queue_count = cnxk_gpio_queue_count,
	.queue_setup = cnxk_gpio_queue_setup,
	.queue_release = cnxk_gpio_queue_release,
};

static int
cnxk_gpio_probe(struct rte_vdev_device *dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct cnxk_gpiochip *gpiochip;
	struct rte_rawdev *rawdev;
	char buf[CNXK_GPIO_BUFSZ];
	int ret;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	cnxk_gpio_format_name(name, sizeof(name));
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(*gpiochip),
					 rte_socket_id());
	if (!rawdev) {
		RTE_LOG(ERR, PMD, "failed to allocate %s rawdev", name);
		return -ENOMEM;
	}

	rawdev->dev_ops = &cnxk_gpio_rawdev_ops;
	rawdev->device = &dev->device;
	rawdev->driver_name = dev->device.name;

	gpiochip = rawdev->dev_private;
	cnxk_gpio_set_defaults(gpiochip);

	/* defaults may be overwritten by this call */
	ret = cnxk_gpio_parse_args(gpiochip, dev->device.devargs);
	if (ret)
		goto out;

	/* read gpio base */
	snprintf(buf, sizeof(buf), "%s/gpiochip%d/base", CNXK_GPIO_CLASS_PATH,
		 gpiochip->num);
	ret = cnxk_gpio_read_attr_int(buf, &gpiochip->base);
	if (ret) {
		RTE_LOG(ERR, PMD, "failed to read %s", buf);
		goto out;
	}

	/* read number of available gpios */
	snprintf(buf, sizeof(buf), "%s/gpiochip%d/ngpio", CNXK_GPIO_CLASS_PATH,
		 gpiochip->num);
	ret = cnxk_gpio_read_attr_int(buf, &gpiochip->num_gpios);
	if (ret) {
		RTE_LOG(ERR, PMD, "failed to read %s", buf);
		goto out;
	}

	gpiochip->gpios = rte_calloc(NULL, gpiochip->num_gpios,
				     sizeof(struct cnxk_gpio *), 0);
	if (!gpiochip->gpios) {
		RTE_LOG(ERR, PMD, "failed to allocate gpios memory");
		ret = -ENOMEM;
		goto out;
	}

	return 0;
out:
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

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

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

	rte_free(gpiochip->gpios);
	rte_rawdev_pmd_release(rawdev);

	return 0;
}

static struct rte_vdev_driver cnxk_gpio_drv = {
	.probe = cnxk_gpio_probe,
	.remove = cnxk_gpio_remove,
};

RTE_PMD_REGISTER_VDEV(cnxk_gpio, cnxk_gpio_drv);
RTE_PMD_REGISTER_PARAM_STRING(cnxk_gpio, "gpiochip=<int>");
