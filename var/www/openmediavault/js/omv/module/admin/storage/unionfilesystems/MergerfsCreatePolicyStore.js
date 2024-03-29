/**
 * Copyright (C) 2014-2022 OpenMediaVault Plugin Developers
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

Ext.define('OMV.module.admin.storage.unionfilesystems.MergerfsCreatePolicyStore', {
    extend: 'Ext.data.SimpleStore',
    fields: [
        'value',
        'text'
    ],
    data: [
        ['all', _('All.')],
        ['epall', _('Existing path, all.')],
        ['epff', _('Existing path, first found.')],
        ['eplfs', _('Existing path, least free space.')],
        ['eplus', _('Existing path, least used space.')],
        ['epmfs', _('Existing path, most free space.')],
        ['eprand', _('Existing path, random.')],
        ['erofs', _('Read-only.')],
        ['ff', _('First found.')],
        ['lfs', _('Least free space.')],
        ['lus', _('Least used space.')],
        ['mfs', _('Most free space.')],
        ['msplfs', _('Most shared path - least free space')],
        ['msplus', _('Most shared path - least used space')],
        ['mspmfs', _('Most shared path - most free space')],
        ['msppfrd', _('Most shared path - percentage free random distribution')],
        ['newest', _('Newest file.')],
        ['pfrd', _('Percentage free random distribution')],
        ['rand', _('Random.')]
    ]
});
