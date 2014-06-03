/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_ALARM_H_
#define _RTE_ALARM_H_

/**
 * @file
 *
 * Alarm functions
 *
 * Simple alarm-clock functionality supplied by eal.
 * Does not require hpet support.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Signature of callback back function called when an alarm goes off.
 */
typedef void (*rte_eal_alarm_callback)(void *arg);

/**
 * Function to set a callback to be triggered when us microseconds
 * have expired. Accuracy of timing to the microsecond is not guaranteed. The
 * alarm function will not be called *before* the requested time, but may
 * be called a short period of time afterwards.
 * The alarm handler will be called only once. There is no need to call
 * "rte_eal_alarm_cancel" from within the callback function.
 *
 * @param us
 *   The time in microseconds before the callback is called
 * @param cb
 *   The function to be called when the alarm expires
 * @param cb_arg
 *   Pointer parameter to be passed to the callback function
 *
 * @return
 *   On success, zero.
 *   On failure, a negative error number
 */
int rte_eal_alarm_set(uint64_t us, rte_eal_alarm_callback cb, void *cb_arg);

/**
 * Function to cancel an alarm callback which has been registered before.
 *
 * @param cb_fn
 *  alarm callback
 * @param cb_arg
 *  Pointer parameter to be passed to the callback function. To remove all
 *  copies of a given callback function, irrespective of parameter, (void *)-1
 *  can be used here.
 *
 * @return
 *  - The number of callbacks removed
 */
int rte_eal_alarm_cancel(rte_eal_alarm_callback cb_fn, void *cb_arg);

#ifdef __cplusplus
}
#endif


#endif /* _RTE_ALARM_H_ */
