/*
 * u_hailo_rfs_load.h -- Header file for Hailo RFS loading USB function
 *
 * This file contains structures and function declarations specific to the
 * Hailo RFS (Root File System) loading functionality.
 */

#ifndef __U_HAILO_RFS_LOAD_H
#define __U_HAILO_RFS_LOAD_H

#include <linux/usb/composite.h>

struct f_hailo_rfs_load_opts {
	struct usb_function_instance	func_inst;
	
	/* function has been bounded */
	bool bound;
    /* RFS-specific configuration */
	unsigned status_interval_ms;	/* Status update interval */

	/*
	 * Read/write access to configfs attributes is handled by configfs.
	 *
	 * This is to protect the data from concurrent access by read/write
	 * and create symlink/remove symlink.
	 */
	struct mutex lock;
	int	refcnt;
};

#endif /* __U_HAILO_RFS_LOAD_H */