=======================================
Hailo SWU Upload USB Gadget Driver
=======================================

:Author: Hailo Technologies
:Date: November 2025
:Version: 1.0

Overview
========

The Hailo SWU Upload USB Gadget (``f_hailo_swu_load.c``) is a specialized USB function driver designed for uploading SWU (Software Update) images to Hailo10 devices. This gadget enables efficient firmware and root filesystem updates through USB communication.

Architecture
============

USB Configuration
-----------------

============== =============================================
Parameter      Value
============== =============================================
Vendor ID      0x0B05 (ASUSTek Computer, Inc.)
Product ID     0x1D6F (Hailo SWU load Gadget)
Interface Class 0xFF (Vendor-specific)
Interface Protocol 1 (SW update mode protocol)
Endpoints      Bulk OUT (data), Interrupt IN (status)
============== =============================================

Key Components
--------------

1. **Kernel Driver** (``f_hailo_swu_load.c``)
   
   - USB gadget function implementation
   - SWU image buffering and processing
   - Integration with swupdate mechanism
   - Status reporting and control interface

2. **Userspace Tool** (``hailo_sw_update.c``)
   
   - libusb-based client application
   - SWU image upload functionality
   - Progress monitoring and status reporting
   - Error handling and retry logic

System Architecture Diagram
---------------------------

::

Host System                    Target Device (Hailo10)
┌──────────────────┐           ┌─────────────────────────┐
│ hailo_sw_update  │           │ USB SWU Gadget Function │
│                  │  Control  │ f_hailo_swu_load.c      │
│ • Upload SWU     │ Requests  │ • HAILO_REQ__SWU_*      │
│ • Monitor Status │<--------->│ • Bulk OUT (64KB)       │
│ • Wait Execution │   USB     │ • vmalloc buffer        │
│                  │  Bulk     │ • Status reporting      │
│ SWU Image File   │ Transfer  │                         │
└──────────────────┘           └─────────────────────────┘
                                         │
                                         │ Write buffered data
                                         v
                               ┌─────────────────────────┐
                               │    /tmp/image.swu       │
                               │   Temporary SWU Storage │
                               └─────────────────────────┘
                                         │
                                         │ Execute script
                                         v
                               ┌─────────────────────────┐
                               │  /etc/run_swupdate.sh   │
                               │   SWUpdate Script       │
                               └─────────────────────────┘
                                         │
                                         v
                               ┌─────────────────────────┐
                               │ SWUpdate Process        │
                               │ • IDLE/IN_PROGRESS      │
                               │ • END_OK/END_FAIL       │
                               │ • Real-time monitoring  │
                               │ • Exit code reporting   │
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
0x03         HAILO_REQ__SWU_GET_STATUS         Get function status string
0x11         HAILO_REQ__SWU_GET_INFO           Get SWU model information
0x12         HAILO_REQ__SWU_LOAD               Load SWU image command
0x13         HAILO_REQ__SWU_FINISH             Finish SWU loading
0x15         HAILO_REQ__SWU_CTRL               SWU control operations
0x16         HAILO_REQ__SWU_SYS_REBOOT         System reboot command
============ ================================= ==========================================

SWU Control Sub-commands (0x15)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

============ ======================= =========================================
Sub-command  Name                    Response
============ ======================= =========================================
0            GET_STATUS              Ready status (1 byte)
1            GET_RX_CNT              Bytes received count (4 bytes, LE)
2            CLR_RX_CNT              Reset SWU counter (1 byte response)
3            GET_EXECUTION_STATUS    Swupdate status (8 bytes: state + exit)
============ ======================= =========================================

Upload Protocol Flow
--------------------

1. **Initialization**::

    Host → Device: HAILO_REQ__SWU_LOAD (wValue=size_low, wIndex=size_high)
    Device → Host: ACK (prepares vmalloc buffer)

2. **Data Transfer**::

    Host → Device: Bulk OUT transfers (64KB chunks)
    Device: Buffers data in vmalloc memory

3. **Completion**::

    Host → Device: HAILO_REQ__SWU_FINISH
    Device: Writes buffered data to /tmp/image.swu
    Device: Executes /etc/run_swupdate.sh

4. **Status Monitoring**::

    Host → Device: HAILO_REQ__SWU_CTRL (GET_EXECUTION_STATUS)
    Device → Host: State + Exit Code (8 bytes)

5. **System Reboot** (Optional)::

    Host → Device: HAILO_REQ__SWU_SYS_REBOOT
    Device → Host: ACK (initiates system reboot)
    Device: Executes /sbin/reboot via kernel thread

Status Strings
--------------

The ``HAILO_REQ__SWU_GET_STATUS`` request returns human-readable status strings:

=============== =======================================
Status String   Description
=============== =======================================
idle:ready      Device ready for operations
load:xxxxxx     SWU upload in progress, xxxxxx bytes received (hex)
exec:running    Executing SW update
invalid:xxxx    Invalid state or error
=============== =======================================

Execution States
----------------

The swupdate execution state machine:

===== ============= =====================================
State Name          Description
===== ============= =====================================
0     IDLE          Never started or completed
1     IN_PROGRESS   Currently running
2     END_OK        Completed successfully
3     END_FAIL      Completed with error
===== ============= =====================================

System Reboot Functionality
----------------------------

The ``HAILO_REQ__SWU_SYS_REBOOT`` command provides remote reboot capability:

Reboot Protocol
~~~~~~~~~~~~~~~

1. **Host Request**: Sends ``HAILO_REQ__SWU_SYS_REBOOT`` vendor request
2. **Immediate ACK**: Device acknowledges request immediately
3. **Kernel Thread**: Device creates background thread for reboot execution
4. **System Call**: Thread executes ``/sbin/reboot`` via ``call_usermodehelper()``
5. **System Shutdown**: Device initiates controlled system reboot

Use Cases
~~~~~~~~~

- **Post-Update Reboot**: Restart device after firmware/software updates
- **Remote Maintenance**: Perform system restart without physical access
- **Error Recovery**: Force restart when device becomes unresponsive
- **Configuration Changes**: Apply system-level configuration updates

Safety Features
~~~~~~~~~~~~~~~

- **Non-blocking Operation**: USB request returns immediately
- **Controlled Shutdown**: Uses proper system reboot mechanism
- **Kernel Thread**: Prevents blocking USB communication
- **Error Logging**: Comprehensive logging for debugging

Implementation Details
======================

Memory Management
-----------------

- **Buffer Allocation**: Uses ``vmalloc()`` for large SWU image buffering
- **Maximum Size**: 256MB per SWU image
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
  - Execution monitoring: 5 minutes

**Retry Logic**: Up to 3 retries for bulk transfers

**Errno Usage**: Appropriate errno values (``-ETIMEDOUT``, ``-EPERM``)

Integration with swupdate
-------------------------

- **Output File**: ``/tmp/image.swu``
- **Execution Script**: ``/etc/run_swupdate.sh`` triggered after successful upload
- **Monitoring**: Real-time status and exit code reporting
- **Security**: Validates image integrity and permissions

Usage Examples
==============

Basic Upload
------------

.. code-block:: bash

   ./hailo_sw_update firmware.swu

Verbose Upload with Progress
----------------------------

.. code-block:: bash

   ./hailo_sw_update -v firmware.swu

Status Check Only
-----------------

.. code-block:: bash

   ./hailo_sw_update --status

Upload Without Waiting for Completion
--------------------------------------

.. code-block:: bash

   ./hailo_sw_update -n firmware.swu

Device Reboot Only
-------------------

.. code-block:: bash

   ./hailo_sw_update --reboot

Configuration
=============

Kernel Configuration
--------------------

.. code-block:: kconfig

   CONFIG_USB_CONFIGFS=y
   CONFIG_USB_GADGET=y
   CONFIG_USB_F_HAILO_SWU_LOAD=y

Device Tree
-----------

.. code-block:: dts

   &usb_gadget {
       hailo_swu_load {
           compatible = "hailo,swu-load";
           status = "okay";
       };
   };

udev Rules
----------

.. code-block:: udev

   # /etc/udev/rules.d/99-hailo-swu.rules
   SUBSYSTEM=="usb", ATTR{idVendor}=="0b05", ATTR{idProduct}=="1d6f", MODE="0666"

Development and Debugging
=========================

Compilation
-----------

.. code-block:: bash

   # Userspace tool
   gcc -o hailo_sw_update hailo_sw_update.c -lusb-1.0

   # Or use build script
   ./build_hailo_sw_update.sh

Debugging
---------

.. code-block:: bash

   # Enable verbose output
   ./hailo_sw_update -v firmware.swu

   # Check kernel logs
   dmesg | grep hailo_swu

   # Monitor USB traffic
   usbmon

Security Considerations
=======================

- **Permissions**: Requires root privileges or udev rules
- **Validation**: SWU image integrity checking
- **Size Limits**: Maximum 256MB to prevent DoS
- **Rate Limiting**: Built-in timeouts and retry limits
- **Secure Boot**: Compatible with verified boot chains

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
   - Verify SWU image integrity
   - Monitor kernel logs

Log Analysis
------------

.. code-block:: bash

   # Kernel logs
   journalctl -k | grep hailo_swu

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

   // GET_EXECUTION_STATUS response
   struct swu_execution_status {
       __le32 state;       // HAILO_SWUPDATE_EXEC_STATE_*
       __le32 exit_code;   // Process exit code
   };

Limitations
===========

- Single concurrent upload per device
- Maximum 256MB SWU image size
- Requires sufficient RAM for buffering
- USB 2.0 minimum requirement

Future Enhancements
===================

- Streaming mode (no buffering)
- Compression support
- Multiple image types
- Enhanced security features
- Performance optimizations

See Also
========

- :doc:`gadget` - USB Gadget Framework
- `swupdate Documentation <https://sbabic.github.io/swupdate/>`_
- `libusb API Reference <https://libusb.info/>`_

.. note::
   This documentation covers the Hailo SWU Upload USB Gadget implementation
   for Linux kernel versions 5.4 and later.

.. warning::
   Always verify SWU image integrity before upload to prevent device
   corruption or security vulnerabilities.