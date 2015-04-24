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

// require("js/omv/WorkspaceManager.js")
// require("js/omv/workspace/window/Form.js")
// require("js/omv/workspace/window/plugin/ConfigObject.js")
// require("js/omv/form/plugin/LinkedFields.js")

Ext.define("OMV.module.admin.storage.unionfilesystems.Pool", {
    extend: "OMV.workspace.window.Form",
    requires: [
        "OMV.workspace.window.plugin.ConfigObject",
        "OMV.form.plugin.LinkedFields"
    ],

    plugins: [{
        ptype: "configobject"
    },{
        ptype        : "linkedfields",
        correlations : [{
            conditions : [{
                name  : "type",
                value : "mhddfs"
            }],
            name       : [
                "mlimit"
            ],
            properties : [
                "show"
            ]
        }]
    }],

    rpcService: "UnionFilesystems",
    rpcGetMethod: "get",
    rpcSetMethod: "set",

    hideResetButton: true,

    getFormItems: function() {
        return [{
            xtype: "textfield",
            name: "name",
            fieldLabel: _("Name"),
            allowBlank: false
        }, {
            xtype: "combo",
            name: "type",
            fieldLabel: _("Type"),
            queryMode: "local",
            store: Ext.create("Ext.data.SimpleStore", {
                fields: [
                    "value",
                    "text"
                ],
                data: [
                    ["aufs", _("aufs")],
                    ["mhddfs", _("mhddfs")]
                ]
            }),
            displayField: "text",
            valueField: "value",
            allowBlank: false,
            editable: false,
            triggerAction: "all",
            value: "aufs"
        }, {
            xtype: "textfield",
            name: "mlimit",
            fieldLabel: _("Threshold"),
            allowBlank: false,
            maskRe: /[\dmg%]/,
            regex: /^\d+(m|g|%)$/,
            regexText: _("This field must have the format 4g, 100m, or 100%"),
            maxLength: 5,
            value: "4g",
            plugins: [{
                ptype: "fieldinfo",
                text: _("Units can be g, m, or %. If a drive has the free space less than the threshold specifed then another drive will be chosen while creating a new file.  If all the drives have free space less than the threshold specified then a drive containing most free space will be choosen.")
            }]
        }, {
            xtype: "checkboxgridfield",
            name: "branches",
            fieldLabel: _("Branches"),
            valueField: "uuid",
            minSelections: 2,
            flex: 1,
            store: Ext.create("OMV.data.Store", {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: "uuid",
                    fields: [{
                        name: "uuid",
                        type: "string"
                    }, {
                        name: "devicefile",
                        type: "string"
                    }, {
                        name: "dir",
                        type: "string"
                    }, {
                        name: "fsname",
                        type: "string"
                    }, {
                        name: "label",
                        type: "string"
                    }, {
                        name: "type",
                        type: "string"
                    }]
                }),
                proxy: {
                    type: "rpc",
                    appendSortParams: false,
                    rpcData: {
                        service: "UnionFilesystems",
                        method: "enumerateCandidates"
                    }
                },
                sorters: [{
                    direction: "ASC",
                    property: "devicefile"
                }]
            }),
            gridConfig: {
                columns: [{
                    text: _("Device"),
                    sortable: true,
                    dataIndex: "devicefile",
                    flex: 1
                }, {
                    text: _("Label"),
                    sortable: true,
                    dataIndex: "label",
                    flex: 1
                },{
                    text: _("Type"),
                    sortable: true,
                    dataIndex: "type",
                    flex: 1
                }]
            }
        }];
    }
});
