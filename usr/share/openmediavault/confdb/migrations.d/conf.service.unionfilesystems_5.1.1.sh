#!/bin/sh
#
# @license   http://www.gnu.org/licenses/gpl.html GPL Version 3
# @author    OpenMediaVault Plugin Developers <plugins@omv-extras.org>
# @copyright Copyright (c) 2020 OpenMediaVault Plugin Developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

set -e

. /usr/share/openmediavault/scripts/helper-functions

SERVICE_XPATH_NAME="unionfilesystems"
SERVICE_XPATH="/config/services/${SERVICE_XPATH_NAME}"
SERVICE_XPATH_FS="${SERVICE_XPATH}/filesystem"

count=$(omv_config_get_count "${SERVICE_XPATH_FS}");
index=1;
while [ ${index} -le ${count} ]; do
    if omv_config_exists "${SERVICE_XPATH_FS}[position()=${index}]/self-mntentref"; then
        omv_config_rename "${SERVICE_XPATH_FS}[position()=${index}]/self-mntentref" "self_mntentref"
    fi
    if omv_config_exists "${SERVICE_XPATH_FS}[position()=${index}]/create-policy"; then
        omv_config_rename "${SERVICE_XPATH_FS}[position()=${index}]/create-policy" "create_policy"
    fi
    if omv_config_exists "${SERVICE_XPATH_FS}[position()=${index}]/min-free-space"; then
        omv_config_rename "${SERVICE_XPATH_FS}[position()=${index}]/min-free-space" "min_free_space"
    fi
    index=$(( ${index} + 1 ))
done;

exit 0
