<?php

/**
 * Copyright (C) 2013-2015 OpenMediaVault Plugin Developers
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
require_once "openmediavault/error.inc";
require_once "openmediavault/module.inc";
require_once "openmediavault/rpc.inc";
require_once "openmediavault/rpcservice.inc";

class OMVRpcServiceUnionFilesystems extends OMVRpcServiceAbstract
{
    /**
     * Get the service configuration XPath.
     *
     * @return string
     */
    private function getXpath()
    {
        return "/config/services/unionfilesystems";
    }

    /**
     * Get the service configuration base XPath for pools.
     *
     * @return string
     */
    private function getPoolsXpath()
    {
        return sprintf("%s/pools", $this->getXpath());
    }

    /**
     * Get the service configuration XPath for a single pool by its UUID.
     *
     * @param string $uuid
     * @return string
     */
    private function getPoolXpathByUuid($uuid)
    {
        return sprintf("%s/pool[uuid='%s']", $this->getPoolsXpath(), $uuid);
    }

    /**
     * Get the name of the RPC service.
     *
     * @return string
     */
    public function getName()
    {
        return "UnionFilesystems";
    }

    /**
     * Initialize the RPC service.
     *
     * @return void
     */
    public function initialize()
    {
        $this->registerMethod("enumerateAvailableBackends");
        $this->registerMethod("enumerateCandidates");
        $this->registerMethod("getList");
        $this->registerMethod("get");
        $this->registerMethod("set");
        $this->registerMethod("delete");
    }

    /**
     * Enumerate the available backends on the system.
     *
     * @param array $params The method parameters.
     * @param array $context The context of the caller.
     * @return array
     */
    public function enumerateAvailableBackends($params, $context)
    {
        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        $backendCommands = [
            "aufs" => "mount.aufs",
            "mergerfs" => "mergerfs",
            "mhddfs" => "mhddfs",
        ];

        $backends = [];

        foreach ($backendCommands as $backend => $command) {
            if (shell_exec("which $command")) {
                $backends[] = ["type" => $backend];
            }
        }

        return $backends;
    }

    /**
     * Enumerate the available filesystem candidates that can be added to a
     * union filesystem.
     *
     * @param array $params The method parameters.
     * @param array $context The context of the caller.
     * @return mixed
     */
    public function enumerateCandidates($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        $entries = OMVRpc::exec("FsTab", "enumerateEntries", [], $context);

        // Prepare the result list.
        $result = [];

        // Set the correct XPath filter to filter out candidates.
        $xpathFormat = sprintf(
            "%s//pool//branches[contains(.,'%%s')]",
            $this->getPoolsXpath()
        );

        if ($params["uuid"] != $GLOBALS["OMV_UUID_UNDEFINED"]) {
            // If we're editing a pool we want to include the filesystems
            // already in the pool.
            $xpathFormat = sprintf(
                "%s//pool[not(uuid='%s')]//branches[contains(.,'%%s')]",
                $this->getPoolsXpath(),
                $params["uuid"]
            );
        }

        foreach ($entries as $entry) {
            // Check if the device is already referenced/used by a branch.
            $xpath = sprintf($xpathFormat, $entry["uuid"]);

            if ($xmlConfig->exists($xpath)) {
                continue;
            }

            if ($entry["type"] == "aufs" ||
                $entry["type"] == "mergerfs" ||
                $entry["type"] == "mhddfs" ||
                $entry["type"] == "none"
            ) {
                continue;
            }

            $filesystem = OMVFilesystemFactory::get($entry["fsname"]);

            if (!is_null($filesystem) && $filesystem->exists()) {
                // The device is a potential candidate to be a member of a union
                // filesystem pool.
                $result[] = [
                    "uuid" => $entry["uuid"],
                    "devicefile" => $filesystem->getDeviceFile(),
                    "dir" => $entry["dir"],
                    "fsname" => $entry["fsname"],
                    "label" => $filesystem->getLabel(),
                    "type" => $entry["type"],
                ];
            }
        }

        return $result;
    }

    /**
     * Get a list of the existing union filesystems.
     *
     * @param array $params The method parameters.
     * @param array $context The context of the caller.
     * @return array
     *
     * @throws OMVException
     */
    public function getList($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "start": { "type": "integer" },
                    "limit": {' . $GLOBALS["OMV_JSONSCHEMA_COUNTFIELD"] . '},
                    "sortfield": {' . $GLOBALS["OMV_JSONSCHEMA_SORTFIELD"] . '},
                    "sortdir": {' . $GLOBALS["OMV_JSONSCHEMA_SORTDIR"] . '}
                }
            }'
        );

        // Get the configuration object.
        $xpath = sprintf("%s/pool/uuid", $this->getPoolsXpath());
        $uuids = $xmlConfig->getList($xpath);

        if (is_null($uuids)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
        }

        $pools = [];

        foreach ($uuids as $uuid) {
            try {
                $pool = $this->callMethod("get", [
                    "uuid" => $uuid
                ], $context);

                // Add additional information.
                $branchesInfo = [];

                foreach ($pool["branches"] as $branch) {
                    $mntent = OMVRpc::exec("FsTab", "get", [
                        "uuid" => $branch
                    ], $context);

                    $filesystem = OMVFilesystemFactory::get($mntent["fsname"]);

                    $branchInfo = "n/a";

                    if (!is_null($filesystem) && $filesystem->exists()) {
                        $branchInfo = sprintf(
                            "%s (%s)",
                            $filesystem->getDeviceFile(),
                            $filesystem->getLabel()
                        );
                    }

                    $branchesInfo[] = $branchInfo;
                }

                $pool["branches_info"] = $branchesInfo;

                $pools[] = $pool;

            } catch (Exception $e) {
                throw $e;
            }
        }

        return $this->applyFilter(
            $pools,
            $params["start"],
            $params["limit"],
            $params["sortfield"],
            $params["sortdir"]
        );
    }

    /**
     * Get the configuration for a single existing union filesystem.
     *
     * @param array $params The method parameters.
     * @param array $context The context of the caller.
     * @return mixed
     *
     * @throws OMVException
     */
    public function get($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "uuid": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '}
                }
            }'
        );

        $pool = $xmlConfig->get($this->getPoolXpathByUuid($params["uuid"]));

        if (is_null($pool)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getPoolXpathByUuid($params["uuid"])
            );
        }

        $branches = $pool["branches"]["mntentref"];

        // If we have no branches left, which could happen when removing all
        // missing filesystems, just set branches to an empty array.
        if (empty($branches)) {
            $branches = [];
        }

        // If there's only one branch in the pool, chances are that it's being
        // treated as a string, add it to an array.
        if (is_string($branches)) {
            $branches = [$branches];
        }

        $pool["branches"] = $branches;

        // Merge the pool configuration with its own options array if the
        // options array exists.
        if (isset($pool["options"])) {
            $options = $pool["options"];
            unset($pool["options"]);
            $pool = array_merge($pool, $options);
        }

        return $pool;
    }

    /**
     * Set the configuration for a single new or existing union filesystem.
     *
     * @param array $params The method parameters.
     * @param array $context The context of the caller.
     * @return mixed
     *
     * @throws OMVException
     */
    public function set($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "uuid": {' . $GLOBALS["OMV_JSONSCHEMA_UUID_UNDEFINED"] . '},
                    "name": { "type": "string" },
                    "type": {
                        "type": "string",
                        "enum": [
                            "aufs",
                            "mergerfs",
                            "mhddfs"
                        ]
                    },
                    "branches": {
                        "type": "array",
                        "items": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '},
                        "minItems": 1,
                        "uniqueItems": true,
                        "optional": true
                    },
                    "mntentref": {' . $GLOBALS["OMV_JSONSCHEMA_UUID_UNDEFINED"] . '},
                    "aufs_policy_create": {
                        "type": "string",
                        "enum": [
                            "mfs",
                            "pmfs",
                            "rr",
                            "tdp"
                        ],
                        "optional": true
                    },
                    "mergerfs_category_create": {
                        "type": "string",
                        "enum": [
                            "all",
                            "eplfs",
                            "epmfs",
                            "erofs",
                            "ff",
                            "fwfs",
                            "lfs",
                            "mfs",
                            "newest",
                            "rand"
                        ],
                        "optional": true
                    },
                    "mergerfs_min_free_space": {
                        "type": "string",
                        "format": "regex",
                        "pattern": "\\/^\\\d+(K|M|G)$\\/",
                        "optional": true
                    },
                    "mhddfs_mlimit": {
                        "type": "string",
                        "format": "regex",
                        "pattern": "\\/^\\\d+(M|G|%)$\\/",
                        "optional": true
                    }
                }
            }'
        );

        $options = [];

        // Sanitize the mount options and add them to an options array in the
        // configuration.
        if ($params["type"] === "aufs") {
            $options["aufs_policy_create"] = $params["aufs_policy_create"];
        }

        if ($params["type"] === "mergerfs") {
            $params = $this->sanitizeMergerfsOptions($params);

            $options["mergerfs_category_create"] = $params["mergerfs_category_create"];

            if (isset($params["mergerfs_min_free_space"])) {
                $options["mergerfs_min_free_space"] = $params["mergerfs_min_free_space"];
            }
        }

        if ($params["type"] === "mhddfs") {
            $params = $this->sanitizeMhddfsOptions($params);

            $options["mhddfs_mlimit"] = $params["mhddfs_mlimit"];
        }

        // Create the configuration array.
        $pool = [
            "uuid" => ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) ?
                OMVUtil::uuid() : $params["uuid"],
            "name" => $params["name"],
            "type" => $params["type"],
            "mntentref" => $params["mntentref"],
            "options" => $options,
        ];

        // Add an empty branches list if the branches option is not set.
        $branches = [];

        // In case the branches option is set, add the branches as mntentrefs.
        if (!empty($params["branches"])) {
            $branches["mntentref"] = [];

            foreach ($params["branches"] as $branch) {
                $branches["mntentref"][] = $branch;
            }
        }

        $pool["branches"] = $branches;

        // Validate the name.
        $nameXpath = sprintf(
            "%s/pool[not(uuid='%s') and name='%s']",
            $this->getPoolsXpath(),
            $pool["uuid"],
            $params["name"]
        );

        if ($xmlConfig->exists($nameXpath)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS,
                gettext("This name is already in use by another pool.")
            );
        }

        // Create or update the mntent entry.
        $mntent = $this->setMntentEntry($pool);
        $pool["mntentref"] = $mntent["uuid"];

        // Save the configuration.
        $success = false;
        $xpath = null;

        if ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) {
            $success = $xmlConfig->set(
                $xpath = $this->getPoolsXpath(),
                ["pool" => $pool],
                $pool
            );
        } else {
            $success = $xmlConfig->replace(
                $xpath = $this->getPoolXpathByUuid($params["uuid"]),
                $pool
            );

            $moduleManager = OMVModuleMgr::getInstance();
            $moduleManager->setModuleDirty("fstab");
        }

        if (!$success) {
            OMVRpc::exec("FsTab", "delete", [
                "uuid" => $pool["mntentref"]
            ], $context);

            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND,
                $xpath
            );
        }

        return $pool;
    }

    /**
     * Delete the configuration of a single existing union filesystem.
     *
     * @param array $params The method parameters.
     * @param array $context The context of the caller.
     * @return mixed
     *
     * @throws OMVException
     */
    public function delete($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, [
            "role" => OMV_ROLE_ADMINISTRATOR
        ]);

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "uuid": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '}
                }
            }'
        );

        $pool = $this->callMethod("get", $params, $context);

        if ($this->isUsed($pool)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_OBJECT_INUSE);
        }

        $xmlConfig->delete($this->getPoolXpathByUuid($params["uuid"]));

        $mntent = OMVRpc::exec("FsTab", "delete", [
            "uuid" => $pool["mntentref"]
        ], $context);
    }

    /**
     * Create or update a mntent configuration object from a pool.
     *
     * @param array $pool The pool to create a mntent entry for.
     * @return string Returns the mntentref for the mntent entry.
     */
    private function setMntentEntry($pool)
    {
        $mountPoint = OMVFilesystemUnion::buildMountPath($pool["uuid"]);

        $mntentEntry = [
            "uuid" => $pool["mntentref"],
            "fsname" => $pool["uuid"],
            "dir" => $mountPoint,
            "type" => $pool["type"],
            "opts" => "",
            "freq" => 0,
            "passno" => 0,
        ];

        $mntent = OMVRpc::exec(
            "FsTab",
            "set",
            $mntentEntry,
            $this->getAdminContext()
        );

        return $mntent;
    }

    /**
     * Check if the provided pool is in use.
     *
     * @param array $pool
     * @return bool
     */
    private function isUsed($pool)
    {
        global $xmlConfig;

        $xpath = sprintf(
            "//mntentref[contains(.,'%s') and not(../uuid/text()='%s')]",
            $pool["mntentref"],
            $pool["uuid"]
        );

        if ($xmlConfig->exists($xpath)) {
            return true;
        }

        return false;
    }

    /**
     * Sanitize Mergerfs specific options.
     *
     * @param array $params
     * @return array
     */
    private function sanitizeMergerfsOptions(array $params)
    {
        $createPolicy = $params["category_create"];

        if ($createPolicy !== "eplfs" &&
            $createPolicy !== "epmfs" &&
            $createPolicy !== "fwfs" &&
            $createPolicy !== "lfs"
        ) {
            unset($params["min_free_space"]);
        }

        return $params;
    }

    /**
     * Sanitize Mhddfs specific options.
     *
     * @param array $params
     * @return array
     */
    private function sanitizeMhddfsOptions(array $params)
    {
        $mlimitUnit = substr($params["mlimit"], -1);
        $mlimitValue = intval($params["mlimit"]);

        // Set mlimit to the default value.
        $params["mlimit"] = "4G";

        if ($mlimitUnit === "%") {
            // Make sure the limit is not out of range.
            if ($mlimitValue > 100) {
                $params["mlimit"] = "100%";
            } elseif ($mlimitValue < 1) {
                $params["mlimit"] = "1%";
            }
        }

        if ($mlimitUnit === "M") {
            // Make sure to not go below the 100MB limit.
            if ($mlimitValue < 100) {
                $params["mlimit"] = "100m";
            }
        }

        if ($mlimitUnit === "G") {
            // 1GB is the lowest allowed when using GB.
            if ($mlimitValue < 1) {
                $params["mlimit"] = "1g";
            }
        }

        return $params;
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceUnionFilesystems());
