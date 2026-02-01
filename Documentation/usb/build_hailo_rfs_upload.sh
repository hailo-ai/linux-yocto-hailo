#!/bin/bash
# 
# Build script for Hailo RFS Upload Tool
#

echo "Building Hailo RFS Upload Tool..."

# Check for libusb-1.0 development headers
if ! pkg-config --exists libusb-1.0; then
    echo "Error: libusb-1.0 development package not found"
    echo "Please install: sudo apt-get install libusb-1.0-0-dev"
    exit 1
fi

# Get libusb flags
USB_CFLAGS=$(pkg-config --cflags libusb-1.0)
USB_LIBS=$(pkg-config --libs libusb-1.0)

# Build the tool
gcc -Wall -Wextra -g $USB_CFLAGS -o hailo_rfs_upload hailo_rfs_upload.c $USB_LIBS

if [ $? -eq 0 ]; then
    echo "✓ Build successful: hailo_rfs_upload"
    echo "Usage: ./hailo_rfs_upload [-v] <rfs_image_file>"
    echo ""
    echo "Note: You may need root privileges or udev rules for USB access"
else
    echo "✗ Build failed"
    exit 1
fi