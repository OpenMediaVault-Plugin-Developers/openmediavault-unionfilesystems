#!/bin/sh

set -e

. /usr/share/openmediavault/scripts/helper-functions

xpath="/config/services/unionfilesystems"

# Rename and relocate each filesystem.
omv_config_move "$xpath/pools/pool" "$xpath"
omv_config_rename "$xpath/pool" "filesystem"

# Convert everything to fuse.mergerfs.
omv_config_delete "$xpath/filesystem/type"
omv_config_update "/config/system/fstab/mntent/type[. = 'aufs' or . = 'mergerfs' or . = 'mhddfs']" "fuse.mergerfs"

# Set default values on the filesystem to prevent future fstab breakage.
omv_config_update "$xpath/filesystem/create-policy" "epmfs"
omv_config_update "$xpath/filesystem/min-free-space" "4G"
omv_config_update "$xpath/filesystem/options" "defaults,allow_other"
