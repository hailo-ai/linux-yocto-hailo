#ifndef SCMI_HAILO_H
#define SCMI_HAILO_H

#include <linux/scmi_protocol.h>

extern struct scmi_hailo_proto_ops *hailo_ops;
extern struct scmi_notify_ops *hailo_notify_ops;
extern struct scmi_protocol_handle *ph;

int scmi_hailo_ops_init(struct scmi_device *sdev);

#endif