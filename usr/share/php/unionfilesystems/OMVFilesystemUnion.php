<?php

/**
 * Copyright (C) 2014 OpenMediaVault Plugin Developers
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
require_once "openmediavault/util.inc";

class OMVFilesystemUnion extends OMVFilesystemAbstract
{
    private $dataCached = false;

    private function getPoolXpathByUuid($uuid)
    {
        return sprintf(
            "/config/services/unionfilesystems/pools/pool[uuid='%s']",
            $uuid
        );
    }

    public function __construct($id)
    {
        $this->deviceFile = $id;
        $this->usage = "filesystem";
    }

    protected function getData()
    {
        if ($this->dataCached) {
            return true;
        }

        global $xmlConfig;

        $uuid = str_replace(
            $GLOBALS["OMV_MOUNT_DIR"] . "/",
            "",
            $this->deviceFile
        );

        $pool = $xmlConfig->get($this->getPoolXpathByUuid($uuid));

        $this->label = $pool["name"];
        $this->type = $pool["type"];

        $this->dataCached = true;

        return true;
    }

    public function refresh()
    {
        $this->dataCached = false;

        return $this->getData() !== false;
    }

    public function exists()
    {
        return $this->getData() !== false;
    }

    public function hasUuid()
    {
        return false;
    }

    public function getUuid()
    {
        return false;
    }

    public function hasLabel()
    {
        $label = $this->getLabel();

        return !empty($label);
    }

    public function getLabel()
    {
        if (!$this->getData()) {
            return false;
        }

        return $this->label;
    }

    public function getType()
    {
        if (!$this->getData()) {
            return false;
        }

        return $this->type;
    }

    public function getPartitionScheme()
    {
         return false;
    }

    public function getUsage()
    {
        if (!$this->getData()) {
            return false;
        }

        return $this->usage;
    }

    public function getPartitionEntry()
    {
        return false;
    }

    public function getDeviceFile()
    {
        if (!$this->getData()) {
            return false;
        }

        return $this->deviceFile;
    }

    public function getCanonicalDeviceFile()
    {
        if (!$this->getData()) {
            return false;
        }

        return $this->deviceFile;
    }

    public function getDeviceFileByUuid()
    {
        return false;
    }

    public function getStorageDeviceFile()
    {
        return false;
    }

    public function getBlockSize()
    {
        return false;
    }

    public function remove()
    {
        return false;
    }

    public function getMountPoint()
    {
        return $this->deviceFile;
    }

    public function getStatistics()
    {
        if (!$this->getData()) {
            return false;
        }

        $mountPoint = $this->getMountPoint();

        if (!$mountPoint) {
            return false;
        }

        @OMVUtil::exec("export LANG=C; df -PT $mountPoint", $output, $result);

        if ($result !== 0) {
            $this->setLastError($output);

            return true;
        }

        $result = false;

        foreach ($output as $line) {
            $matches = preg_split("/[\s,]+/", $line);

            if (0 !== strcasecmp($mountPoint, $matches[6])) {
                continue;
            }

            $result = array(
                "devicefile" => $this->deviceFile,
                "type" => $matches[1],
                "blocks" => $matches[2],
                "size" => bcmul($matches[2], "1024", 0),
                "used" => binary_convert($matches[3], "KiB", "B"),
                "available" => binary_convert($matches[4], "KiB", "B"),
                "percentage" => intval(trim($matches[5], "%")),
                "mountpoint" => $matches[6]
            );
        }

        return $result;
    }

    public function isMounted()
    {
        if (!$this->getData()) {
            return false;
        }

        $mountPoint = $this->getMountPoint();

        if (!$mountPoint) {
            return false;
        }

        $cmd = sprintf(
            "export LANG=C; mount | grep %s | grep %s",
            $this->type,
            $mountPoint
        );

        @OMVUtil::exec($cmd, $output, $result);

        if ($result !== 0) {
            $this->setLastError($output);

            return false;
        }

        return true;
    }

    public function mount($options = "")
    {
        $cmd = sprintf("export LANG=C; mount %s 2>&1", $this->getMountPoint());

        @OMVUtil::exec($cmd, $output, $result);

        if ($result !== 0) {
            $this->setLastError($output);

            return false;
        }

        return true;
    }

    public function umount($force = false, $lazy = false)
    {
        $cmd = sprintf("export LANG=C; umount %s 2>&1", $this->getMountPoint());

        @OMVUtil::exec($cmd, $output, $result);

        if ($result !== 0) {
            $this->setLastError($output);

            return false;
        }

        return true;
    }
}
