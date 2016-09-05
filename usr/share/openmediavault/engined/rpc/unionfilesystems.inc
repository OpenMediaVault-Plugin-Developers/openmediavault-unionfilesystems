<?php

/**
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers.
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
use OMV\Uuid;

class UnionFilesystems extends ServiceAbstract
{
    /** @var Database */
    private $database;

    /** @var string */
    private $dataModelPath = 'conf.service.unionfilesystems.pool';

    /** @var string */
    private $mountPointDataModelPath = 'conf.system.filesystem.mountpoint';

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
        $this->registerMethod('enumerateAvailableBackends');
        $this->registerMethod('enumerateCandidates');
        $this->registerMethod('getList');
        $this->registerMethod('get');
        $this->registerMethod('set');
        $this->registerMethod('delete');
    }

    /**
     * Enumerate the available backends on the system.
     *
     * @param $params  The method parameters.
     * @param $context The context of the caller.
     *
     * @return array
     */
    public function enumerateAvailableBackends($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);

        $backendCommands = [
            'aufs' => 'mount.aufs',
            'mergerfs' => 'mergerfs',
            'mhddfs' => 'mhddfs',
        ];

        $backends = [];

        foreach ($backendCommands as $backend => $command) {
            if (shell_exec('which ' . $command)) {
                $backends[] = ['type' => $backend];
            }
        }

        return $backends;
    }

    /**
     * Enumerate the available filesystem candidates that can be added to a
     * union filesystem.
     *
     * @param $params  The method parameters.
     * @param $context The context of the caller.
     *
     * @return array
     */
    public function enumerateCandidates($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');

        $entries = Rpc::call('FsTab', 'enumerateEntries', [], $context);

        $candidates = [];

        foreach ($entries as $entry) {
            // Create filter to exclude all filesystems already used.
            $filter = [
                'operator' => 'stringEquals',
                'arg0' => 'mntentref',
                'arg1' => $entry['uuid'],
            ];

            if ($params['uuid'] !== $GLOBALS['OMV_UUID_UNDEFINED']) {
                // If we're editing a pool we don't want to exclude the filesystems
                // already in the pool.
                $filter = [
                    'operator' => 'and',
                    'arg0' => [
                        'operator' => 'stringNotEquals',
                        'arg0' => 'uuid',
                        'arg1' => $params['uuid'],
                    ],
                    'arg1' => $filter,
                ];
            }

            // Filter out used filesystems.
            if ($this->database->exists($this->dataModelPath, $filter)) {
                continue;
            }

            // Exclude these types.
            if ($entry['type'] == 'aufs' ||
                $entry['type'] == 'mergerfs' ||
                $entry['type'] == 'mhddfs' ||
                $entry['type'] == 'none'
            ) {
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
     * @param $params  The method parameters.
     * @param $context The context of the caller.
     *
     * @return array
     */
    public function getList($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.getlist');

        $pools = [];

        foreach ($this->database->get($this->dataModelPath) as $object) {
            // Add additional information.
            $branchesInfo = [];

            foreach ($object->get('mntentref') as $mntentref) {
                $branchInfo = 'n/a';
                $mntent = Rpc::call('FsTab', 'get', ['uuid' => $mntentref], $context);
                $filesystem = Filesystem::getImpl($mntent['fsname']);

                if ($filesystem && $filesystem->exists()) {
                    $branchInfo = $filesystem->getDeviceFile() . ' (' . $filesystem->getLabel() . ')';
                }

                $branchesInfo[] = $branchInfo;
            }

            $object->add('source_mounts_info', 'any');
            $object->set('source_mounts_info', $branchesInfo);

            $pools[] = $object->getAssoc();
        }

        return $this->applyFilter(
            $pools,
            $params['start'],
            $params['limit'],
            $params['sortfield'],
            $params['sortdir']
        );
    }

    /**
     * Get the configuration for a single existing union filesystem.
     *
     * @param $params  The method parameters.
     * @param $context The context of the caller.
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
     * @param $params  The method parameters.
     * @param $context The context of the caller.
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

        // Create or update the mntent entry.
        $selfMntent = $this->setMntentEntry($object);

        // The fstab module does not mark itself as dirty when a mntent
        // configuration is modified, only when created or deleted.
        if (!$isNewObject) {
            $moduleManager = ModuleManager::getInstance();
            $moduleManager->setModuleDirty('fstab');
        }

        if ($isNewObject) {
            try {
                $object->set('self-mntentref', $selfMntent['uuid']);

                $this->database->set($object);
            } catch (Exception $e) {
                $this->database->delete($object);
                Rpc::call('FsTab', 'delete', ['uuid' => $selfMntent['uuid']], $context);

                throw $e;
            }
        }

        return $object->getAssoc();
    }

    /**
     * Delete the configuration of a single existing union filesystem.
     *
     * @param $params  The method parameters.
     * @param $context The context of the caller.
     *
     * @return void
     */
    public function delete($params, $context)
    {
        $this->validateMethodContext($context, ['role' => OMV_ROLE_ADMINISTRATOR]);
        $this->validateMethodParams($params, 'rpc.common.objectuuid');

        // FIXME: Check that pool is not used before deleting it.
        $object = $this->database->get($this->dataModelPath, $params['uuid']);
        $this->database->delete($object);

        Rpc::call('FsTab', 'delete', ['uuid' => $object->get('self-mntentref')], $context);
    }

    /**
     * Create or update a mntent configuration object from a pool.
     *
     * @param $pool The pool to create a mntent entry for.
     *
     * @return string Returns the mntentref for the mntent entry.
     */
    private function setMntentEntry(ConfigObject $object)
    {
        $mountPoint = Filesystem::buildMountPath($object->get('uuid'));

        $mntentEntry = [
            'uuid' => $object->get('self-mntentref'),
            'fsname' => $object->get('uuid'),
            'dir' => $mountPoint,
            'type' => $object->get('type'),
            'opts' => '',
            'freq' => 0,
            'passno' => 0,
        ];

        $mntent = Rpc::call('FsTab', 'set', $mntentEntry, $this->getAdminContext());

        return $mntent;
    }
}
