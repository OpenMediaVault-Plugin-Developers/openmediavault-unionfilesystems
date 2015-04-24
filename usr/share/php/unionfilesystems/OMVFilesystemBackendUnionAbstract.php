<?php

/**
 * Copyright (C) 2014-2015 OpenMediaVault Plugin Developers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

require_once "openmediavault/config.inc";
require_once "openmediavault/system.inc";

require_once "unionfilesystems/OMVFilesystemUnion.php";

abstract class OMVFilesystemBackendUnionAbstract extends OMVFilesystemBackendAbstract
{
    /**
     * Get all pool configurations of a filesystem type.
     *
     * @param string $type The filesystem type (e.g. aufs, mhddfs).
     *
     * @return array Pool configurations.
     */
    private function getPoolConfigurationsByType($type)
    {
        global $xmlConfig;

        $xpath = "/config/services/unionfilesystems/pools/pool[type='$type']";

        return $xmlConfig->getList($xpath);
    }

    /**
     * Get a list of filesystems of the given filesystem backend.
     *
     * @return array|bool A list of filesystems, otherwise false.
     */
    public function enumerate()
    {
        global $xmlConfig;

        $result = array();
        $pools = $this->getPoolConfigurationsByType($this->type);

        if ($pools) {
            foreach ($pools as $pool) {
                $filesystem = $this->getImpl($pool["uuid"]);

                $result[$filesystem->getDeviceFile()] = array(
                    "devicefile" => $filesystem->getDeviceFile(),
                    "uuid" => $filesystem->getUuid(),
                    "label" => $filesystem->getLabel(),
                    "type" => $filesystem->getType(),
                );
            }
        }

        return $result;
    }

    /**
     * Check whether the filesystem implemented by this backend is identified by
     * the block device identification library. If this is not the case, then
     * the backend must override the enumerate method.
     *
     * @return bool
     */
    public function isBlkidEnumerated()
    {
        return false;
    }

    /**
     * Check whether the given filesystem identifier is represented by this
     * filesystem backend.
     *
     * @param string $id The filesystem identifier (e.g. UUID or device path).
     *
     * @return bool True if represented, otherwise false.
     */
    public function isTypeOf($id)
    {
        $mounts = $this->enumerate();

        foreach ($mounts as $mount) {
            if ($mount["uuid"] == $id || $mount["devicefile"] == $id) {
                return true;
            }
        }

        return false;
    }

    /**
     * Does the filesystem have a device file? E.g. union mount or overlay
     * filesystems like overlayfs and mhddfs don't have a device file.
     *
     * @return bool
     */
    public function hasDeviceFile()
    {
        return false;
    }

    /**
     * Get the object of the class that represents and implements a filesystem
     * of this filesystem backend.
     *
     * @param array $args The arguments to the class constructor.
     *
     * @return OMVFilesystemAbstract|null
     */
    public function getImpl($args)
    {
        return new OMVFilesystemUnion($args);
    }
}
