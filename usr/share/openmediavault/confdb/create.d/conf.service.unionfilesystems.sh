#!/bin/sh

set -e

. /usr/share/openmediavault/scripts/helper-functions

if ! omv_config_exists "/config/services/unionfilesystems"
then
    omv_config_add_node "/config/services" "unionfilesystems"
fi
