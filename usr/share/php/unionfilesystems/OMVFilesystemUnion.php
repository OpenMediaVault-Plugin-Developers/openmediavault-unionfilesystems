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
require_once "openmediavault/util.inc";

class OMVFilesystemUnion extends OMVFilesystemAbstract
{
    private $dataCached = false;

    /**
     * Extract an UUID from a path.
     *
     * @param string $path A path containing the UUID last.
     *
     * @return string The extracted UUID.
     */
    private function extractUuidFromPath($path)
    {
        $data = explode(DIRECTORY_SEPARATOR, $path);

        if (!empty($data)) {
            $uuid = end($data);

            if (is_uuid($uuid)) {
                return $uuid;
            }
        }

        throw new Exception("Couldn't extract an UUID from the provided path.");
    }

    /**
     * Get the pool configuration by its UUID.
     *
     * @param string $uuid The UUID.
     *
     * @return array|false A pool configuration.
     */
    private function getPoolConfiguration($uuid)
    {
        global $xmlConfig;

        $xpath = "/config/services/unionfilesystems/pools/pool[uuid='$uuid']";
        $pool = $xmlConfig->get($xpath);

        return !is_null($pool) ?  $pool : false;
    }

    /**
     * Constructor.
     *
     * @param string $id The UUID or device path of the filesystem.
     */
    public function __construct($id)
    {
        $this->uuid = $id;

        if (!is_uuid($id)) {
            // Assume we got the mount directory.
            $this->uuid = $this->extractUuidFromPath($id);
        }

        $this->usage = "filesystem";
    }

    /**
     * Get the filesystem detailed information.
     *
     * @return bool True if successful, otherwise false.
     */
    protected function getData()
    {
        if ($this->dataCached) {
            return true;
        }

        if (!($pool = $this->getPoolConfiguration($this->uuid))) {
            return false;
        }

        $this->label = $pool["name"];
        $this->type = $pool["type"];

        $this->dataCached = true;

        return true;
    }

    /**
     * Refresh the cached information.
     *
     * @return bool
     */
    public function refresh()
    {
        $this->dataCached = false;

        return $this->getData();
    }

    /**
     * Check if the filesystem exists.
     *
     * @return bool
     */
    public function exists()
    {
        return $this->getData();
    }

    /**
     * Check if the filesystem has an UUID.
     *
     * @return bool
     */
    public function hasUuid()
    {
        return $this->getUuid() !== false;
    }

    /**
     * Get the UUID of the filesystem.
     *
     * @return string|bool The UUID or false.
     */
    public function getUuid()
    {
        if (!$this->getData()) {
            return false;
        }

        return !empty($this->uuid) ? $this->uuid : false;
    }

    /**
     * Check if the filesystem has a label.
     *
     * @return bool
     */
    public function hasLabel()
    {
        return $this->getLabel() !== false;
    }

    /**
     * Get the filesystem label.
     *
     * @return string|bool The label or false.
     */
    public function getLabel()
    {
        if (!$this->getData()) {
            return false;
        }

        return !empty($this->label) ? $this->label : false;
    }

    /**
     * Get the filesystem type, e.g. aufs, btrfs, mhddfs.
     *
     * @return string|bool The fileystem type or false.
     */
    public function getType()
    {
        if (!$this->getData()) {
            return false;
        }

        return !empty($this->type) ? $this->type : false;
    }

    /**
     * Get the partition scheme, e.g. gpt, mbr.
     *
     * @return string|bool The filesystem scheme or false.
     */
    public function getPartitionScheme()
    {
         return false;
    }

    /**
     * Get the usage, e.g. other or filesystem.
     *
     * @return string|bool The usage or false.
     */
    public function getUsage()
    {
        if (!$this->getData()) {
            return false;
        }

        return !empty($this->usage) ? $this->usage : false;
    }

    /**
     * Get the partition entry information.
     *
     * @return array|bool An array with the fields scheme, uuid, type, flags,
     * number, offset, size and disk, otherwise false.
     */
    public function getPartitionEntry()
    {
        return false;
    }

    /**
     * Get the device path of the filesystem, e.g. /dev/sdb1.
     *
     * @return string|bool The device path, otherwise false.
     */
    public function getDeviceFile()
    {
        if (!$this->getData()) {
            return false;
        }

        return $this->getMountPoint();
    }

    /**
     * Get the canonical path of a device file. E.g. the real path to the device
     * file.
     *
     * @return string|bool The canonical path to a device file, otherwise false.
     */
    public function getCanonicalDeviceFile()
    {
        return false;
    }

    /**
     * Get the device file by UUID.
     *
     * @return string|bool The device path (/dev/disk/by-uuid/xxx) if available,
     * otherwise /dev/xxx will be returned. In case of an error false will be
     * returned.
     */
    public function getDeviceFileByUuid()
    {
        return false;
    }

    /**
     * Get the device file of the storage device containing this file system.
     * Example: /dev/sdb1 => /dev/sdb
     *
     * @return string|bool The device file of the underlying storage device,
     * otherwise false.
     */
    public function getStorageDeviceFile()
    {
        return false;
    }

    /**
     * Get the filesystem block size.
     *
     * @return int|bool The block size, otherwise false.
     */
    public function getBlockSize()
    {
        return false;
    }

    /**
     * Grow the filesystem.
     *
     * @return bool True if successful, otherwise false.
     */
    public function grow()
    {
        return false;
    }

    /**
     * Shrink the filesystem.
     *
     * @return bool True if successful, otherwise false.
     */
    public function shrink()
    {
        return false;
    }

    /**
     * Remove the filesystem.
     *
     * @return bool True if successful, otherwise false.
     */
    public function remove()
    {
        return false;
    }

    /**
     * Get the mount point of the given filesystem.
     *
     * @return string|bool The mount point or false.
     */
    public function getMountPoint()
    {
        if ($this->hasUuid()) {
            return self::buildMountPath($this->getUuid());
        }

        return false;
    }

    /**
     * Get statistics from a mounted filesystem.
     *
     * @return array|bool The filesystem statistics if successful, otherwise
     * false. The following fields are included: devicefile, type, blocks, size,
     * used, available, percentage and mountpoint. Please note, the fields size,
     * used and available are strings and their unit is B (bytes).
     */
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
                "devicefile" => $this->getDeviceFile(),
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

    /**
     * Check if the filesystem is mounted.
     *
     * @return bool
     */
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

    /**
     * Mount the filesystem by its device file or UUID.
     *
     * @param string $options Additional mount options. Empty string by default.
     *
     * @return bool
     */
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

    /**
     * Unmount the filesystem.
     *
     * @param bool $force Force the unmount.
     * @param bool $lazy Do a lazy unmount.
     *
     * @return bool
     */
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
