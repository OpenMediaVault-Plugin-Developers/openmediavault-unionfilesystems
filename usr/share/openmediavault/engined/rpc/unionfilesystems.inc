<?php

/**
 * Copyright (C) 2013-2014 OpenMediaVault Plugin Developers
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
require_once "openmediavault/object.inc";
require_once "openmediavault/rpc.inc";
require_once "openmediavault/rpcservice.inc";

class OMVRpcServiceUnionFilesystems extends OMVRpcServiceAbstract
{
    private function getXpath()
    {
        return sprintf("/config/services/unionfilesystems");
    }

    private function getPoolsXpath()
    {
        return sprintf("%s/pools", $this->getXpath());
    }

    private function getPoolXpathByUuid($uuid)
    {
        return sprintf("%s/pool[uuid='%s']", $this->getPoolsXpath(), $uuid);
    }

    public function getName()
    {
        return "UnionFilesystems";
    }

    public function initialize()
    {
        $this->registerMethod("getList");
        $this->registerMethod("get");
        $this->registerMethod("set");
        $this->registerMethod("delete");
        $this->registerMethod("enumerateCandidates");
        $this->registerMethod("setExpand");
        $this->registerMethod("removeMissingFilesystems");
    }

    public function getList($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

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

        // Get configuration object
        $xpath = sprintf("%s/pool/uuid", $this->getPoolsXpath());
        $uuids = $xmlConfig->getList($xpath);

        if (is_null($uuids)) {
            throw new OMVException(OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED, $xpath);
        }

        $pools = array();

        foreach ($uuids as $uuid) {
            try {
                $pool = $this->callMethod("get", array(
                    "uuid" => $uuid
                ), $context);

                // Add additional information.
                $branchesInfo = array();

                foreach ($pool["branches"] as $branch) {
                    $mntent = OMVRpc::exec("FsTab", "get", array(
                        "uuid" => $branch
                    ), $context);

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

    public function get($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

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
        $pool["branches"] = $pool["branches"]["mntentref"];

        if (is_null($pool)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getPoolXpathByUuid($params["uuid"])
            );
        }

        return $pool;
    }

    public function set($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "uuid": {' . $GLOBALS["OMV_JSONSCHEMA_UUID_UNDEFINED"] . '},
                    "name": { "type": "string" },
                    "type": { "type": "string", "enum": ["aufs", "mhddfs"] },
                    "branches": {
                        "type": "array",
                        "items": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '},
                        "minItems": 1,
                        "uniqueItems": true
                    },
                    "mntentref": { "type": "string", "optional" : true }
                }
            }'
        );

        $pool = array(
            "uuid" => ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) ?
                OMVUtil::uuid() : $params["uuid"],
            "name" => $params["name"],
            "type" => $params["type"],
        );

        if (isset($params["mntentref"])) {
            $pool["mntentref"] = $params["mntentref"];
        }

        $branches = array("mntentref" => array());

        foreach ($params["branches"] as $branch) {
            $branches["mntentref"][] = $branch;
        }

        $pool["branches"] = $branches;

        // Save configuration.
        $success = false;
        $xpath = null;

        if ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) {
            // Create the Mntent entry.
            $mntent = $this->createMntentEntry($pool);
            $pool["mntentref"] = $mntent["uuid"];

            $xpath = sprintf(
                "%s/pool[name='%s']",
                $this->getPoolsXpath(),
                $params["name"]
            );

            if ($xmlConfig->exists($xpath)) {
                throw new OMVException(
                    OMVErrorMsg::E_CONFIG_OBJECT_UNIQUENESS,
                    gettext("This name is already in use by another pool.")
                );
            }

            $success = $xmlConfig->set(
                $xpath = $this->getPoolsXpath(),
                array("pool" => $pool),
                $pool
            );
        } else {
            $this->createMntentEntry($pool, $pool["mntentref"]);

            $success = $xmlConfig->replace(
                $xpath = $this->getPoolXpathByUuid($params["uuid"]),
                $pool
            );
        }

        if (!$success) {
            OMVRpc::exec("FsTab", "delete", array(
                "uuid" => $pool["mntentref"]
            ), $context);

            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND,
                $xpath
            );
        }

        return $pool;
    }

    public function setExpand($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        // Validate the parameters of the RPC service method.
        $this->validateMethodParams(
            $params,
            '{
                "type": "object",
                "properties": {
                    "uuid": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '},
                    "branches": {
                        "type": "array",
                        "items": {' . $GLOBALS["OMV_JSONSCHEMA_UUID"] . '},
                        "minItems": 1,
                        "uniqueItems": true
                    }
                }
            }'
        );

        $pool = $this->callMethod("get", array(
            "uuid" => $params["uuid"]
        ), $context);

        $branches = array_merge(
            $pool["branches"],
            $params["branches"]
        );

        $pool["branches"] = $branches;

        return $this->callMethod("set", $pool, $context);
    }

    public function delete($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

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

        $mntent = OMVRpc::exec("FsTab", "delete", array(
            "uuid" => $pool["mntentref"]
        ), $context);
    }

    public function enumerateCandidates($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

        $entries = OMVRpc::exec("FsTab", "enumerateEntries", array(), $context);

        // Prepare the result list.
        $result = array();

        foreach ($entries as $entry) {
            // Check if device is referenced/used by a branch.
            $xpath = sprintf("//branches[contains(.,'%s')]", $entry["uuid"]);

            if ($xmlConfig->exists($xpath)) {
                continue;
            }

            if ($entry["type"] == "aufs" ||
                $entry["type"] == "mhddfs" ||
                $entry["type"] == "none"
            ) {
                continue;
            }

            $filesystem = OMVFilesystemFactory::get($entry["fsname"]);

            if (!is_null($filesystem) && $filesystem->exists()) {
                // The device is a potential candidate to be a member of a
                // union filesystem pool.
                $result[] = array(
                    "uuid" => $entry["uuid"],
                    "devicefile" => $filesystem->getDeviceFile(),
                    "dir" => $entry["dir"],
                    "fsname" => $entry["fsname"],
                    "label" => $filesystem->getLabel(),
                    "type" => $entry["type"],
                );
            }
        }

        return $result;
    }

    public function removeMissingFilesystems($params, $context)
    {
        global $xmlConfig;

        // Validate the RPC caller context.
        $this->validateMethodContext($context, array(
            "role" => OMV_ROLE_ADMINISTRATOR
        ));

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
        $branches = array();

        foreach ($pool["branches"] as $branch) {
            $mntent = OMVRpc::exec("FsTab", "get", array(
                "uuid" => $branch
            ), $context);

            $filesystem = OMVFilesystemFactory::get($mntent["fsname"]);

            if (!is_null($filesystem) && $filesystem->exists()) {
                $branches[] = $branch;
            }
        }

        // If the branches are the same, don't do anything.
        if ($branches == $pool["branches"]) {
            return $pool;
        }

        // If the remaining branches isn't enough to create a pool
        // try to delete the pool.
        if (count($branches) < 2) {
            return $this->callMethod("delete", $params, $context);
        }

        $pool["branches"] = $branches;

        return $this->callMethod("set", $pool, $context);
    }

    private function createMntentEntry($pool, $mntentref = null)
    {
        if (!$mntentref) {
            $mntentref = $GLOBALS["OMV_UUID_UNDEFINED"];
        }

        $mountPoint = OMVFilesystemUnion::buildMountPath($pool["uuid"]);

        $mntentEntry = array(
            "uuid" => $mntentref,
            "fsname" => $mountPoint,
            "dir" => $mountPoint,
            "type" => $pool["type"],
            "opts" => "",
            "freq" => 0,
            "passno" => 0,
        );

        $mntent = OMVRpc::exec(
            "FsTab",
            "set",
            $mntentEntry,
            $this->getAdminContext()
        );

        return $mntent;
    }

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
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceUnionFilesystems());
