=======================================
Hailo RFS Upload USB Gadget Driver
=======================================

:Author: Hailo Technologies
:Date: November 2025
:Version: 1.0

Overview
========

The Hailo RFS Upload USB Gadget (``f_hailo_rfs_load.c``) is a specialized USB function driver designed for uploading RFS (Root File System) images to Hailo10 devices. This gadget enables efficient root filesystem deployment and updates through USB communication.

Architecture
============

USB Configuration
-----------------

============== =============================================
Parameter      Value
============== =============================================
Vendor ID      0x0B05 (ASUSTek Computer, Inc.)
Product ID     0x1D6F (Hailo Gadget)
Interface Class 0xFF (Vendor-specific)
Interface Protocol 1 (RFS load protocol)
Endpoints      Bulk OUT (data), Interrupt IN (status)
============== =============================================

Key Components
--------------

1. **Kernel Driver** (``f_hailo_rfs_load.c``)
   
   - USB gadget function implementation
   - RFS image buffering and processing
   - Direct file system deployment
   - Status reporting and control interface

2. **Userspace Tool** (``hailo_rfs_upload.c``)
   
   - libusb-based client application
   - RFS image upload functionality
   - Progress monitoring and status reporting
   - Error handling and retry logic

System Architecture Diagram
---------------------------

::

    Host System                    Target Device (Hailo10)
    ┌─────────────────┐           ┌─────────────────────────┐
    │ hailo_rfs_upload│           │ USB RFS Loading Function│
    │ C Upload Tool   │<--------->│ f_hailo_rfs_load.c      │
    │                 │   USB     │                         │
    │ RFS Image File  │           │ /initrd.image           │
    └─────────────────┘           └─────────────────────────┘
                                             │
                                             v
                                  ┌─────────────────────────┐
                                  │ Filesystem Deployment   │
                                  │ Direct Write to Target  │
                                  │                         │
                                  │ RFS Integration System  │
                                  └─────────────────────────┘

Protocol Specification
======================

Control Requests
----------------

The gadget supports several vendor-specific control requests:

Primary Control Requests
~~~~~~~~~~~~~~~~~~~~~~~~~

============ ================================= ==========================================
Request Code Name                              Description
============ ================================= ==========================================
0x03         HAILO_REQ__RFS_GET_STATUS         Get function status string
0x11         HAILO_REQ__RFS_GET_INFO           Get RFS model information
0x12         HAILO_REQ__RFS_LOAD               Load RFS image command
0x13         HAILO_REQ__RFS_FINISH             Finish RFS loading
0x15         HAILO_REQ__RFS_CTRL               RFS control operations
============ ================================= ==========================================

RFS Control Sub-commands (0x15)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

============ ======================= =========================================
Sub-command  Name                    Response
============ ======================= =========================================
0            GET_STATUS              Ready status (1 byte)
1            GET_RX_CNT              Bytes received count (4 bytes, LE)
2            CLR_RX_CNT              Reset RFS counter (1 byte response)
============ ======================= =========================================

Upload Protocol Flow
--------------------

1. **Initialization**::

    Host → Device: HAILO_REQ__RFS_LOAD (wValue=size_low, wIndex=size_high)
    Device → Host: ACK (prepares vmalloc buffer)

2. **Data Transfer**::

    Host → Device: Bulk OUT transfers (64KB chunks)
    Device: Buffers data in vmalloc memory

3. **Completion**::

    Host → Device: HAILO_REQ__RFS_FINISH
    Device: Writes buffered data to target filesystem
    Device: Completes RFS deployment

4. **Status Monitoring**::

    Host → Device: HAILO_REQ__RFS_CTRL (GET_STATUS)
    Device → Host: Ready Status (1 byte)

Status Strings
--------------

The ``HAILO_REQ__RFS_GET_STATUS`` request returns human-readable status strings:

=============== =======================================
Status String   Description
=============== =======================================
idle:ready      Device ready for operations
load:xxxxxx     RFS upload in progress, xxxxxx bytes received (hex)
invalid:xxxx    Invalid state or error
=============== =======================================

State Machine
-------------

The RFS upload state machine:

===== ============= =====================================
State Name          Description
===== ============= =====================================
IDLE  Device ready  Not processing
UPLOAD Uploading    RFS image data transfer in progress
===== ============= =====================================

Implementation Details
======================

Memory Management
-----------------

- **Buffer Allocation**: Uses ``vmalloc()`` for large RFS image buffering
- **Maximum Size**: 256MB per RFS image
- **Chunk Size**: 64KB bulk transfers for optimal performance
- **Buffer Lifecycle**: Allocated on LOAD request, freed after processing

Endianness Handling
-------------------

- **USB Protocol**: Little-endian byte order
- **Kernel to USB**: Uses ``cpu_to_le32()`` for outgoing data
- **USB to Host**: Uses ``le32toh()`` for incoming data
- **File Size Encoding**: Split across wValue (low 16 bits) and wIndex (high 16 bits)

Error Handling
--------------

**Timeout Values**:
  - Control requests: 5 seconds
  - Bulk transfers: 10 seconds

**Retry Logic**: Up to 3 retries for bulk transfers

**Errno Usage**: Appropriate errno values (``-ETIMEDOUT``, ``-EPERM``)

File System Integration
-----------------------

- **Output Target**: Direct filesystem deployment
- **Write Strategy**: Buffered write for integrity
- **Verification**: Checksum and size validation
- **Atomic Operations**: Ensures consistent state

Usage Examples
==============

Basic Upload
------------

.. code-block:: bash

   ./hailo_rfs_upload rootfs.img

Verbose Upload with Progress
----------------------------

.. code-block:: bash

   ./hailo_rfs_upload -v rootfs.img

Status Check Only
-----------------

.. code-block:: bash

   ./hailo_rfs_upload --status

Upload Without Waiting for Completion
--------------------------------------

.. code-block:: bash

   ./hailo_rfs_upload -n rootfs.img

Configuration
=============

Kernel Configuration
--------------------

.. code-block:: kconfig

   CONFIG_USB_CONFIGFS=y
   CONFIG_USB_GADGET=y
   CONFIG_USB_F_HAILO_RFS_LOAD=y

Device Tree
-----------

.. code-block:: dts

   &usb_gadget {
       hailo_rfs_load {
           compatible = "hailo,rfs-load";
           status = "okay";
       };
   };

udev Rules
----------

.. code-block:: udev

   # /etc/udev/rules.d/99-hailo-rfs.rules
   SUBSYSTEM=="usb", ATTR{idVendor}=="0b05", ATTR{idProduct}=="1d6f", MODE="0666"

Development and Debugging
=========================

Compilation
-----------

.. code-block:: bash

   # Userspace tool
   gcc -o hailo_rfs_upload hailo_rfs_upload.c -lusb-1.0

   # Or use build script
   ./build_hailo_rfs_upload.sh

Debugging
---------

.. code-block:: bash

   # Enable verbose output
   ./hailo_rfs_upload -v rootfs.img

   # Check kernel logs
   dmesg | grep hailo_rfs

   # Monitor USB traffic
   usbmon

Security Considerations
=======================

- **Permissions**: Requires root privileges or udev rules
- **Validation**: RFS image integrity checking
- **Size Limits**: Maximum 256MB to prevent DoS
- **Rate Limiting**: Built-in timeouts and retry limits
- **Filesystem Security**: Validates mount points and permissions

Performance
===========

=============== =====================================
Metric          Value
=============== =====================================
Transfer Speed  Up to 50MB/s (USB 3.0)
Memory Usage    ~256MB peak (during large uploads)
CPU Usage       Minimal during bulk transfers
Latency         <100ms control request response
=============== =====================================

Troubleshooting
===============

Common Issues
-------------

1. **Device Not Found**
   
   - Check USB connection
   - Verify VID/PID (0x0B05:0x1D6F)
   - Ensure gadget driver is loaded

2. **Permission Denied**
   
   - Run as root or configure udev rules
   - Check device file permissions

3. **Transfer Timeout**
   
   - Increase USB_TIMEOUT value
   - Check USB cable quality
   - Monitor system load

4. **Upload Verification Failed**
   
   - Check available memory
   - Verify RFS image integrity
   - Monitor kernel logs

Log Analysis
------------

.. code-block:: bash

   # Kernel logs
   journalctl -k | grep hailo_rfs

   # USB subsystem logs
   echo 'module usbcore +p' > /sys/kernel/debug/dynamic_debug/control

API Reference
=============

Control Request Structure
-------------------------

.. code-block:: c

   struct usb_ctrlrequest {
       __u8 bRequestType;  // USB_TYPE_VENDOR | USB_DIR_IN/OUT
       __u8 bRequest;      // HAILO_REQ__* constant
       __le16 wValue;      // Request-specific parameter
       __le16 wIndex;      // Request-specific parameter  
       __le16 wLength;     // Data length
   };

Status Response Format
----------------------

.. code-block:: c

   // GET_STATUS response
   struct rfs_status {
       __u8 ready;         // 1 if ready, 0 if not ready
   };

   // GET_RX_CNT response
   struct rfs_rx_count {
       __le32 bytes_received;  // Total bytes received
   };

File System Types
=================

Supported Formats
-----------------

- **ext4**: Linux extended filesystem
- **squashfs**: Compressed read-only filesystem
- **initramfs**: Initial RAM filesystem
- **tar.gz**: Compressed tarball (extracted on device)

Deployment Strategies
---------------------

- **Direct Write**: Raw image to block device
- **Extract and Copy**: Archive extraction to target directory
- **Overlay**: Layered filesystem approach
- **Atomic Switch**: Safe filesystem replacement

Integration Examples
====================

Buildroot Integration
---------------------

.. code-block:: makefile

   # buildroot/package/hailo-rfs-upload/Config.in
   config BR2_PACKAGE_HAILO_RFS_UPLOAD
       bool "hailo-rfs-upload"
       depends on BR2_PACKAGE_LIBUSB
       help
         Hailo RFS upload utility for USB gadget

Yocto Integration
-----------------

.. code-block:: bitbake

   # meta-hailo/recipes-support/hailo-rfs-upload/hailo-rfs-upload.bb
   SUMMARY = "Hailo RFS Upload Utility"
   LICENSE = "GPL-2.0"
   DEPENDS = "libusb1"

   SRC_URI = "file://hailo_rfs_upload.c"

Limitations
===========

- Single concurrent upload per device
- Maximum 256MB RFS image size
- Requires sufficient RAM for buffering
- USB 2.0 minimum requirement
- No compression support (handled externally)

Future Enhancements
===================

- Streaming mode (no buffering)
- Multiple filesystem format support
- Delta updates and incremental transfers
- Compression and decompression
- Multi-partition support
- Enhanced security features

Comparison with SWU Upload
==========================

=============== ================== =======================
Feature         RFS Upload         SWU Upload
=============== ================== =======================
Purpose         Root filesystem    Software update packages
Integration     Direct filesystem  swupdate framework
Execution       File write only    Full update process
Monitoring      Basic status       Detailed execution status
Use Case        Initial deployment Runtime updates
=============== ================== =======================

See Also
========

- :doc:`gadget` - USB Gadget Framework
- :doc:`gadget_hailo_swu_upload` - Hailo SWU Upload Gadget
- `libusb API Reference <https://libusb.info/>`_
- `Linux Filesystem Documentation <../filesystems/>`_

.. note::
   This documentation covers the Hailo RFS Upload USB Gadget implementation
   for Linux kernel versions 5.4 and later.

.. warning::
   Always verify RFS image integrity before upload to prevent device
   corruption or filesystem damage.
2. **Host Upload Tool** (``hailo_rfs_upload.c``) - C-based tool for uploading RFS images  
3. **Kernel Mount Integration** - Modified ``prepare_namespace`` with ``WAIT_INITRD_IMAGE`` support
4. **Configuration System** - Kernel configuration options for enabling/disabling RFS upload

Architecture
============

::

    Host System                    Target Device (Hailo10)
    ┌─────────────────┐           ┌─────────────────────────┐
    │ hailo_rfs_upload│           │ USB RFS Loading Function│
    │ C Upload Tool   │<--------->│ f_hailo_rfs_load.c      │
    │                 │   USB     │                         │
    │ RFS Image File  │           │ /initrd.image           │
    └─────────────────┘           └─────────────────────────┘
                                             │
                                             v
                                  ┌─────────────────────────┐
                                  │ Initrd Wait System      │
                                  │ init/do_mounts_initrd.c │
                                  │                         │
                                  │ CONFIG_WAIT_INITRD_IMAGE│
                                  │ rd_load_image()         │
                                  └─────────────────────────┘
                                             │
                                             v
                                  ┌─────────────────────────┐
                                  │ Root Filesystem         │
                                  │ /                       │
                                  └─────────────────────────┘

USB Protocol
============

Vendor Requests
---------------

* **0x03** - ``HAILO_REQ__RFS_GET_STATUS``: Returns current upload status string
* **0x11** - ``HAILO_REQ__RFS_GET_INFO``: Get RFS model information  
* **0x12** - ``HAILO_REQ__RFS_LOAD``: Load RFS image command (with file size)
* **0x13** - ``HAILO_REQ__RFS_FINISH``: Finish RFS loading
* **0x15** - ``HAILO_REQ__RFS_LOAD_CTRL``: RFS control operations

RFS Control Sub-commands (0x15)
--------------------------------

* **0x00** - ``GET_STATUS``: Return ready status (1 byte: 1=ready, 0=not ready)
* **0x01** - ``GET_RX_CNT``: Return bytes received count (4 bytes)
* **0x02** - ``CLR_RX_CNT``: Reset RFS counter (1 byte response)

Status Responses
----------------

* **"rfs:XXXXXX"** - Upload in progress, XXXXXX = hex bytes received
* **"idle:ready"** - Device ready for upload

USB Device Identifiers
-----------------------

* **Vendor ID**: ``0x0B05`` (ASUSTek Computer, Inc.)
* **Product ID**: ``0x1D6F`` (Hailo Gadget)

USB Interface
-------------

* **Interface Class**: ``0xFF`` (Vendor-specific)
* **Interface Protocol**: ``0x01`` (RFS Load mode)
* **Endpoints**: Bulk OUT + Interrupt IN (2 endpoints total)

Transfer Parameters
-------------------

* **Bulk Transfer Size**: 64KB chunks
* **Maximum RFS Size**: 256MB
* **USB Timeout**: 10 seconds
* **Control Timeout**: 5 seconds

Host Tool
=========

C Tool (``hailo_rfs_upload``)
-----------------------------

The host upload tool is provided as source code in the kernel documentation:

::

    # Build using the provided build script
    cd Documentation/usb/
    ./build_hailo_rfs_upload.sh

    # Or compile manually with libusb
    gcc -o hailo_rfs_upload hailo_rfs_upload.c -lusb-1.0 \
        -I../../drivers/usb/gadget/function -I/usr/include/libusb-1.0

Usage::

    # Upload RFS image
    ./hailo_rfs_upload filesystem.ext4

    # Verbose mode with progress
    ./hailo_rfs_upload -v filesystem.ext4

    # Status-only check (no upload)
    ./hailo_rfs_upload --status

Features
--------

* Multi-configuration USB device support (automatically finds RFS configuration)
* 64KB optimized bulk transfers for maximum performance
* Progress reporting with transfer speed
* Enhanced device status parsing and verification
* Bulk endpoint readiness testing with ping mechanism
* Zero-length packet (ZLP) flushing for reliable transfers
* Comprehensive error handling and retry logic
* Status-only mode for device monitoring

Building the Tool
-----------------

The host tool source code and build script are included in the kernel documentation 
for easy access and building:

**Method 1: Using the build script (recommended)**::

    cd Documentation/usb/
    chmod +x build_hailo_rfs_upload.sh
    ./build_hailo_rfs_upload.sh

**Method 2: Manual compilation**::

    # Install libusb development package first (Ubuntu/Debian)
    sudo apt-get install libusb-1.0-0-dev

    # Compile
    cd Documentation/usb/
    gcc -o hailo_rfs_upload hailo_rfs_upload.c -lusb-1.0 \
        -I../../drivers/usb/gadget/function -I/usr/include/libusb-1.0

**Requirements:**

* libusb-1.0 development headers
* GCC compiler
* Access to kernel source headers (for USB gadget function definitions)

Kernel Configuration
====================

Enable RFS Upload Support
--------------------------

::

    # Core USB gadget support
    CONFIG_USB_GADGET=y
    CONFIG_USB_LIBCOMPOSITE=y

    # Hailo RFS loading function
    CONFIG_USB_F_HAILO_RFS_LOAD=y  # This automatically selects WAIT_INITRD_IMAGE

    # Initrd wait mechanism (automatically selected by USB_F_HAILO_RFS_LOAD)
    CONFIG_WAIT_INITRD_IMAGE=y
    CONFIG_WAIT_INITRD_IMAGE_TIMEOUT=300  # 5 minutes timeout
    CONFIG_BLK_DEV_INITRD=y

Kernel Parameters
-----------------

::

    # Enable waiting for initrd image upload with RAM disk root
    root=/dev/ram0 wait_initrd_image ramdisk_size=0x10000000

Device Tree Example
-------------------

::

    chosen {
        bootargs = "console=ttyS1,115200n8 quiet loglevel=3 rootwait wait_initrd_image root=/dev/ram0 ramdisk_size=0x10000000";
        linux,initrd-start = <0x90000000>;
        linux,initrd-end = <0xA0000000>;
    };

Boot Process
============

Normal Boot Flow
----------------

1. Kernel starts
2. ``prepare_namespace()`` called
3. ``initrd_load()`` checks for initrd
4. Regular root device mounted
5. Init process started

RFS Upload Boot Flow
--------------------

1. Kernel starts with ``wait_initrd_image`` parameter and ``root=/dev/ram0``
2. ``prepare_namespace()`` detects wait_initrd_image mode
3. ``initrd_load()`` waits for ``/initrd.image`` to become available
4. Host tool uploads RFS image via USB RFS loading function
5. USB function saves received data to ``/initrd.image``
6. ``initrd_load()`` detects ``/initrd.image`` and loads it into RAM disk
7. Standard initrd handling mounts RAM disk as root
8. Init process started from uploaded RFS

Upload Protocol Flow
--------------------

1. **Device Detection**: Host tool finds Hailo device and RFS configuration
2. **Status Check**: Verify device is ready for upload
3. **Start Upload**: Send ``HAILO_REQ__RFS_LOAD`` with file size
4. **Buffer Allocation**: Device allocates vmalloc buffer for RFS data
5. **Data Transfer**: Stream RFS data in 64KB chunks via bulk OUT
6. **Completion**: Send ``HAILO_REQ__RFS_FINISH`` to finalize upload
7. **File Write**: Device writes complete RFS to ``/initrd.image``

File Locations
==============

Target Device (Hailo10)
------------------------

* **USB RFS Loading Function**: ``drivers/usb/gadget/function/f_hailo_rfs_load.c``
* **Function Headers**: ``drivers/usb/gadget/function/u_hailo_rfs_load.h``
* **Gadget Composite**: ``drivers/usb/gadget/legacy/hailo.c`` (multi-configuration)
* **Initrd Wait Integration**: ``init/do_mounts_initrd.c``
* **Configuration**: ``init/Kconfig`` (WAIT_INITRD_IMAGE), ``drivers/usb/gadget/Kconfig`` (USB_F_HAILO_RFS_LOAD)
* **Upload Storage**: ``/initrd.image`` (temporary storage for RFS data)
* **Final Mount**: ``/dev/ram0`` (loaded initrd in reserved memory)

Host System  
-----------

* **Upload Tool Source**: ``Documentation/usb/hailo_rfs_upload.c`` (C-based with libusb)
* **Build Script**: ``Documentation/usb/build_hailo_rfs_upload.sh``
* **RFS Images**: Any ext2/ext3/ext4/squashfs filesystem image

Supported Filesystems
======================

The uploaded RFS image is loaded as an initrd, so it supports any filesystem 
that can be used in initrd:

1. **ext2/ext3/ext4** (recommended for root filesystems)
2. **cramfs** (compressed, read-only)
3. **squashfs** (compressed, read-only)
4. **cpio/tar archives** (traditional initrd format)
5. **gzip compressed** versions of any of the above

The initrd loading mechanism will automatically detect the filesystem type.

Compression Support
===================

The system supports **gzip-compressed** RFS images for faster transfers:

Upload Methods
--------------

* **Uncompressed**: Direct upload of ``.ext4`` files
* **Pre-compressed**: Upload ``.ext4.gz`` files (automatically detected)

Troubleshooting
===============

Enable Debug Output
-------------------

::

    # Kernel messages
    dmesg | grep -i hailo

    # USB subsystem debug
    echo 'module usbcore +p' > /sys/kernel/debug/dynamic_debug/control
    echo 'module usb_gadget +p' > /sys/kernel/debug/dynamic_debug/control

    # RFS loading function debug
    echo 'module f_hailo_rfs_load +p' > /sys/kernel/debug/dynamic_debug/control

Common Issues
-------------

1. **"Cannot get config descriptor: LIBUSB_ERROR_NOT_FOUND"**

   * Device found but RFS configuration not accessible
   * Check if device is in correct USB configuration
   * Verify multi-configuration support in gadget driver

2. **"No RFS interface found in any configuration"**

   * RFS loading function not enabled or not bound
   * Check kernel config: ``CONFIG_USB_F_HAILO_RFS_LOAD=y``
   * Verify composite gadget includes RFS configuration

3. **"Cannot set configuration X: LIBUSB_ERROR_*"**

   * USB configuration switching failed
   * Check device permissions (may need root or udev rules)
   * Verify target device supports configuration switching

4. **Upload stalls during bulk transfer**

   * Check USB signal integrity and cable quality
   * Verify sufficient target RAM for vmalloc buffer
   * Monitor for atomic context violations in kernel logs

5. **"VFS: Unable to mount root fs on /dev/ram0"**

   * ``/initrd.image`` not created or corrupted
   * Check ``wait_initrd_image`` kernel parameter
   * Verify filesystem image integrity
   * Ensure ``CONFIG_WAIT_INITRD_IMAGE=y``

Integration Example
===================

Device Tree Configuration
-------------------------

::

    &usb_otg {
        status = "okay";
        dr_mode = "peripheral";
        pinctrl-names = "default";
        pinctrl-0 = <&pinctrl_usb_overcurrent_n_in>, <&pinctrl_usb_drive_vbus_out>;
    };

    chosen {
        bootargs = "console=ttyS1,115200n8 quiet loglevel=3 rootwait wait_initrd_image root=/dev/ram0 ramdisk_size=0x10000000";
        linux,initrd-start = <0x90000000>;
        linux,initrd-end = <0xA0000000>;
    };

Legacy Gadget Setup
-------------------

::

    #!/bin/sh
    # The composite hailo_ai gadget now includes RFS loading as configuration 2
    # Configuration 1: AI processing (default)
    # Configuration 2: RFS loading 
    # Configuration 3: Software update

    # Legacy gadget is typically built into kernel, no manual setup needed
    # VID: 0x0B05, PID: 0x1D6F

Performance Characteristics
===========================

Transfer Speeds
---------------

* **USB 2.0**: ~30-35 MB/s (theoretical 60 MB/s)
* **USB 3.0**: ~90-120 MB/s (theoretical 625 MB/s) 
* **Actual speeds depend on**: USB controller, cable quality, system load

Memory Usage
------------

* **vmalloc Buffer**: Allocated per RFS image size (up to 256MB)
* **USB Requests**: 64KB per bulk transfer
* **Workqueue**: Minimal overhead for file operations

Optimization Features
---------------------

* 64KB bulk transfers for maximum throughput
* Zero-length packet (ZLP) support for proper USB framing
* Atomic context avoidance (process context for memory allocation)
* Multi-configuration device support for endpoint resource management

Support
=======

For issues and questions:

1. Check kernel logs with ``dmesg``
2. Verify USB gadget configuration
3. Test with minimal RFS image first
4. Consult USB gadget framework documentation
