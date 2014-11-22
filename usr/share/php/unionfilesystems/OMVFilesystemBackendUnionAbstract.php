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

require_once "unionfilesystems/OMVFilesystemUnion.php";

abstract class OMVFilesystemBackendUnionAbstract extends OMVFilesystemBackendAbstract
{
    private function getPoolListXpathByType($type)
    {
        return sprintf(
            "/config/services/unionfilesystems/pools/pool[type='%s']",
            $type
        );
    }

    public function enumerate()
    {
        global $xmlConfig;

        $result = array();
        $pools = $xmlConfig->getList($this->getPoolListXpathByType($this->type));

        if ($pools) {
            foreach ($pools as $pool) {
                $deviceFile = OMVFilesystemUnion::buildMountPath($pool["uuid"]);

                $result[$deviceFile] = array(
                    "devicefile" => $deviceFile,
                    "uuid" => "",
                    "label" => "",
                    "type" => $this->type,
                );
            }
        }

        return $result;
    }

    public function isBlkidEnumerated()
    {
        return false;
    }

    public function isTypeOf($id)
    {
        $mounts = $this->enumerate();

        foreach ($mounts as $mount) {
            if ($mount["devicefile"] == $id) {
                return true;
            }
        }

        return false;
    }

    public function getImpl($args)
    {
        return new OMVFilesystemUnion($args);
    }
}
