#ifndef __HAILO15_ISP_V4L_H
#define __HAILO15_ISP_V4L_H

#include "hailo15-isp.h"

struct list_head *hailo15_isp_get_empty_queue(struct hailo15_isp_device *, int);
struct list_head *hailo15_isp_get_full_queue(struct hailo15_isp_device *, int);
struct mutex *hailo15_isp_get_empty_lock(struct hailo15_isp_device *, int);
struct mutex *hailo15_isp_get_full_lock(struct hailo15_isp_device *, int);

#endif /*__HAILO15_ISP_V4L_H*/
