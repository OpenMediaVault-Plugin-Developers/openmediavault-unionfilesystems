#!/bin/sh

set -e

. /usr/share/openmediavault/scripts/helper-functions

pool_index=1
pool_count=0
pool_xpath="/config/services/unionfilesystems/pools/pool"

pool_count=$(omv_config_get_count "$pool_xpath")

while [ "$pool_index" -lt "$pool_count" ] || [ "$pool_index" -eq "$pool_count" ]
do
    xpath="$pool_xpath[$pool_index]"

    # Rename/move some elements.
    omv_config_rename "$xpath/mntentref" "self-mntentref"
    omv_config_move "$xpath/branches/mntentref" "$xpath"

    # Remove unneeded elements.
    omv_config_delete "$xpath/branches"

    # Add/set default values.
    omv_config_add_key "$xpath" "create-policy" "epmfs"
    omv_config_add_key "$xpath" "min-free-space" "4G"
    omv_config_update "$xpath/options" "defaults,allow_other"

    if [ "$(omv_config_get "$xpath/type")" = "aufs" ]
    then
        omv_config_update "$xpath/create-policy" "tdp"
        omv_config_update "$xpath/options" "sum"
    fi

    pool_index=$((pool_index + 1))
done
