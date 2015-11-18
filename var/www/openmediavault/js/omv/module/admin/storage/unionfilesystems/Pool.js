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
    }, {
        ptype: "linkedfields",
        correlations: [{
            conditions: [{
                name: "type",
                value: "aufs"
            }],
            name: ["aufs_policy_create"],
            properties: ["show"]
        }, {
            conditions: [{
                name: "type",
                value: "mergerfs"
            }],
            name: ["mergerfs_category_create"],
            properties: ["show"]
        }, {
            conditions: [{
                name: "type",
                value: "mergerfs"
            }, {
                name: "mergerfs_category_create",
                func: function(values) {
                    var value = values.mergerfs_category_create;

                    return value === "epmfs" || value === "fwfs" || value === "lfs";
                }
            }],
            name: ["mergerfs_min_free_space"],
            properties: ["show"]
        }, {
            conditions: [{
                name: "type",
                value: "mhddfs"
            }],
            name: ["mhddfs_mlimit"],
            properties: ["show"]
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
            emptyText: _("Select a type ..."),
            allowBlank: false,
            allowNone: false,
            editable: false,
            triggerAction: "all",
            displayField: "type",
            valueField: "type",
            store: Ext.create("OMV.data.Store", {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: "type",
                    fields: [{
                        name: "type",
                        type: "string"
                    }]
                }),
                proxy: {
                    type: "rpc",
                    rpcData: {
                        service: "UnionFilesystems",
                        method: "enumerateAvailableBackends"
                    },
                },
                sorters: [{
                    direction: "ASC",
                    property: "type"
                }]
            }),
            value: ""
        }, {
            xtype: "checkboxgridfield",
            name: "branches",
            fieldLabel: _("Branches"),
            valueField: "uuid",
            minSelections: 1,
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
                        method: "enumerateCandidates",
                        params: {
                            uuid: this.uuid
                        }
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
                }, {
                    text: _("Type"),
                    sortable: true,
                    dataIndex: "type",
                    flex: 1
                }]
            }
        }, {
            xtype: "hiddenfield",
            name: "mntentref",
            value: OMV.UUID_UNDEFINED
        }, {
            xtype: "fieldset",
            title: _("Mount options"),
            defaults: {
                labelSeparator: ""
            },
            items: [{
                xtype: "combo",
                name: "aufs_policy_create",
                fieldLabel: _("Create policy"),
                queryMode: "local",
                store: Ext.create("Ext.data.SimpleStore", {
                    fields: [
                        "value",
                        "text"
                    ],
                    data: [
                        ["mfs", _("mfs")],
                        ["pmfs", _("pmfs")],
                        ["rr", _("rr")],
                        ["tdp", _("tdp")]
                    ]
                }),
                displayField: "text",
                valueField: "value",
                allowBlank: false,
                editable: false,
                triggerAction: "all",
                value: "tdp"
            }, {
                xtype: "combo",
                name: "mergerfs_category_create",
                fieldLabel: _("Create policy"),
                queryMode: "local",
                store: Ext.create("Ext.data.SimpleStore", {
                    fields: [
                        "value",
                        "text"
                    ],
                    data: [
                        ["all", _("All.")],
                        ["epmfs", _("Existing path, most free space.")],
                        ["ff", _("First found.")],
                        ["ffwp", _("First found with permissions.")],
                        ["fwfs", _("First with free space.")],
                        ["lfs", _("Least free space.")],
                        ["mfs", _("Most free space.")],
                        ["newest", _("Newest file.")],
                        ["rand", _("Random.")]
                    ]
                }),
                displayField: "text",
                valueField: "value",
                allowBlank: false,
                editable: false,
                triggerAction: "all",
                value: "epmfs"
            }, {
                xtype: "textfield",
                name: "mergerfs_min_free_space",
                fieldLabel: _("Minimum free space"),
                allowBlank: false,
                maskRe: /[\dKMG]/,
                regex: /^\d+(K|M|G)$/,
                regexText: _("This field must have the format 4G, 100M or 1000K."),
                maxLength: 5,
                value: "4G"
            }, {
                xtype: "textfield",
                name: "mhddfs_mlimit",
                fieldLabel: _("Threshold"),
                allowBlank: false,
                maskRe: /[\dMG%]/,
                regex: /^\d+(M|G|%)$/,
                regexText: _("This field must have the format 4G, 100M, or 100%"),
                maxLength: 5,
                value: "4G",
                plugins: [{
                    ptype: "fieldinfo",
                    text: _("Units can be G, M, or %. If a drive has the free space less than the threshold specifed then another drive will be chosen while creating a new file.  If all the drives have free space less than the threshold specified then a drive containing most free space will be choosen.")
                }]
            }]
        }];
    }
});
