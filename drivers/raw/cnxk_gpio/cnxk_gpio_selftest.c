/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_service.h>

#include "cnxk_gpio.h"
#include "rte_pmd_cnxk_gpio.h"

#define CNXK_GPIO_BUFSZ 128

#define OTX_IOC_MAGIC 0xF2
#define OTX_IOC_TRIGGER_GPIO_HANDLER                                           \
	_IO(OTX_IOC_MAGIC, 3)

static int fd;

static int
cnxk_gpio_attr_exists(const char *attr)
{
	struct stat st;

	return !stat(attr, &st);
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

#define CNXK_GPIO_ERR_STR(err, str, ...) do {                                  \
	if (err) {                                                             \
		RTE_LOG(ERR, PMD, "%s:%d: " str " (%d)\n", __func__, __LINE__, \
			##__VA_ARGS__, err);                                   \
		goto out;                                                      \
	}                                                                      \
} while (0)

static int
cnxk_gpio_validate_attr(char *attr, const char *expected)
{
	char buf[CNXK_GPIO_BUFSZ];
	int ret;

	ret = cnxk_gpio_read_attr(attr, buf);
	if (ret)
		return ret;

	if (strncmp(buf, expected, sizeof(buf)))
		return -EIO;

	return 0;
}

#define CNXK_GPIO_PATH_FMT "/sys/class/gpio/gpio%d"

static int
cnxk_gpio_test_input(uint16_t dev_id, int base, int gpio)
{
	char buf[CNXK_GPIO_BUFSZ];
	int ret, n;

	n = snprintf(buf, sizeof(buf), CNXK_GPIO_PATH_FMT, base + gpio);
	snprintf(buf + n, sizeof(buf) - n, "/direction");

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_IN);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to input");
	ret = cnxk_gpio_validate_attr(buf, "in");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 1) |
	      rte_pmd_gpio_set_pin_value(dev_id, gpio, 0);
	if (!ret) {
		ret = -EIO;
		CNXK_GPIO_ERR_STR(ret, "input pin overwritten");
	}

	snprintf(buf + n, sizeof(buf) - n, "/edge");

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio,
					CNXK_GPIO_PIN_EDGE_FALLING);
	CNXK_GPIO_ERR_STR(ret, "failed to set edge to falling");
	ret = cnxk_gpio_validate_attr(buf, "falling");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio,
					CNXK_GPIO_PIN_EDGE_RISING);
	CNXK_GPIO_ERR_STR(ret, "failed to change edge to rising");
	ret = cnxk_gpio_validate_attr(buf, "rising");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_BOTH);
	CNXK_GPIO_ERR_STR(ret, "failed to change edge to both");
	ret = cnxk_gpio_validate_attr(buf, "both");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_NONE);
	CNXK_GPIO_ERR_STR(ret, "failed to set edge to none");
	ret = cnxk_gpio_validate_attr(buf, "none");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	/*
	 * calling this makes sure kernel driver switches off inverted
	 * logic
	 */
	rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_IN);

out:
	return ret;
}

static int
cnxk_gpio_trigger_irq(int gpio)
{
	int ret;

	ret = ioctl(fd, OTX_IOC_TRIGGER_GPIO_HANDLER, gpio);

	return ret == -1 ? -errno : 0;
}

static void
cnxk_gpio_irq_handler(int gpio, void *data)
{
	*(int *)data = gpio;
}

static int
cnxk_gpio_test_irq(uint16_t dev_id, int gpio)
{
	int irq_data, ret;

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_IN);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to input");

	irq_data = 0;
	ret = rte_pmd_gpio_register_irq(dev_id, gpio, rte_lcore_id(),
					cnxk_gpio_irq_handler, &irq_data);
	CNXK_GPIO_ERR_STR(ret, "failed to register irq handler");

	ret = rte_pmd_gpio_enable_interrupt(dev_id, gpio,
					    CNXK_GPIO_PIN_EDGE_RISING);
	CNXK_GPIO_ERR_STR(ret, "failed to enable interrupt");

	ret = cnxk_gpio_trigger_irq(gpio);
	CNXK_GPIO_ERR_STR(ret, "failed to trigger irq");
	rte_delay_ms(1);
	ret = *(volatile int *)&irq_data == gpio ? 0 : -EIO;
	CNXK_GPIO_ERR_STR(ret, "failed to test irq");

	ret = rte_pmd_gpio_disable_interrupt(dev_id, gpio);
	CNXK_GPIO_ERR_STR(ret, "failed to disable interrupt");

	ret = rte_pmd_gpio_unregister_irq(dev_id, gpio);
	CNXK_GPIO_ERR_STR(ret, "failed to unregister irq handler");
out:
	rte_pmd_gpio_disable_interrupt(dev_id, gpio);
	rte_pmd_gpio_unregister_irq(dev_id, gpio);

	return ret;
}

static int
cnxk_gpio_test_output(uint16_t dev_id, int base, int gpio)
{
	char buf[CNXK_GPIO_BUFSZ];
	int ret, val, n;

	n = snprintf(buf, sizeof(buf), CNXK_GPIO_PATH_FMT, base + gpio);

	snprintf(buf + n, sizeof(buf) - n, "/direction");
	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_OUT);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to out");
	ret = cnxk_gpio_validate_attr(buf, "out");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	snprintf(buf + n, sizeof(buf) - n, "/value");
	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 0);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 0");
	ret = cnxk_gpio_validate_attr(buf, "0");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 0", val);

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 1);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 1");
	ret = cnxk_gpio_validate_attr(buf, "1");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val != 1)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 1", val);

	snprintf(buf + n, sizeof(buf) - n, "/direction");
	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_LOW);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to low");
	ret = cnxk_gpio_validate_attr(buf, "out");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);
	snprintf(buf + n, sizeof(buf) - n, "/value");
	ret = cnxk_gpio_validate_attr(buf, "0");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	snprintf(buf + n, sizeof(buf) - n, "/direction");
	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_HIGH);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to high");
	ret = cnxk_gpio_validate_attr(buf, "out");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);
	snprintf(buf + n, sizeof(buf) - n, "/value");
	ret = cnxk_gpio_validate_attr(buf, "1");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	snprintf(buf + n, sizeof(buf) - n, "/edge");
	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio,
					CNXK_GPIO_PIN_EDGE_FALLING);
	ret = ret == 0 ? -EIO : 0;
	CNXK_GPIO_ERR_STR(ret, "changed edge to falling");
	ret = cnxk_gpio_validate_attr(buf, "none");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio,
					CNXK_GPIO_PIN_EDGE_RISING);
	ret = ret == 0 ? -EIO : 0;
	CNXK_GPIO_ERR_STR(ret, "changed edge to rising");
	ret = cnxk_gpio_validate_attr(buf, "none");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_BOTH);
	ret = ret == 0 ? -EIO : 0;
	CNXK_GPIO_ERR_STR(ret, "changed edge to both");
	ret = cnxk_gpio_validate_attr(buf, "none");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	/* this one should succeed */
	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_NONE);
	CNXK_GPIO_ERR_STR(ret, "failed to change edge to none");
	ret = cnxk_gpio_validate_attr(buf, "none");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	snprintf(buf + n, sizeof(buf) - n, "/active_low");
	ret = rte_pmd_gpio_set_pin_active_low(dev_id, gpio, 1);
	CNXK_GPIO_ERR_STR(ret, "failed to set active_low to 1");
	ret = cnxk_gpio_validate_attr(buf, "1");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_get_pin_active_low(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read active_low");
	if (val != 1)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 1", val);

	snprintf(buf + n, sizeof(buf) - n, "/value");
	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 1);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 1");
	ret = cnxk_gpio_validate_attr(buf, "1");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 0);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 0");
	ret = cnxk_gpio_validate_attr(buf, "0");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

	snprintf(buf + n, sizeof(buf) - n, "/active_low");
	ret = rte_pmd_gpio_set_pin_active_low(dev_id, gpio, 0);
	CNXK_GPIO_ERR_STR(ret, "failed to set active_low to 0");
	ret = cnxk_gpio_validate_attr(buf, "0");
	CNXK_GPIO_ERR_STR(ret, "failed to validate %s", buf);

out:
	return ret;
}

int
cnxk_gpio_selftest(uint16_t dev_id)
{
	struct cnxk_gpio_queue_conf conf;
	struct cnxk_gpiochip *gpiochip;
	char buf[CNXK_GPIO_BUFSZ];
	struct rte_rawdev *rawdev;
	unsigned int queues, i;
	struct cnxk_gpio *gpio;
	int ret, ret2;

	rawdev = rte_rawdev_pmd_get_named_dev("cnxk_gpio");
	if (!rawdev)
		return -ENODEV;
	gpiochip = rawdev->dev_private;

	queues = rte_rawdev_queue_count(dev_id);
	if (queues == 0)
		return -ENODEV;

	ret = rte_rawdev_start(dev_id);
	if (ret)
		return ret;

	fd = open("/dev/otx-gpio-ctr", O_RDWR | O_SYNC);
	if (fd < 0)
		return -errno;

	for (i = 0; i < queues; i++) {
		ret = rte_rawdev_queue_conf_get(dev_id, i, &conf, sizeof(conf));
		if (ret) {
			RTE_LOG(ERR, PMD,
				"failed to read queue configuration (%d)\n",
				ret);
			goto out;
		}

		RTE_LOG(INFO, PMD, "testing queue%d (gpio%d)\n", i, conf.gpio);

		if (conf.size != 1) {
			RTE_LOG(ERR, PMD, "wrong queue size received\n");
			ret = -EIO;
			goto out;
		}

		ret = rte_rawdev_queue_setup(dev_id, i, NULL, 0);
		if (ret) {
			RTE_LOG(ERR, PMD, "failed to setup queue (%d)\n", ret);
			goto out;
		}

		gpio = gpiochip->gpios[conf.gpio];
		snprintf(buf, sizeof(buf), CNXK_GPIO_PATH_FMT, gpio->num);
		if (!cnxk_gpio_attr_exists(buf)) {
			RTE_LOG(ERR, PMD, "%s does not exist\n", buf);
			ret = -ENOENT;
			goto release;
		}

		ret = cnxk_gpio_test_input(dev_id, gpiochip->base, conf.gpio);
		if (ret)
			goto release;

		ret = cnxk_gpio_test_irq(dev_id, conf.gpio);
		if (ret)
			goto release;

		ret = cnxk_gpio_test_output(dev_id, gpiochip->base, conf.gpio);
release:
		ret2 = ret;
		ret = rte_rawdev_queue_release(dev_id, i);
		if (ret) {
			RTE_LOG(ERR, PMD, "failed to release queue (%d)\n",
				ret);
			break;
		}

		if (cnxk_gpio_attr_exists(buf)) {
			RTE_LOG(ERR, PMD, "%s still exists\n", buf);
			ret = -EIO;
			break;
		}

		if (ret2) {
			ret = ret2;
			break;
		}
	}

out:
	close(fd);
	rte_rawdev_stop(dev_id);

	return ret;
}
