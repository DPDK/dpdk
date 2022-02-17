/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _RTE_PMD_CNXK_GPIO_H_
#define _RTE_PMD_CNXK_GPIO_H_

#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_rawdev.h>

/**
 * @file rte_pmd_cnxk_gpio.h
 *
 * Marvell GPIO PMD specific structures and interface
 *
 * This API allows applications to manage GPIOs in user space along with
 * installing interrupt handlers for low latency signal processing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Queue default configuration */
struct cnxk_gpio_queue_conf {
	/** Queue size */
	int size;
	/** GPIO number as seen by hardware */
	int gpio;
};

/** Available message types */
enum cnxk_gpio_msg_type {
	/** Type used to set output value */
	CNXK_GPIO_MSG_TYPE_SET_PIN_VALUE,
	/** Type used to set edge */
	CNXK_GPIO_MSG_TYPE_SET_PIN_EDGE,
	/** Type used to set direction */
	CNXK_GPIO_MSG_TYPE_SET_PIN_DIR,
	/** Type used to set inverted logic */
	CNXK_GPIO_MSG_TYPE_SET_PIN_ACTIVE_LOW,
	/** Type used to read value */
	CNXK_GPIO_MSG_TYPE_GET_PIN_VALUE,
	/** Type used to read edge */
	CNXK_GPIO_MSG_TYPE_GET_PIN_EDGE,
	/** Type used to read direction */
	CNXK_GPIO_MSG_TYPE_GET_PIN_DIR,
	/** Type used to read inverted logic state */
	CNXK_GPIO_MSG_TYPE_GET_PIN_ACTIVE_LOW,
	/** Type used to register interrupt handler */
	CNXK_GPIO_MSG_TYPE_REGISTER_IRQ,
	/** Type used to remove interrupt handler */
	CNXK_GPIO_MSG_TYPE_UNREGISTER_IRQ,
};

/** Available edges */
enum cnxk_gpio_pin_edge {
	/** Set edge to none */
	CNXK_GPIO_PIN_EDGE_NONE,
	/** Set edge to falling */
	CNXK_GPIO_PIN_EDGE_FALLING,
	/** Set edge to rising */
	CNXK_GPIO_PIN_EDGE_RISING,
	/** Set edge to both rising and falling */
	CNXK_GPIO_PIN_EDGE_BOTH,
};

/** Available directions */
enum cnxk_gpio_pin_dir {
	/** Set direction to input */
	CNXK_GPIO_PIN_DIR_IN,
	/** Set direction to output */
	CNXK_GPIO_PIN_DIR_OUT,
	/** Set direction to output and value to 1 */
	CNXK_GPIO_PIN_DIR_HIGH,
	/* Set direction to output and value to 0 */
	CNXK_GPIO_PIN_DIR_LOW,
};

/**
 * GPIO interrupt handler
 *
 * @param gpio
 *   Zero-based GPIO number
 * @param data
 *   Cookie passed to interrupt handler
 */
typedef void (*cnxk_gpio_irq_handler_t)(int gpio, void *data);

struct cnxk_gpio_irq {
	/** Interrupt handler */
	cnxk_gpio_irq_handler_t handler;
	/** User data passed to irq handler */
	void *data;
	/** CPU which will run irq handler */
	int cpu;
};

struct cnxk_gpio_msg {
	/** Message type */
	enum cnxk_gpio_msg_type type;
	/** Message data passed to PMD or received from PMD */
	void *data;
};

/** @internal helper routine for enqueuing/dequeuing messages */
static __rte_always_inline int
__rte_pmd_gpio_enq_deq(uint16_t dev_id, int gpio, void *req, void *rsp,
		       size_t rsp_size)
{
	struct rte_rawdev_buf *bufs[1];
	struct rte_rawdev_buf buf;
	void *q;
	int ret;

	q = (void *)(size_t)gpio;
	buf.buf_addr = req;
	bufs[0] = &buf;

	ret = rte_rawdev_enqueue_buffers(dev_id, bufs, RTE_DIM(bufs), q);
	if (ret < 0)
		return ret;
	if (ret != RTE_DIM(bufs))
		return -EIO;

	if (!rsp)
		return 0;

	ret = rte_rawdev_dequeue_buffers(dev_id, bufs, RTE_DIM(bufs), q);
	if (ret < 0)
		return ret;
	if (ret != RTE_DIM(bufs))
		return -EIO;

	rte_memcpy(rsp, buf.buf_addr, rsp_size);
	rte_free(buf.buf_addr);

	return 0;
}

/**
 * Set output to specific value
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param val
 *   Value output will be set to. 0 represents low state while
 *   1 high state
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_set_pin_value(uint16_t dev_id, int gpio, int val)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_SET_PIN_VALUE,
		.data = &val,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, NULL, 0);
}

/**
 * Select signal edge that triggers interrupt
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param edge
 *   Signal edge that triggers interrupt
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_set_pin_edge(uint16_t dev_id, int gpio,
			  enum cnxk_gpio_pin_edge edge)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_SET_PIN_EDGE,
		.data = &edge
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, NULL, 0);
}

/**
 * Configure GPIO as input or output
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param dir
 *   Direction of the GPIO
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_set_pin_dir(uint16_t dev_id, int gpio, enum cnxk_gpio_pin_dir dir)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_SET_PIN_DIR,
		.data = &dir,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, NULL, 0);
}

/**
 * Enable or disable inverted logic
 *
 * If GPIO is configured as output then writing 1 or 0 will result in setting
 * output to respectively low or high
 *
 * If GPIO is configured as input then logic inversion applies to edges. Both
 * current and future settings are affected
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param val
 *   0 to disable, 1 to enable inverted logic
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_set_pin_active_low(uint16_t dev_id, int gpio, int val)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_SET_PIN_ACTIVE_LOW,
		.data = &val,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, NULL, 0);
}

/**
 * Read GPIO value
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param val
 *   Where to store read logical signal value i.e 0 or 1
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_get_pin_value(uint16_t dev_id, int gpio, int *val)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_GET_PIN_VALUE,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, val, sizeof(*val));
}

/**
 * Read GPIO edge
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param edge
 *   Where to store edge
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_get_pin_edge(uint16_t dev_id, int gpio,
			  enum cnxk_gpio_pin_edge *edge)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_GET_PIN_EDGE,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, edge, sizeof(*edge));
}

/**
 * Read GPIO direction
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param dir
 *   Where to store direction
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_get_pin_dir(uint16_t dev_id, int gpio, enum cnxk_gpio_pin_dir *dir)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_GET_PIN_DIR,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, dir, sizeof(*dir));
}

/**
 * Read whether GPIO is active low
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param val
 *   Where to store active low state
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_get_pin_active_low(uint16_t dev_id, int gpio, int *val)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_GET_PIN_ACTIVE_LOW,
		.data = &val,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, val, sizeof(*val));
}

/**
 * Attach interrupt handler to GPIO
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param cpu
 *   CPU which will be handling interrupt
 * @param handler
 *   Interrupt handler to be executed
 * @param data
 *   Data to be passed to interrupt handler
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_register_irq(uint16_t dev_id, int gpio, int cpu,
			  cnxk_gpio_irq_handler_t handler, void *data)
{
	struct cnxk_gpio_irq irq = {
		.handler = handler,
		.data = data,
		.cpu = cpu,
	};
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_REGISTER_IRQ,
		.data = &irq,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, NULL, 0);
}

/**
 * Detach interrupt handler from GPIO
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_unregister_irq(uint16_t dev_id, int gpio)
{
	struct cnxk_gpio_msg msg = {
		.type = CNXK_GPIO_MSG_TYPE_UNREGISTER_IRQ,
		.data = &gpio,
	};

	return __rte_pmd_gpio_enq_deq(dev_id, gpio, &msg, NULL, 0);
}

/**
 * Enable interrupt
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 * @param edge
 *   Edge that should trigger interrupt
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_enable_interrupt(uint16_t dev_id, int gpio,
			      enum cnxk_gpio_pin_edge edge)
{
	return rte_pmd_gpio_set_pin_edge(dev_id, gpio, edge);
}

/**
 * Disable interrupt
 *
 * @param dev_id
 *   The identifier of the device
 * @param gpio
 *   Zero-based GPIO number
 *
 * @return
 *   Returns 0 on success, negative error code otherwise
 */
static __rte_always_inline int
rte_pmd_gpio_disable_interrupt(uint16_t dev_id, int gpio)
{
	return rte_pmd_gpio_set_pin_edge(dev_id, gpio, CNXK_GPIO_PIN_EDGE_NONE);
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_CNXK_GPIO_H_ */
