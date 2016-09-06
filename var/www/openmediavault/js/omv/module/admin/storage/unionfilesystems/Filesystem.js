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
// require("js/omv/form/field/plugin/FieldInfo.js")
// require("js/omv/module/admin/storage/unionfilesystems/MergerfsCreatePolicyStore.js")

Ext.define('OMV.module.admin.storage.unionfilesystems.Filesystem', {
    extend: 'OMV.workspace.window.Form',
    requires: [
        'OMV.form.field.plugin.FieldInfo',
        'OMV.workspace.window.plugin.ConfigObject',
        'OMV.module.admin.storage.unionfilesystems.MergerfsCreatePolicyStore'
    ],

    plugins: [{
        ptype: 'configobject'
    }],

    hideResetButton: true,

    rpcService: 'UnionFilesystems',
    rpcGetMethod: 'get',
    rpcSetMethod: 'set',

    getFormItems: function() {
        return [{
            xtype: 'textfield',
            name: 'name',
            fieldLabel: _('Name'),
            allowBlank: false,
            readOnly: this.uuid !== OMV.UUID_UNDEFINED
        }, {
            xtype: 'hiddenfield',
            name: 'self-mntentref',
            value: OMV.UUID_UNDEFINED
        }, {
            xtype: 'checkboxgridfield',
            name: 'mntentref',
            fieldLabel: _('Branches'),
            valueField: 'uuid',
            minSelections: 2,
            flex: 1,
            store: Ext.create('OMV.data.Store', {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: 'uuid',
                    fields: [{
                        name: 'uuid',
                        type: 'string'
                    }, {
                        name: 'devicefile',
                        type: 'string'
                    }, {
                        name: 'dir',
                        type: 'string'
                    }, {
                        name: 'fsname',
                        type: 'string'
                    }, {
                        name: 'label',
                        type: 'string'
                    }, {
                        name: 'type',
                        type: 'string'
                    }]
                }),
                proxy: {
                    type: 'rpc',
                    appendSortParams: false,
                    rpcData: {
                        service: 'UnionFilesystems',
                        method: 'enumerateCandidates',
                        params: {
                            uuid: this.uuid
                        }
                    }
                },
                sorters: [{
                    direction: 'ASC',
                    property: 'devicefile'
                }]
            }),
            gridConfig: {
                columns: [{
                    text: _('Device'),
                    sortable: true,
                    dataIndex: 'devicefile',
                    flex: 1
                }, {
                    text: _('Label'),
                    sortable: true,
                    dataIndex: 'label',
                    flex: 1
                }, {
                    text: _('Type'),
                    sortable: true,
                    dataIndex: 'type',
                    flex: 1
                }]
            }
        }, {
            xtype: 'fieldset',
            title: _('Mount options'),
            defaults: {
                labelSeparator: ''
            },
            items: [{
                xtype: 'combo',
                name: 'create-policy',
                fieldLabel: _('Create policy'),
                queryMode: 'local',
                store: Ext.create('OMV.module.admin.storage.unionfilesystems.MergerfsCreatePolicyStore'),
                displayField: 'text',
                valueField: 'value',
                allowBlank: false,
                editable: false,
                triggerAction: 'all',
                value: 'epmfs'
            }, {
                xtype: 'textfield',
                name: 'min-free-space',
                fieldLabel: _('Minimum free space'),
                allowBlank: false,
                maskRe: /[0-9KMG]/,
                regex: /^[0-9]+[KMG]$/,
                value: '4G',
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('When the minimum free space is reached on a filesystem it will not be written to unless all the other filesystem also has reached the limit. Format: {amount}{unit}. Allows the units K, M and G.')
                }]
            }, {
                xtype: 'textfield',
                name: 'options',
                fieldLabel: _('Options'),
                value: 'defaults,allow_other'
            }]
        }];
    }
});
