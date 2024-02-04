#ifndef SCMI_HAILO_H
#define SCMI_HAILO_H

#include <linux/scmi_protocol.h>

int scmi_hailo_fs_init(struct scmi_device *sdev);
void scmi_hailo_fs_fini(struct scmi_device *sdev);

#endif