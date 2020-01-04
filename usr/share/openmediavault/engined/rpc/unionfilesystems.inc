<?php

/**
 * Copyright (C) 2013-2020 OpenMediaVault Plugin Developers.
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

namespace OMV\Engined\Rpc;

use Exception;
use OMV\Config\ConfigObject;
use OMV\Config\Database;
use OMV\Engine\Module\Manager as ModuleManager;
use OMV\Rpc\Rpc;
use OMV\Rpc\ServiceAbstract;
use OMV\System\Filesystem\Filesystem;
use OMV\System\MountPoint;
use OMV\Uuid;

class UnionFilesystems extends ServiceAbstract
{
    /**
     * @var Database
     */
    private $database;

    /**
     * @var string
     */
    private $dataModelPath = 'conf.service.unionfilesystems.filesystem';

    public function __construct(Database $database = null)
    {
        $this->database = $database ?: Database::getInstance();
    }

    /**
     * Get the name of the RPC service.
     *
     * @return string
     */
    public function getName()
    {
        return 'UnionFilesystems';
    }

    /**
     * Initialize the RPC service.
     *
     * @return void
     */
    public function initialize()
    {
        $this->registerMethod('enumerateCandidates');
        $this->registerMethod('getList');
        $this->registerMethod('get');
        $this->registerMethod('set');
        $this->registerMethod('delete');
    }

    /**
     * Enumerate the available filesystem candidates that can be added to a
     * union filesystem.
     *
     * @param $params  The method parameters
     * @param $context The context of the caller
     *
     * @return array
     */
    public function enumerateCandidates($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');

        $candidates = [];
        $entries = Rpc::call('FsTab', 'enumerateEntries', [], $context);

        foreach ($entries as $entry) {
            // Exclude these types.
            if ($entry['type'] == 'fuse.mergerfs' || $entry['type'] == 'none') {
                continue;
            }

            $filesystem = Filesystem::getImpl($entry['fsname']);

            if (!is_null($filesystem) && $filesystem->exists()) {
                $candidates[] = [
                    'uuid' => $entry['uuid'],
                    'devicefile' => $filesystem->getDeviceFile(),
                    'dir' => $entry['dir'],
                    'fsname' => $entry['fsname'],
                    'label' => $filesystem->getLabel(),
                    'type' => $entry['type'],
                ];
            }
        }

        return $candidates;
    }

    /**
     * Get a list of the existing union filesystems.
     *
     * @param $params  The method parameters
     * @param $context The context of the caller
     *
     * @return array
     */
    public function getList($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.getlist');

        $filesystems = array_map(function ($object) use ($context) {
            $branchesInfo = array_map(function ($mntentref) use ($context) {
                $mntent = Rpc::call('FsTab', 'get', ['uuid' => $mntentref], $context);
                $filesystem = Filesystem::getImpl($mntent['fsname']);

                if ($filesystem && $filesystem->exists()) {
                    return $filesystem->getDeviceFile() . ' (' . $filesystem->getLabel() . ')';
                }

                return 'n/a';
            }, $object->get('mntentref'));

            $object->add('source_mounts_info', 'any');
            $object->set('source_mounts_info', $branchesInfo);

            return $object->getAssoc();
        }, $this->database->get($this->dataModelPath));

        return $this->applyFilter(
            $filesystems,
            $params['start'],
            $params['limit'],
            $params['sortfield'],
            $params['sortdir']
        );
    }

    /**
     * Get the configuration for a single existing union filesystem.
     *
     * @param $params  The method parameters
     * @param $context The context of the caller
     *
     * @return array
     */
    public function get($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');

        $object = $this->database->get($this->dataModelPath, $params['uuid']);

        return $object->getAssoc();
    }

    /**
     * Set the configuration for a single new or existing union filesystem.
     *
     * @param $params  The method parameters
     * @param $context The context of the caller
     *
     * @return array
     */
    public function set($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.unionfilesystems.set');

        // Hack around "is not array" issue in OMV.
        $mntentref = $params['mntentref'];
        unset($params['mntentref']);

        $object = new ConfigObject($this->dataModelPath);
        $object->setAssoc($params);

        // Hack around "is not array" issue in OMV.
        $object->set('mntentref', $mntentref);

        $isNewObject = $object->isNew();

        if ($isNewObject) {
            $this->database->assertIsUnique($object, 'name');
        }

        // Save the object before generating a fstab entry to generate a valid
        // UUID.
        $this->database->set($object);

        // If it's a new entry we'll create a new mntent.
        if ($isNewObject) {
            $selfMntent = $this->createMntentEntry($object);

            try {
                $object->set('self_mntentref', $selfMntent['uuid']);

                $this->database->set($object);
            } catch (Exception $e) {
                $this->database->delete($object);
                Rpc::call('FsTab', 'delete', ['uuid' => $selfMntent['uuid']], $context);

                throw $e;
            }
        }

        // The fstab module does not mark itself as dirty when a mntent
        // configuration is modified, only when created or deleted.
        if (!$isNewObject) {
            $moduleManager = ModuleManager::getInstance();
            $moduleManager->setModuleDirty('fstab');
        }

        return $object->getAssoc();
    }

    /**
     * Delete the configuration of a single existing union filesystem.
     *
     * @param $params  The method parameters
     * @param $context The context of the caller
     *
     * @return void
     */
    public function delete($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');

        // FIXME: Check that the filesystem is not used before deleting it.
        $object = $this->database->get($this->dataModelPath, $params['uuid']);
        $this->database->delete($object);

        Rpc::call('FsTab', 'delete', ['uuid' => $object->get('self_mntentref')], $context);
    }

    /**
     * Create a mntent configuration object from a configuration object.
     *
     * NOTE: Currently this should only be used when creating a new file system
     * as it will calculate the mount point every time. This can in turn break
     * existing setups if the default mount point directory has changed.
     *
     * @param ConfigObject $object The object to create a mntent entry for
     *
     * @return string Returns the mntentref for the mntent entry
     */
    private function createMntentEntry(ConfigObject $object)
    {
        $mountPoint = MountPoint::buildPath($object->get('uuid'));

        $mntentEntry = [
            'uuid' => $object->get('self_mntentref'),
            'fsname' => $object->get('uuid'),
            'dir' => $mountPoint,
            'type' => 'fuse.mergerfs',
            'opts' => '',
            'freq' => 0,
            'passno' => 0,
        ];

        $mntent = Rpc::call('FsTab', 'set', $mntentEntry, $this->getAdminContext());

        return $mntent;
    }
}
