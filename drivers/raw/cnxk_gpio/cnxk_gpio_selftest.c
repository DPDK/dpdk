/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <linux/gpio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <rte_cycles.h>
#include <rte_io.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "cnxk_gpio.h"
#include "rte_pmd_cnxk_gpio.h"

#define CNXK_GPIO_ERR_STR(err, str, ...) do {                                                      \
	if (err) {                                                                                 \
		CNXK_GPIO_LOG(ERR, "%s:%d: " str " (%d)", __func__, __LINE__, ##__VA_ARGS__, err); \
		goto out;                                                                          \
	}                                                                                          \
} while (0)

static int
cnxk_gpio_test_input(uint16_t dev_id, int gpio)
{
	int ret;

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_IN);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to input");

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 1) |
	      rte_pmd_gpio_set_pin_value(dev_id, gpio, 0);
	if (!ret) {
		ret = -EIO;
		CNXK_GPIO_ERR_STR(ret, "input pin overwritten");
	}

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_FALLING);
	CNXK_GPIO_ERR_STR(ret, "failed to set edge to falling");

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_RISING);
	CNXK_GPIO_ERR_STR(ret, "failed to change edge to rising");

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_BOTH);
	CNXK_GPIO_ERR_STR(ret, "failed to change edge to both");

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_NONE);
	CNXK_GPIO_ERR_STR(ret, "failed to set edge to none");

	/*
	 * calling this makes sure kernel driver switches off inverted
	 * logic
	 */
	rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_IN);

out:
	return ret;
}

static uint32_t triggered;

static void
cnxk_gpio_irq_handler(struct gpio_v2_line_event *event, void *data)
{
	RTE_SET_USED(event);
	RTE_SET_USED(data);

#ifdef CNXK_GPIO_V2_PRESENT
	int gpio = (int)(size_t)data;

	if ((int)event->offset != gpio)
		CNXK_GPIO_LOG(ERR, "event from gpio%d instead of gpio%d", event->offset, gpio);
#endif

	rte_write32(1, &triggered);
}

static int
cnxk_gpio_test_irq(uint16_t dev_id, int gpio)
{
	int ret;

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_IN);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to input");

	ret = rte_pmd_gpio_register_irq2(dev_id, gpio, cnxk_gpio_irq_handler, (int *)(size_t)gpio);
	CNXK_GPIO_ERR_STR(ret, "failed to register irq handler");

	ret = rte_pmd_gpio_enable_interrupt(dev_id, gpio, CNXK_GPIO_PIN_EDGE_RISING);
	CNXK_GPIO_ERR_STR(ret, "failed to enable interrupt");

	rte_delay_ms(2);
	rte_read32(&triggered);
	CNXK_GPIO_ERR_STR(!triggered, "failed to trigger irq");

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
cnxk_gpio_test_output(uint16_t dev_id, int gpio)
{
	int ret, val;

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_OUT);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to out");

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 0);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 0");
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 0", val);

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 1);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 1");
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val != 1)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 1", val);

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_LOW);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to low");

	ret = rte_pmd_gpio_set_pin_dir(dev_id, gpio, CNXK_GPIO_PIN_DIR_HIGH);
	CNXK_GPIO_ERR_STR(ret, "failed to set dir to high");
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val != 1)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 1", val);

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_FALLING);
	ret = ret == 0 ? -EIO : 0;
	CNXK_GPIO_ERR_STR(ret, "changed edge to falling");

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_RISING);
	ret = ret == 0 ? -EIO : 0;
	CNXK_GPIO_ERR_STR(ret, "changed edge to rising");

	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_BOTH);
	ret = ret == 0 ? -EIO : 0;
	CNXK_GPIO_ERR_STR(ret, "changed edge to both");

	/* this one should succeed */
	ret = rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_NONE);
	CNXK_GPIO_ERR_STR(ret, "failed to change edge to none");

	ret = rte_pmd_gpio_set_pin_active_low(dev_id, gpio, 1);
	CNXK_GPIO_ERR_STR(ret, "failed to set active_low to 1");

	ret = rte_pmd_gpio_get_pin_active_low(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read active_low");
	if (val != 1)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 1", val);

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 1);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 1");
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val != 1)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 1", val);

	ret = rte_pmd_gpio_set_pin_value(dev_id, gpio, 0);
	CNXK_GPIO_ERR_STR(ret, "failed to set value to 0");
	ret = rte_pmd_gpio_get_pin_value(dev_id, gpio, &val);
	CNXK_GPIO_ERR_STR(ret, "failed to read value");
	if (val != 0)
		ret = -EIO;
	CNXK_GPIO_ERR_STR(ret, "read %d instead of 0", val);

	ret = rte_pmd_gpio_set_pin_active_low(dev_id, gpio, 0);
	CNXK_GPIO_ERR_STR(ret, "failed to set active_low to 0");
out:
	return ret;
}

int
cnxk_gpio_selftest(uint16_t dev_id)
{
	struct cnxk_gpio_queue_conf conf;
	struct rte_rawdev *rawdev;
	unsigned int queues, i;
	int ret, ret2;

	rawdev = rte_rawdev_pmd_get_named_dev("cnxk_gpio");
	if (!rawdev)
		return -ENODEV;

	queues = rte_rawdev_queue_count(dev_id);
	if (queues == 0)
		return -ENODEV;

	ret = rte_rawdev_start(dev_id);
	if (ret)
		return ret;

	for (i = 0; i < queues; i++) {
		ret = rte_rawdev_queue_conf_get(dev_id, i, &conf, sizeof(conf));
		if (ret) {
			CNXK_GPIO_LOG(ERR, "failed to read queue configuration (%d)",
				ret);
			goto out;
		}

		CNXK_GPIO_LOG(INFO, "testing queue%d (gpio%d)", i, conf.gpio);

		if (conf.size != 1) {
			CNXK_GPIO_LOG(ERR, "wrong queue size received");
			ret = -EIO;
			goto out;
		}

		ret = rte_rawdev_queue_setup(dev_id, i, NULL, 0);
		if (ret) {
			CNXK_GPIO_LOG(ERR, "failed to setup queue (%d)", ret);
			goto out;
		}

		ret = cnxk_gpio_test_input(dev_id, conf.gpio);
		if (ret)
			goto release;

		ret = cnxk_gpio_test_irq(dev_id, conf.gpio);
		if (ret)
			goto release;

		ret = cnxk_gpio_test_output(dev_id, conf.gpio);
release:
		ret2 = ret;
		ret = rte_rawdev_queue_release(dev_id, i);
		if (ret) {
			CNXK_GPIO_LOG(ERR, "failed to release queue (%d)",
				ret);
			break;
		}

		if (ret2) {
			ret = ret2;
			break;
		}
	}

out:
	rte_rawdev_stop(dev_id);

	return ret;
}
