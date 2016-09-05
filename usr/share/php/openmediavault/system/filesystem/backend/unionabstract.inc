<?php

/**
 * Copyright (C) 2014-2015 OpenMediaVault Plugin Developers.
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

namespace OMV\System\Filesystem\Backend;

use OMV\Config\Database;
use OMV\Config\DatabaseException;
use OMV\System\Filesystem\Union as UnionFilesystem;

abstract class UnionAbstract extends BackendAbstract
{
    /**
     * @var Database
     */
    private $database;

    /**
     * @var string
     */
    private $dataModelPath = 'conf.service.unionfilesystems.pool';

    public function __construct(Database $database)
    {
        $this->database = $database;
    }

    /**
     * Get a list of filesystems of the given filesystem backend.
     *
     * @return array|bool A list of filesystems, otherwise false.
     */
    public function enumerate()
    {
        $filesystems = [];
        $objects = [];

        try {
            foreach ($this->database->get($this->dataModelPath) as $object) {
                if ($object->get('type') === $this->type) {
                    $objects[] = $object;
                }
            }
        } catch (DatabaseException $e) {
            return $filesystems;
        }

        foreach ($objects as $object) {
            $filesystem = $this->getImpl($object->get('uuid'));

            $filesystems[$filesystem->getDeviceFile()] = [
                'devicefile' => $filesystem->getDeviceFile(),
                'uuid' => $filesystem->getUuid(),
                'label' => $filesystem->getLabel(),
                'type' => $filesystem->getType(),
            ];
        }

        return $filesystems;
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
     * @return bool
     */
    public function isTypeOf($id)
    {
        $mounts = $this->enumerate();

        foreach ($mounts as $mount) {
            if ($mount['uuid'] == $id || $mount['devicefile'] == $id) {
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
     * @return \OMV\System\Filesystem\IFilesystem
     */
    public function getImpl($args)
    {
        $uuid = $args;

        if (!is_uuid($args)) {
            // Assume we got the mount directory.
            $pathUuid = end(explode(DIRECTORY_SEPARATOR, $args));

            if (!is_uuid($pathUuid)) {
                throw new Exception('Couldn\'t extract an UUID from the provided path.');
            }

            $uuid = $pathUuid;
        }

        $object = $this->database->get($this->dataModelPath, $uuid);

        return new UnionFilesystem($object, $this);
    }
}
