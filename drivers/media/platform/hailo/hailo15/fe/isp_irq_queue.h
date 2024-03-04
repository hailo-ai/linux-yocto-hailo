 /****************************************************************************
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2023 Vivante Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *****************************************************************************
 *
 * The GPL License (GPL)
 *
 * Copyright (c) 2014-2023 Vivante Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program;
 *
 *****************************************************************************
 *
 * Note: This software is released under dual MIT and GPL licenses. A
 * recipient may use this file under the terms of either the MIT license or
 * GPL License. If you wish to use only one license not the other, you can
 * indicate your decision by deleting one of the above license notices in your
 * version of this file.
 *
 *****************************************************************************/
#ifndef _ISP_IRQ_QUEUE_H_
#define _ISP_IRQ_QUEUE_H_
#ifdef __KERNEL__
#include <linux/list.h>
#endif
//#include "isp_ioctl.h"
typedef enum isp_src_e {
	SRC_ISP_IRQ = 0X0000,
	SRC_JPE_STATUS_IRQ,
	SRC_JPE_ERROR_IRQ,
	SRC_MI_IRQ,
	SRC_MI1_IRQ,
	SRC_MI2_IRQ,
	SRC_MI3_IRQ,
	SRC_MIPI_IRQ,
	SRC_ISP_STITCHING_IRQ,
	SRC_MI_HDR1_IRQ,
	SRC_IRQ_MAX,
} isp_src_t;

typedef struct isp_mis_s {
	unsigned int irq_src;
	unsigned int val;
	unsigned int ispId;
#ifdef __KERNEL__
	struct list_head list;
#endif
} isp_mis_t;

typedef struct isp_mis_list_s {

	isp_mis_t *pHead;
	isp_mis_t *pRead;
	isp_mis_t *pWrite;
} isp_mis_list_t;

int isp_irq_enqueue(isp_mis_t *mis_new, isp_mis_t *head);
int isp_irq_dequeue(isp_mis_t *data, isp_mis_t *head);
bool isp_irq_is_queue_empty(isp_mis_t *head);

#define QUEUE_NODE_COUNT 48
int isp_irq_create_circle_queue(isp_mis_list_t *pCList, int number);
int isp_irq_destroy_circle_queue(isp_mis_list_t *pCList);

int isp_irq_read_circle_queue(isp_mis_t *data, isp_mis_list_t *pCList);
int isp_irq_write_circle_queue(isp_mis_t *data, isp_mis_list_t *pCList);

#endif
