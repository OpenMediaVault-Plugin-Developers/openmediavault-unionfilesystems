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
// require("js/omv/workspace/grid/Panel.js")
// require("js/omv/data/Store.js")
// require("js/omv/data/Model.js")
// require("js/omv/data/proxy/Rpc.js")
// require("js/omv/module/admin/storage/unionfilesystems/Pool.js")

Ext.define("OMV.module.admin.storage.unionfilesystems.Pools", {
    extend: "OMV.workspace.grid.Panel",
    requires: [
        "OMV.data.Store",
        "OMV.data.Model",
        "OMV.data.proxy.Rpc",
        "OMV.module.admin.storage.unionfilesystems.Pool"
    ],

    hideEditButton: true,
    hidePagingToolbar: false,
    reloadOnActivate: true,

    columns: [{
        header: _("UUID"),
        hidden: true,
        dataIndex: "uuid"
    }, {
        header: _("Name"),
        flex: 1,
        sortable: true,
        dataIndex: "name"
    }, {
        header: _("Branches"),
        flex: 1,
        sortable: true,
        dataIndex: "branches_info",
        renderer: function(value) {
            var template = Ext.create("Ext.XTemplate", '<tpl for=".">{.}<br/></tpl>');

            return template.apply(value);
        }
    }, {
        header: _("Type"),
        flex: 1,
        sortable: true,
        dataIndex: "type"
    }],

    store: Ext.create("OMV.data.Store", {
        autoLoad: true,
        model: OMV.data.Model.createImplicit({
            idProperty: "uuid",
            fields: [{
                name: "uuid",
                type: "string"
            }, {
                name: "name",
                type: "string"
            }, {
                name: "type",
                type: "string"
            }, {
                name: "branches_info",
                type: "array"
            }]
        }),
        proxy: {
            type: "rpc",
            rpcData: {
                "service": "UnionFilesystems",
                "method": "getList"
            }
        },
        remoteSort: true,
        sorters: [{
            direction: "ASC",
            property: "name"
        }]
    }),

    getTopToolbarItems: function() {
        var me = this;
        var items = me.callParent(arguments);
        Ext.Array.insert(items, 3, [{
            id: me.getId() + "-expand",
            xtype: "button",
            text: _("Expand"),
            icon: "images/expand.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            handler: Ext.Function.bind(this.onExpandButton, this),
            disabled: true,
            scope: this,
            selectionConfig: {
                minSelections: 1,
                maxSelections: 1
            }
        }, {
            id: me.getId() + "-remove-missing",
            xtype: "button",
            text: _("Remove missing"),
            icon: "images/minus.png",
            iconCls: Ext.baseCSSPrefix + "btn-icon-16x16",
            handler: Ext.Function.bind(this.onRemoveMissingButton, this),
            disabled: true,
            scope: this,
            selectionConfig: {
                minSelections: 1,
                maxSelections: 1
            }
        }]);
        return items;
    },

    onAddButton: function() {
        Ext.create("OMV.module.admin.storage.unionfilesystems.Pool", {
            title: _("Add pool"),
            uuid: OMV.UUID_UNDEFINED,
            listeners: {
                scope: this,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    doDeletion: function(record) {
        OMV.Rpc.request({
            scope: this,
            callback: this.onDeletion,
            rpcData: {
                service: "UnionFilesystems",
                method: "delete",
                params: {
                    uuid: record.get("uuid")
                }
            }
        });
    },

    onExpandButton: function() {
        var record = this.getSelected();

        Ext.create("OMV.module.admin.storage.unionfilesystems.Expand", {
            title: _("Expand pool"),
            uuid: record.get("uuid"),
            listeners: {
                scope: this,
                submit: function() {
                    this.doReload();
                }
            }
        }).show();
    },

    onRemoveMissingButton: function() {
        var msg = _("Are you sure that you really want to remove missing filesystems from this pool?") + " " +
            _("By clicking yes the system will try to find and remove the missing filesystems from the pool.");

        OMV.MessageBox.show({
            title: _("Confirmation"),
            msg: msg,
            buttons: Ext.Msg.YESNO,
            fn: function(answer) {
                if (answer !== "yes") {
                    return;
                }

                var record = this.getSelected();

                OMV.Rpc.request({
                    scope: this,
                    callback: function() {
                        this.doReload();
                    },
                    rpcData: {
                        service: "UnionFilesystems",
                        method: "removeMissingFilesystems",
                        params: {
                            uuid: record.get("uuid")
                        }
                    }
                });
            },
            scope: this,
            icon: Ext.Msg.QUESTION
        });
    }
});

OMV.WorkspaceManager.registerPanel({
    id: "pools",
    path: "/storage/unionfilesystems",
    text: _("Pools"),
    position: 30,
    className: "OMV.module.admin.storage.unionfilesystems.Pools"
});
