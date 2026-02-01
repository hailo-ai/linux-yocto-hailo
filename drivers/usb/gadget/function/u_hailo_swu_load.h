/*
 * u_hailo_swu_load.h -- Header file for Hailo SWU loading USB function
 *
 * This file contains structures and function declarations specific to the
 * Hailo SWU (Root File System) loading functionality.
 */

#ifndef __U_HAILO_SWU_LOAD_H
#define __U_HAILO_SWU_LOAD_H

#include <linux/usb/composite.h>

struct f_hailo_swu_load_opts {
	struct usb_function_instance	func_inst;
	
	/* function has been bounded */
	bool bound;
    /* SWU-specific configuration */
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

#endif /* __U_HAILO_SWU_LOAD_H */