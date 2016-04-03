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
use OMV\AssertException;
use OMV\Config\ConfigObject;
use OMV\Config\Database;
use OMV\Engine\Module\Manager as ModuleManager;
use OMV\Engine\Notify\Dispatcher;
use OMV\Rpc\Rpc;
use OMV\Rpc\ServiceAbstract;
use OMV\System\Filesystem\Filesystem;
use OMV\System\Filesystem\Union as UnionFilesystem;
use OMV\Uuid;

class UnionFilesystems extends ServiceAbstract
{
    /** @var Database */
    private $database;

    /** @var string */
    private $dataModelPath = 'conf.service.unionfilesystems.pool';

    /** @var Dispatcher */
    private $dispatcher;

    public function __construct(Database $database = null, Dispatcher $dispatcher = null)
    {
        $this->database = $database ?: Database::getInstance();
        $this->dispatcher = $dispatcher ?: Dispatcher::getInstance();
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
            if (shell_exec('which '.$command)) {
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

        // Create a predicate to filter out all filesystems already used.
        $predicateFormat = './branches/mntentref[contains(.,\'%s\')]';

        if ($params['uuid'] !== $GLOBALS['OMV_UUID_UNDEFINED']) {
            // If we're editing a pool we don't want to exclude the filesystems
            // already in the pool.
            $predicateFormat = sprintf(
                'not(uuid=\'%s\') and ./branches/mntentref[contains(.,\'%%s\')]',
                $params['uuid']
            );
        }

        foreach ($entries as $entry) {
            $predicate = sprintf($predicateFormat, $entry['uuid']);

            // Filter out used filesystems.
            if ($this->database->existsByPredicate($this->dataModelPath, $predicate)) {
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

            foreach ($object->get('branches.mntentref') as $branch) {
                $branchInfo = 'n/a';
                $mntent = Rpc::call('FsTab', 'get', ['uuid' => $branch], $context);
                $filesystem = Filesystem::getImpl($mntent['fsname']);

                if ($filesystem && $filesystem->exists()) {
                    $branchInfo = $filesystem->getDeviceFile().' ('.$filesystem->getLabel().')';
                }

                $branchesInfo[] = $branchInfo;
            }

            $object->add('branches_info', 'any');
            $object->set('branches_info', $branchesInfo);

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

        $object->move('branches.mntentref', 'branches');

        // Merge the pool configuration with its own options array if the
        // options array exists.
        $properties = [
            'aufs_policy_create',
            'mergerfs_category_create',
            'mergerfs_min_free_space',
            'mhddfs_mlimit',
        ];

        foreach ($properties as $property) {
            $beforeProperty = 'options.'.$property;

            try {
                // Remove empty properties.
                if ($object->isEmpty($beforeProperty)) {
                    $object->remove($beforeProperty);
                    continue;
                }

                $object->move($beforeProperty, $property);
            } catch (AssertException $e) {
                // Property doesn't exist. Ignore the error.
            }
        }

        $object->remove('options');

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

        $options = [];

        foreach ([
            'aufs_policy_create',
            'mergerfs_category_create',
            'mergerfs_min_free_space',
            'mhddfs_mlimit',
        ] as $optionKey) {
            if (isset($params[$optionKey])) {
                $options[$optionKey] = $params[$optionKey];
            }
        }

        // Sanitize the mount options.
        if ($params['type'] === 'mergerfs') {
            $options = $this->sanitizeMergerfsOptions($options);
        }

        if ($params['type'] === 'mhddfs') {
            $options = $this->sanitizeMhddfsOptions($options);
        }

        $object = new ConfigObject($this->dataModelPath);
        $object->setAssoc([
            'uuid' => $params['uuid'],
            'name' => $params['name'],
            'type' => $params['type'],
            'mntentref' => $params['mntentref'],
            'options' => $options,
        ]);

        $object->set('branches.mntentref', $params['branches']);

        $isNewObject = $object->isNew();

        if ($isNewObject) {
            $this->database->assertIsUnique($object, 'name');
        }

        // Create or update the mntent entry.
        $mntent = $this->setMntentEntry($object);
        $object->set('mntentref', $mntent['uuid']);

        // Try to save the pool. If it fails and it's a new pool, remove the
        // newly created mntent configuration.
        try {
            $this->database->set($object);
        } catch (Exception $e) {
            if (!$isNewObject) {
                Rpc::call('FsTab', 'delete', ['uuid' => $mntent['uuid']], $context);
            }

            throw $e;
        }

        // The fstab module do not mark itself as dirty when a mntent
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

        Rpc::call('FsTab', 'delete', ['uuid' => $object->get('mntentref')], $context);
    }

    /**
     * Create or update a mntent configuration object from a pool.
     *
     * @param $pool The pool to create a mntent entry for.
     *
     * @return string Returns the mntentref for the mntent entry.
     */
    private function setMntentEntry(ConfigObject $pool)
    {
        $mountPoint = UnionFilesystem::buildMountPath($pool->get('uuid'));

        $mntentEntry = [
            'uuid' => $pool->get('mntentref'),
            'fsname' => $pool->get('uuid'),
            'dir' => $mountPoint,
            'type' => $pool->get('type'),
            'opts' => '',
            'freq' => 0,
            'passno' => 0,
        ];

        $mntent = Rpc::call('FsTab', 'set', $mntentEntry, $this->getAdminContext());

        return $mntent;
    }

    /**
     * Sanitize Mergerfs specific options.
     *
     * @param $options
     *
     * @return array
     */
    private function sanitizeMergerfsOptions(array $options)
    {
        $createPolicy = $options['mergerfs_category_create'];

        if ($createPolicy !== 'eplfs' &&
            $createPolicy !== 'epmfs' &&
            $createPolicy !== 'fwfs' &&
            $createPolicy !== 'lfs'
        ) {
            unset($options['mergerfs_min_free_space']);
        }

        return $options;
    }

    /**
     * Sanitize Mhddfs specific options.
     *
     * @param $options
     *
     * @return array
     */
    private function sanitizeMhddfsOptions(array $options)
    {
        $mlimitUnit = substr($options['mhddfs_mlimit'], -1);
        $mlimitValue = intval($options['mhddfs_mlimit']);

        // Set mlimit to the default value.
        $options['mhddfs_mlimit'] = '4G';

        if ($mlimitUnit === '%') {
            // Make sure the limit is not out of range.
            if ($mlimitValue > 100) {
                $options['mhddfs_mlimit'] = '100%';
            } elseif ($mlimitValue < 1) {
                $options['mhddfs_mlimit'] = '1%';
            }
        }

        if ($mlimitUnit === 'M') {
            // Make sure to not go below the 100MB limit.
            if ($mlimitValue < 100) {
                $options['mhddfs_mlimit'] = '100m';
            }
        }

        if ($mlimitUnit === 'G') {
            // 1GB is the lowest allowed when using GB.
            if ($mlimitValue < 1) {
                $options['mhddfs_mlimit'] = '1g';
            }
        }

        return $options;
    }
}
