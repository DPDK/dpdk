/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright 2008-2012 Freescale Semiconductor Inc.
 * Copyright 2017-2020 NXP
 */

#ifndef __NCSW_EXT_H
#define __NCSW_EXT_H

#include <stdint.h>

#define PTR_TO_UINT(_ptr)	((uintptr_t)(_ptr))
#define UINT_TO_PTR(_val)	((void *)(uintptr_t)(_val))

/* phys_address_t should be uintptr_t */
typedef uint64_t phys_address_t;

/*
 * @Description   Possible RxStore callback responses.
 */
typedef enum e_rx_store_response {
	e_RX_STORE_RESPONSE_PAUSE
		/**< Pause invoking callback with received data; in polling
		 * mode, start again invoking callback only next time user
		 * invokes the receive routine; in interrupt mode, start again
		 * invoking callback only next time a receive event triggers an
		 * interrupt; in all cases, received data that are pending are
		 * not lost, rather, their processing is temporarily deferred;
		 * in all cases, received data are processed in the order in
		 * which they were received.
		 */
	, e_RX_STORE_RESPONSE_CONTINUE
		/**< Continue invoking callback with received data. */
} e_rx_store_response;


/*
 * @Description   General Handle
 */
typedef void *t_handle;   /**< handle, used as object's descriptor */

/* @} */

/*
 * @Function	  t_get_buf_function
 *
 * @Description   User callback function called by driver to get data buffer.
 *
 *		  User provides this function. Driver invokes it.
 *
 * @Param[in]	  h_buffer_pool		A handle to buffer pool manager
 * @Param[out]	  p_buf_context_handle	Returns the user's private context that
 *					should be associated with the buffer
 *
 * @Return	  Pointer to data buffer, NULL if error
 */
typedef uint8_t * (t_get_buf_function)(t_handle   h_buffer_pool,
					t_handle *p_buf_context_handle);

/*
 * @Function	  t_put_buf_function
 *
 * @Description   User callback function called by driver to return data buffer.
 *		  User provides this function. Driver invokes it.
 *
 * @Param[in]	  h_buffer_pool		A handle to buffer pool manager
 * @Param[in]	  p_buffer		A pointer to buffer to return
 * @Param[in]	  h_buf_context		The user's private context associated
 *					with the returned buffer
 *
 * @Return	  E_OK on success; Error code otherwise
 */
typedef uint32_t (t_put_buf_function)(t_handle h_buffer_pool,
				uint8_t  *p_buffer,
				t_handle h_buf_context);

/*
 * @Function	  t_phys_to_virt
 *
 * @Description   Translates a physical address to the matching virtual address.
 *
 * @Param[in]	  addr		The physical address to translate.
 *
 * @Return	  Virtual address.
 */
typedef void *t_phys_to_virt(phys_address_t addr);

/*
 * @Function	  t_virt_to_phys
 *
 * @Description   Translates a virtual address to the matching physical address.
 *
 * @Param[in]	  addr		The virtual address to translate.
 *
 * @Return	  Physical address.
 */
typedef phys_address_t t_virt_to_phys(void *addr);

/*
 * @Description   Buffer Pool Information Structure.
 */
typedef struct t_buffer_pool_info {
	t_handle		h_buffer_pool;
		/**< A handle to the buffer pool mgr */
	t_get_buf_function	*f_get_buf;
		/**< User callback to get a free buffer */
	t_put_buf_function	*f_put_buf;
		/**< User callback to return a buffer */
	uint16_t		buffer_size;
		/**< Buffer size (in bytes) */
	t_phys_to_virt	*f_phys_to_virt;
		/**< User callback to translate pool buffers physical addresses
		 * to virtual addresses
		 */
	t_virt_to_phys	*f_virt_to_phys;
		/**< User callback to translate pool buffers virtual addresses
		 * to physical addresses
		 */
} t_buffer_pool_info;

/*
 * @Description   User callback function called by driver with receive data.
 *		  User provides this function. Driver invokes it.
 *
 * @Param[in]	  h_app		Application's handle, as was provided to the
 *				driver by the user
 * @Param[in]	  queue_id	Receive queue ID
 * @Param[in]	  p_data	Pointer to the buffer with received data
 * @Param[in]	  h_buf_context	The user's private context associated with the
 *				given data buffer
 * @Param[in]	  length	Length of received data
 * @Param[in]	  status	Receive status and errors
 * @Param[in]	  position	Position of buffer in frame
 * @Param[in]	  flags		Driver-dependent information
 *
 * @Retval	  e_RX_STORE_RESPONSE_CONTINUE	order the driver to continue Rx
 *						operation for all ready data.
 * @Retval	  e_RX_STORE_RESPONSE_PAUSE	order the driver to stop Rx ops.
 */
typedef e_rx_store_response(t_rx_store_function)(t_handle  h_app,
						uint32_t  queue_id,
						uint8_t   *p_data,
						t_handle  h_buf_context,
						uint32_t  length,
						uint16_t  status,
						uint8_t   position,
						uint32_t  flags);

typedef struct t_device {
	uintptr_t   id;	/**< the device id */
	int	fd;	/**< the device file descriptor */
	t_handle	h_user_priv;
	uint32_t	owners;
} t_device;

t_handle create_device(t_handle h_user_priv, t_handle h_dev_id);
t_handle get_device_id(t_handle h_dev);

#endif /* __NCSW_EXT_H */
