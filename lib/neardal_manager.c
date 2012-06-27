/*
 *     NEARDAL (Neard Abstraction Library)
 *
 *     Copyright 2012 Intel Corporation. All rights reserved.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU Lesser General Public License version 2
 *     as published by the Free Software Foundation.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software Foundation,
 *     Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>

#include "neard_manager_proxy.h"
#include "neard_adapter_proxy.h"

#include "neardal.h"
#include "neardal_prv.h"

/******************************************************************************
 * neardal_mgr_prv_cb_property_changed: Callback called when a NFC Manager
 * Property is changed
 *****************************************************************************/
static void neardal_mgr_prv_cb_property_changed(DBusGProxy  *proxy,
						const gchar *arg_unnamed_arg0,
						GVariant *arg_unnamed_arg1,
						void        *user_data)
{
	NEARDAL_TRACEIN();

	g_assert(arg_unnamed_arg0 != NULL);
	(void) proxy; /* remove warning */
	(void) arg_unnamed_arg1; /* remove warning */
	(void) user_data; /* remove warning */

	NEARDAL_TRACEF("arg_unnamed_arg0='%s'\n", arg_unnamed_arg0);
	NEARDAL_TRACEF("arg_unnamed_arg1=%s\n",
		       g_variant_print (arg_unnamed_arg1, TRUE));
	/* Adapters List ignored... */
}

/******************************************************************************
 * neardal_mgr_prv_cb_adapter_added: Callback called when a NFC adapter is
 * added
 *****************************************************************************/
static void neardal_mgr_prv_cb_adapter_added(DBusGProxy *proxy,
					     const gchar *arg_unnamed_arg0,
					     void        *user_data)
{
	errorCode_t	err = NEARDAL_SUCCESS;

	NEARDAL_TRACEIN();
	g_assert(arg_unnamed_arg0 != NULL);
	(void) proxy; /* remove warning */
	(void) user_data; /* remove warning */

	err = neardal_adp_add((char *) arg_unnamed_arg0);
	if (err != NEARDAL_SUCCESS)
		return;

	NEARDAL_TRACEF("NEARDAL LIB adapterList contains %d elements\n",
		      g_list_length(neardalMgr.prop.adpList));
}

/******************************************************************************
 * neardal_mgr_prv_cb_adapter_removed: Callback called when a NFC adapter
 * is removed
 *****************************************************************************/
static void neardal_mgr_prv_cb_adapter_removed(DBusGProxy *proxy,
					       const gchar *arg_unnamed_arg0,
					       void *user_data)
{
	GList	*node	= NULL;

	NEARDAL_TRACEIN();
	g_assert(arg_unnamed_arg0 != NULL);
	(void) proxy; /* remove warning */
	(void) user_data; /* remove warning */

	node = g_list_first(neardalMgr.prop.adpList);
	if (node == NULL) {
		NEARDAL_TRACE_ERR("NFC adapter not found! (%s)\n",
				  arg_unnamed_arg0);
		return;
	}

	/* Invoke client cb 'adapter removed' */
	if (neardalMgr.cb_adp_removed != NULL)
		(neardalMgr.cb_adp_removed)((char *) arg_unnamed_arg0,
					 neardalMgr.cb_adp_removed_ud);

	neardal_adp_remove(((AdpProp *)node->data));

	NEARDAL_TRACEF("NEARDAL LIB adapterList contains %d elements\n",
		      g_list_length(neardalMgr.prop.adpList));
}

/******************************************************************************
 * neardal_mgr_prv_get_all_adapters: Check if neard has an adapter
 *****************************************************************************/
static errorCode_t neardal_mgr_prv_get_all_adapters(GPtrArray **adpArray,
						    gsize *len)
{
	GHashTable	*neardAdapterHash	= NULL;
	GPtrArray	*pathsGpa		= NULL;
	errorCode_t	err			= NEARDAL_ERROR_NO_ADAPTER;

	g_assert(adpArray != NULL);

	/* Invoking method 'GetProperties' on Neard Manager */
	if (org_neard_Manager_get_properties(neardalMgr.proxy,
					     &neardAdapterHash,
					     &neardalMgr.gerror)) {
		/* Receiving a GPtrArray of GList */
		NEARDAL_TRACEF("Parsing neard adapters...\n");

		err = neardal_tools_prv_hashtable_get(neardAdapterHash,
						   NEARD_MGR_SECTION_ADAPTERS,
					DBUS_TYPE_G_ARRAY_OF_OBJECT_PATH,
					&pathsGpa);
		if (err != NEARDAL_SUCCESS || pathsGpa == NULL)
			err = NEARDAL_ERROR_NO_ADAPTER;
		else {
			neardal_tools_prv_g_ptr_array_copy(adpArray, pathsGpa);
		}
		if (len != NULL)
			*len = pathsGpa->len;

		g_hash_table_destroy(neardAdapterHash);
	} else {
		err = NEARDAL_ERROR_DBUS_CANNOT_INVOKE_METHOD;
		NEARDAL_TRACE_ERR("%d:%s\n", neardalMgr.gerror->code,
				 neardalMgr.gerror->message);
		neardal_tools_prv_free_gerror(&neardalMgr.gerror);
	}

	return err;
}


/******************************************************************************
 * neardal_mgr_prv_get_adapter: Get NFC Adapter from name
 *****************************************************************************/
errorCode_t neardal_mgr_prv_get_adapter(gchar *adpName, AdpProp **adpProp)
{
	errorCode_t	err	= NEARDAL_ERROR_NO_ADAPTER;
	guint		len	= 0;
	AdpProp		*adapter;
	GList		*tmpList;

	g_assert(adpProp != NULL);

	tmpList = neardalMgr.prop.adpList;
	while (len < g_list_length(tmpList)) {
		adapter = g_list_nth_data(tmpList, len);
		if (adapter != NULL) {
			if (neardal_tools_prv_cmp_path(adapter->name,
							adpName)) {
				*adpProp = adapter;
				err = NEARDAL_SUCCESS;
				break;
			}
		}
		len++;
	}

	return err;
}

/******************************************************************************
 * neardal_mgr_prv_get_adapter_from_proxy: Get NFC Adapter from proxy
 *****************************************************************************/
errorCode_t neardal_mgr_prv_get_adapter_from_proxy(DBusGProxy *adpProxy,
						   AdpProp **adpProp)
{
	errorCode_t	err	= NEARDAL_ERROR_NO_ADAPTER;
	guint		len = 0;
	AdpProp		*adapter;
	GList		*tmpList;

	g_assert(adpProp != NULL);

	tmpList = neardalMgr.prop.adpList;
	while (len < g_list_length(tmpList)) {
		adapter = g_list_nth_data(tmpList, len);
		if (adapter != NULL) {
			if (adapter->proxy == adpProxy) {
				*adpProp = adapter;
				err = NEARDAL_SUCCESS;
				break;
			}
		}
		len++;
	}

	return err;
}

/******************************************************************************
 * neardal_mgr_prv_get_tag: Get specific tag from adapter
 *****************************************************************************/
errorCode_t neardal_mgr_prv_get_tag(AdpProp *adpProp, gchar *tagName,
				       TagProp **tagProp)
{
	errorCode_t	err	= NEARDAL_ERROR_NO_TAG;
	guint		len;
	TagProp		*tag	= NULL;
	GList		*tmpList;

	g_assert(adpProp != NULL);
	g_assert(tagName != NULL);
	g_assert(tagProp != NULL);

	len = 0;
	tmpList = adpProp->tagList;
	while (len < g_list_length(tmpList)) {
		tag = g_list_nth_data(tmpList, len);

		if (neardal_tools_prv_cmp_path(tag->name, tagName)) {
			*tagProp = tag;
			err = NEARDAL_SUCCESS;
			break;
		}
		len++;
	}

	return err;
}

/******************************************************************************
 * neardal_mgr_prv_get_record: Get specific record from tag
 *****************************************************************************/
errorCode_t neardal_mgr_prv_get_record(TagProp *tagProp, gchar *rcdName,
				       RcdProp **rcdProp)
{
	errorCode_t	err	= NEARDAL_ERROR_NO_RECORD;
	guint		len;
	RcdProp	*rcd	= NULL;

	g_assert(tagProp != NULL);
	g_assert(rcdName != NULL);
	g_assert(rcdProp != NULL);

	len = 0;
	while (len < g_list_length(tagProp->rcdList)) {
		rcd = g_list_nth_data(tagProp->rcdList, len);
		if (neardal_tools_prv_cmp_path(rcd->name, rcdName)) {
			*rcdProp = rcd;
			err = NEARDAL_SUCCESS;
			break;
		}
		len++;
	}

	return err;
}


/******************************************************************************
 * neardal_mgr_create: Get Neard Manager Properties = NFC Adapters list.
 * Create a DBus proxy for the first one NFC adapter if present
 * Register Neard Manager signals ('PropertyChanged')
 *****************************************************************************/
errorCode_t neardal_mgr_create(void)
{
	errorCode_t	err;
	GPtrArray	*adpArray;
	gsize		adpArrayLen;
	char		*adpName;
	guint		len;

	NEARDAL_TRACEIN();
	if (neardalMgr.proxy != NULL) {
		g_signal_handlers_disconnect_by_func(neardalMgr.proxy,
				G_CALLBACK(neardal_mgr_prv_cb_property_changed),
							NULL);
		g_signal_handlers_disconnect_by_func(neardalMgr.proxy,
				G_CALLBACK(neardal_mgr_prv_cb_adapter_added),
							NULL);
		g_signal_handlers_disconnect_by_func(neardalMgr.proxy,
				G_CALLBACK(neardal_mgr_prv_cb_adapter_removed),
							NULL);
		g_object_unref(neardalMgr.proxy);
		neardalMgr.proxy = NULL;
	}

	err = neardal_tools_prv_create_proxy(neardalMgr.conn,
						  &neardalMgr.proxy,
						  "/", NEARD_MGR_IF_NAME);

	if (err != NEARDAL_SUCCESS)
		return err;

	/* Get and store NFC adapters (is present) */
	adpArray = NULL;
	err = neardal_mgr_prv_get_all_adapters(&adpArray, &adpArrayLen);
	if (adpArray != NULL && adpArrayLen > 0) {
		len = 0;
		while (len < adpArrayLen && err == NEARDAL_SUCCESS) {
			adpName =  g_ptr_array_index(adpArray, len++);
			err = neardal_adp_add(adpName);
		}
		neardal_tools_prv_g_ptr_array_free(adpArray);
	}
	/* Register Marshaller for signals (String,Variant) */
	dbus_g_object_register_marshaller(neardal_marshal_VOID__STRING_BOXED,
					  G_TYPE_NONE, G_TYPE_STRING,
					  G_TYPE_VALUE, G_TYPE_INVALID);

	/* Register for manager signals 'PropertyChanged(String,Variant)' */
	NEARDAL_TRACEF("Register Neard-Manager Signal 'PropertyChanged'\n");
	dbus_g_proxy_add_signal(neardalMgr.proxy, NEARD_MGR_SIG_PROPCHANGED,
				G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(neardalMgr.proxy, NEARD_MGR_SIG_PROPCHANGED,
			 G_CALLBACK(neardal_mgr_prv_cb_property_changed),
				   NULL, NULL);

	/* Register for manager signals 'AdapterAdded(ObjectPath)' */
	NEARDAL_TRACEF("Register Neard-Manager Signal 'AdapterAdded'\n");
	dbus_g_proxy_add_signal(neardalMgr.proxy, NEARD_MGR_SIG_ADP_ADDED,
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(neardalMgr.proxy, NEARD_MGR_SIG_ADP_ADDED,
			 G_CALLBACK(neardal_mgr_prv_cb_adapter_added),
				    NULL, NULL);

	/* Register for manager signals 'AdapterRemoved(ObjectPath)' */
	NEARDAL_TRACEF("Register Neard-Manager Signal 'AdapterRemoved'\n");
	dbus_g_proxy_add_signal(neardalMgr.proxy, NEARD_MGR_SIG_ADP_RM,
				DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(neardalMgr.proxy, NEARD_MGR_SIG_ADP_RM,
			 G_CALLBACK(neardal_mgr_prv_cb_adapter_removed),
				    NULL, NULL);

	return err;
}

/******************************************************************************
 * neardal_mgr_destroy: unref DBus proxy, disconnect Neard Manager signals
 *****************************************************************************/
void neardal_mgr_destroy(void)
{
	GList	*node;
	GList	**tmpList;

	NEARDAL_TRACEIN();
	/* Remove all adapters */
	tmpList = &neardalMgr.prop.adpList;
	while (g_list_length((*tmpList))) {
		node = g_list_first((*tmpList));
		neardal_adp_remove(((AdpProp *)node->data));
	}
	neardalMgr.prop.adpList = (*tmpList);

	if (neardalMgr.proxy == NULL)
		return;

	dbus_g_proxy_disconnect_signal(neardalMgr.proxy,
				       NEARD_MGR_SIG_PROPCHANGED,
			G_CALLBACK(neardal_mgr_prv_cb_property_changed),
						NULL);
	dbus_g_proxy_disconnect_signal(neardalMgr.proxy,
				       NEARD_MGR_SIG_ADP_ADDED,
			G_CALLBACK(neardal_mgr_prv_cb_adapter_added),
						NULL);
	dbus_g_proxy_disconnect_signal(neardalMgr.proxy,
				       NEARD_MGR_SIG_ADP_RM,
			G_CALLBACK(neardal_mgr_prv_cb_adapter_removed),
						NULL);
	g_object_unref(neardalMgr.proxy);
	neardalMgr.proxy = NULL;
}
