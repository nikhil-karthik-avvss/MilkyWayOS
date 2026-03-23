#!/bin/bash
# MilkyWayOS disk image builder
# Creates a bootable disk image from the rootfs

set -e

ROOTFS=~/milkywayos/rootfs
IMG=~/milkywayos/milkywayos.img

# Set up loop device
sudo losetup -fP $IMG
LOOPDEV=$(sudo losetup -j $IMG | cut -d: -f1)

# Partition and format
sudo parted $LOOPDEV --script mklabel msdos
sudo parted $LOOPDEV --script mkpart primary ext4 1MiB 100%
sudo mkfs.ext4 ${LOOPDEV}p1

# Copy rootfs
sudo mkdir -p /mnt/milkyway
sudo mount ${LOOPDEV}p1 /mnt/milkyway
sudo cp -a $ROOTFS/* /mnt/milkyway/

# Install GRUB
sudo grub-install --target=i386-pc --boot-directory=/mnt/milkyway/boot $LOOPDEV

# Generate GRUB config
sudo mount --bind /dev /mnt/milkyway/dev
sudo mount --bind /proc /mnt/milkyway/proc
sudo mount --bind /sys /mnt/milkyway/sys
sudo mount --bind /dev/pts /mnt/milkyway/dev/pts
sudo chroot /mnt/milkyway update-grub

# Cleanup
sudo umount /mnt/milkyway/dev/pts
sudo umount /mnt/milkyway/dev
sudo umount /mnt/milkyway/proc
sudo umount /mnt/milkyway/sys
sudo umount /mnt/milkyway
sudo losetup -d $LOOPDEV

echo "MilkyWayOS image built successfully!"
