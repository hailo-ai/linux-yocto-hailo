#!/bin/bash

# Require success of commands
set -eo pipefail

SCRIPT_PATH=$(realpath $(dirname "$0"))

if [ -f "${SCRIPT_PATH}/build.cfg" ]; then
    source "${SCRIPT_PATH}/build.cfg"
else
    source "${SCRIPT_PATH}/build.cfg.default"
fi

source ${TOOLCHAIN}

MAKE="make ARCH=arm64 -j16"

extract_machine() {
	if [ -f "${DEPLOY_DIR}/machine_name.txt" ]; then
        cat "${DEPLOY_DIR}/machine_name.txt"
        return
    fi
    local filename=$(find ${DEPLOY_DIR}/ -type l -name 'u-boot-*.dtb' -printf %P)
    local machine_name=${filename#u-boot-}
    echo ${machine_name%.dtb}
}

if [ -z "$MACHINE" ]; then
	MACHINE=$(extract_machine)
fi

# Function to align a file by adding padding to make its size a multiple of `pad_size`
align_file() {
    binary_file="$1"
    pad_size="$2"

    # Get the current size of the file
    current_size=$(stat --format=%s "$binary_file")

    # Calculate padding needed to make the file size a multiple of `pad_size`
    padding_needed=$(expr $pad_size - \( $current_size % $pad_size \))

    # If no padding is needed (already aligned), exit early
    if [ "$padding_needed" -eq "$pad_size" ]; then
        return
    fi

    # Use dd to add zero padding (or any other byte pattern) to the file
    dd if=/dev/zero bs=1 count="$padding_needed" >> "$binary_file"
}

build_module() {
	module_path=$1
	$MAKE M=$module_path
}

deploy_module() {
	module_path=$1
	sudo $MAKE M=$module_path INSTALL_MOD_PATH=${NFS_DIR} modules_install
}

deploy_modules() {
	echo "Deploying modules to ${NFS_DIR}..."

    # Deploy standard modules to NFS directory
	sudo INSTALL_MOD_PATH=${NFS_DIR} $MAKE modules_install

    # Deploy extra modules to NFS directory
	if [ -n "${HAILORT_DIR}" ]; then
		deploy_module ${HAILORT_DIR}
	fi
	if [ -n "${ENCODER_DIR}" ]; then
		deploy_module ${ENCODER_DIR}
	fi
}

deploy_image() {
	echo "Deploying images to ${DEPLOY_DIR}..."
	cp fitImage ${DEPLOY_DIR}
	cp vmlinux ${DEPLOY_DIR}
	if [ "${EXPORT_MODULES_TO_NFS}" = "yes" ]; then
		deploy_modules
	fi
    echo "Image deployment completed."
}

make_all(){
    echo "Building kernel..."
	if [ ! -f .config ]; then
		if [ -f "${DEPLOY_DIR}/kernel.config" ]; then
			cp "${DEPLOY_DIR}/kernel.config" ${SCRIPT_PATH}/.config
		else
			echo ".config file not found in current directory or deploy directory. Exiting."
			exit 1
		fi
	fi
	$MAKE DTC_FLAGS=-@
	cp ${DEPLOY_DIR}/fitImage-its-${MACHINE} fit-image.its
	if grep -q "gzip" fit-image.its; then
		cp arch/arm64/boot/Image.gz linux.bin
	else
		cp arch/arm64/boot/Image linux.bin
	fi
	align_file linux.bin 64
	cp ${DEPLOY_DIR}/bl31.bin .
	align_file bl31.bin 64
	align_file arch/arm64/boot/dts/hailo/${MACHINE}.dtb 64
	uboot-mkimage -f fit-image.its fitImage
	uboot-mkimage -E -B 0x40 -F -k ${DEPLOY_DIR} -r fitImage

    # Build extra modules only if we are exporting modules to NFS
    if [ "${EXPORT_MODULES_TO_NFS}" = "yes" ]; then
		if [ -n "${HAILORT_DIR}" ]; then
			build_module ${HAILORT_DIR}
		fi
		if [ -n "${ENCODER_DIR}" ]; then
			build_module ${ENCODER_DIR}
		fi
	fi

    echo "Build completed."
}

usage() {
    echo "Usage: $0 [build|deploy|all|clean-env|help] [extra make args...]";
}

case "${1-}" in
  ""|"all"|"build-deploy"|"-a") make_all; deploy_image ;;  # Default action
  "build"|"build-only"|"-b")    make_all ;;
  "deploy"|"deploy-only"|"-d")  deploy_image ;;
  "help"|"-h"|"--help")         usage ;;
  "clean-env"|"-c")             $MAKE mrproper ;;
  *)                            $MAKE "$@" ;;   # forward to make
esac
