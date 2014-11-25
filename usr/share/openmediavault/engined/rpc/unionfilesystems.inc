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
                $pools[] = $this->get(array("uuid" => $uuid), $context);
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
                    "branches": { "type": "string" }
                }
            }'
        );

        $pool = array(
            "uuid" => ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) ?
                OMVUtil::uuid() : $params["uuid"],
            "name" => $params["name"],
            "type" => $params["type"],
            "branches" => $params["branches"],
        );

        $mointPoint = OMVFilesystemUnion::buildMountPath($pool["uuid"]);

        // Create mntent entry
        $mntentEntry = array(
            "uuid" => $GLOBALS["OMV_UUID_UNDEFINED"],
            "fsname" => $mointPoint,
            "dir" => $mointPoint,
            "type" => $pool["type"],
            "opts" => "",
            "freq" => 0,
            "passno" => 0,
        );

        $mntent = OMVRpc::exec("FsTab", "set", $mntentEntry, $context);
        $pool["mntentref"] = $mntent["uuid"];

        // Save configuration
        $success = false;
        $xpath = null;

        if ($params["uuid"] == $GLOBALS["OMV_UUID_UNDEFINED"]) {
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

        $pool = $xmlConfig->delete($this->getPoolXpathByUuid($params["uuid"]));

        if (is_null($pool)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND,
                $this->getPoolXpathByUuid($params["uuid"])
            );
        }

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

            // The device is a potential candidate to be a member of a
            // union filesystem pool.
            $filesystem = OMVFilesystemFactory::get($entry["fsname"]);

            $result[] = array(
                "uuid" => $entry["uuid"],
                "devicefile" => $filesystem->getCanonicalDeviceFile(),
                "dir" => $entry["dir"],
                "fsname" => $entry["fsname"],
                "label" => $filesystem->getLabel(),
                "type" => $entry["type"],
            );
        }

        return $result;
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
                    "branches": { "type": "string" }
                }
            }'
        );

        $oldPool = $xmlConfig->get($this->getPoolXpathByUuid($params["uuid"]));

        if (is_null($oldPool)) {
            throw new OMVException(
                OMVErrorMsg::E_CONFIG_GET_OBJECT_FAILED,
                $this->getPoolXpathByUuid($params["uuid"])
            );
        }

        $pool = array(
            "uuid" => $params["uuid"],
            "name" => $oldPool["name"],
            "type" => $oldPool["type"],
            "branches" => sprintf(
                "%s,%s",
                $oldPool["branches"],
                $params["branches"]
            ),
            "mntentref" => $oldPool["mntentref"],
        );

        // Save configuration
        $success = false;

        $success = $xmlConfig->replace(
            $xpath = $this->getPoolXpathByUuid($params["uuid"]),
            $pool
        );

        if (!$success) {
            OMVRpc::exec("FsTab", "delete", array(
                "uuid" => $pool["mntentref"]
            ), $context);

            throw new OMVException(
                OMVErrorMsg::E_CONFIG_OBJECT_NOT_FOUND,
                $xpath
            );
        }

        // Notify configuration changes.
        $dispatcher = &OMVNotifyDispatcher::getInstance();
        $dispatcher->notify(
            OMV_NOTIFY_MODIFY,
            "org.openmediavault.system.fstab.mntent",
            $pool
        );

        return $pool;
    }
}

// Register the RPC service.
$rpcServiceMgr = &OMVRpcServiceMgr::getInstance();
$rpcServiceMgr->registerService(new OMVRpcServiceUnionFilesystems());
