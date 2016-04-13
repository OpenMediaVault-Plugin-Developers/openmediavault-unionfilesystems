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
// require("js/omv/form/field/plugin/FieldInfo.js")
// require("js/omv/module/admin/storage/unionfilesystems/AufsCreatePolicyStore.js")
// require("js/omv/module/admin/storage/unionfilesystems/MergerfsCreatePolicyStore.js")

Ext.define('OMV.module.admin.storage.unionfilesystems.Pool', {
    extend: 'OMV.workspace.window.Form',
    requires: [
        'OMV.form.field.plugin.FieldInfo',
        'OMV.form.plugin.LinkedFields',
        'OMV.workspace.window.plugin.ConfigObject',
        'OMV.module.admin.storage.unionfilesystems.AufsCreatePolicyStore',
        'OMV.module.admin.storage.unionfilesystems.MergerfsCreatePolicyStore'
    ],

    plugins: [{
        ptype: 'configobject'
    }, {
        ptype: 'linkedfields',
        correlations: [{
            conditions: [{
                name: 'type',
                value: 'aufs'
            }],
            name: ['min-free-space'],
            properties: ['!show', '!submitValue']
        }, {
            conditions: [{
                name: 'type',
                value: 'mhddfs'
            }],
            name: ['create-policy'],
            properties: ['!show', '!submitValue']
        }]
    }],

    hideResetButton: true,

    rpcService: 'UnionFilesystems',
    rpcGetMethod: 'get',
    rpcSetMethod: 'set',

    aufsPolicyStore: Ext.create('OMV.module.admin.storage.unionfilesystems.AufsCreatePolicyStore'),
    aufsPolicyStoreDefaultValue: 'tdp',
    mergerfsPolicyStore: Ext.create('OMV.module.admin.storage.unionfilesystems.MergerfsCreatePolicyStore'),
    mergerfsPolicyStoreDefaultValue: 'epmfs',
    mergerfsMinFreeSpaceRegex: /^[0-9]+[KMG]$/,
    mhddfsMinFreeSpaceRegex: /^((100|[1-9][0-9]?)%|[1-9][0-9]*[KMG])$/,

    getFormItems: function() {
        return [{
            xtype: 'textfield',
            name: 'name',
            fieldLabel: _('Name'),
            allowBlank: false,
            readOnly: this.uuid !== OMV.UUID_UNDEFINED
        }, {
            xtype: 'combo',
            name: 'type',
            fieldLabel: _('Type'),
            emptyText: _('Select a type ...'),
            allowBlank: false,
            allowNone: false,
            editable: false,
            triggerAction: 'all',
            displayField: 'type',
            valueField: 'type',
            store: Ext.create('OMV.data.Store', {
                autoLoad: true,
                model: OMV.data.Model.createImplicit({
                    idProperty: 'type',
                    fields: [{
                        name: 'type',
                        type: 'string'
                    }]
                }),
                proxy: {
                    type: 'rpc',
                    rpcData: {
                        service: 'UnionFilesystems',
                        method: 'enumerateAvailableBackends'
                    },
                },
                sorters: [{
                    direction: 'ASC',
                    property: 'type'
                }]
            }),
            listeners: {
                change: this.onTypeChange.bind(this),
                scope: this
            },
            value: 'mergerfs'
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
                store: this.mergerfsPolicyStore,
                displayField: 'text',
                valueField: 'value',
                allowBlank: false,
                editable: false,
                hidden: true,
                triggerAction: 'all',
                value: this.mergerfsPolicyStoreDefaultValue
            }, {
                xtype: 'textfield',
                name: 'min-free-space',
                fieldLabel: _('Minimum free space'),
                allowBlank: false,
                hidden: true,
                maskRe: /[0-9KMG%]/,
                regex: this.mergerfsMinFreeSpaceRegex,
                value: '4G',
                plugins: [{
                    ptype: 'fieldinfo',
                    text: _('When the minimum free space is reached on a filesystem it will not be written to unless all the other filesystem also has reached the limit. Format: {amount}{unit}. Mergerfs allows the units K, M and G. Mhddfs also allows %.')
                }]
            }, {
                xtype: 'textfield',
                name: 'options',
                fieldLabel: _('Options')
            }]
        }];
    },

    onTypeChange: function(combo, newValue, oldValue) {
        var createPolicy = this.findField('create-policy');
        var minFreeSpace = this.findField('min-free-space');
        var options = this.findField('options');

        if (newValue === 'aufs') {
            createPolicy.setStore(this.aufsPolicyStore);
            createPolicy.setValue(this.aufsPolicyStoreDefaultValue);
            options.setValue('sum');
        }

        if (newValue === 'mergerfs') {
            createPolicy.setStore(this.mergerfsPolicyStore);
            createPolicy.setValue(this.mergerfsPolicyStoreDefaultValue);
            minFreeSpace.regex = this.mergerfsMinFreeSpaceRegex;
            minFreeSpace.setValue('4G');
            options.setValue('defaults,allow_other');
        }

        if (newValue === 'mhddfs') {
            minFreeSpace.regex = this.mhddfsMinFreeSpaceRegex;
            minFreeSpace.setValue('4G');
            options.setValue('defaults,allow_other');
        }
    }
});
