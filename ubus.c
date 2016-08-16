/*
 * ovsd - Open vSwitch integration into LEDE's netifd
 * Copyright (C) 2016 Arne Kappen <akappen@inet.tu-berlin.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <unistd.h>
#include <stdio.h>

#include "ovs.h"
#include "ubus.h"

struct ubus_context *ubus_ctx = NULL;
static struct blob_buf bbuf;
static const char *ubus_path;
static struct ubus_object ovsd_obj;

static int
_ovs_error_to_ubus_error(int s)
{
	switch (s) {
		case OVSD_ENOEXIST:
			return UBUS_STATUS_NOT_FOUND;
		case OVSD_EINVALID_ARG:
		case OVSD_EINVALID_VLAN:
				return UBUS_STATUS_INVALID_ARGUMENT;
		case OVSD_ENOPARENT:
			return UBUS_STATUS_NOT_FOUND;
		default:
			return UBUS_STATUS_UNKNOWN_ERROR;
	}
}

/* Register 'ovs' object with ubus
 */
static int
ovsd_add_ubus_object(void)
{
	int ret = ubus_add_object(ubus_ctx, &ovsd_obj);

	if (ret) {
		fprintf(stderr, "Failed to register '%s' object with ubus: %s\n",
			ovsd_obj.name, ubus_strerror(ret));
		return ret;
	}
	return 0;
}

static void
ovsd_ubus_add_fd(void)
{
	ubus_add_uloop(ubus_ctx);
}

static void
ovsd_timed_ubus_reconnect(struct uloop_timeout *to)
{
	ovsd_log_msg(L_NOTICE, "ubus connection lost\n");

	static struct uloop_timeout retry = {
		.cb = ovsd_timed_ubus_reconnect,
	};
	int t = 2;

	if (ubus_reconnect(ubus_ctx, ubus_path) != 0) {
		ovsd_log_msg(L_NOTICE, "ubus reconnect failed, retry in %ds\n", t);
		uloop_timeout_set(&retry, t * 1000);
		return;
	}

	ovsd_log_msg(L_NOTICE, "reconnected to ubus, new id: %08x\n",
		ubus_ctx->local_id);
	ovsd_ubus_add_fd();
}

static void
ovsd_ubus_connection_lost_cb(struct ubus_context *ubus_ctx)
{
	ovsd_timed_ubus_reconnect(NULL);
}

int
ovsd_ubus_init(const char *path)
{
	uloop_init();
	ubus_path = path;

	ubus_ctx = ubus_connect(path);
	if (!ubus_ctx)
		return -EIO;

	ovsd_log_msg(L_NOTICE, "ubus connection established.\n",
		ubus_ctx->local_id);
	ubus_ctx->connection_lost = ovsd_ubus_connection_lost_cb;
	ovsd_ubus_add_fd();

	ovsd_add_ubus_object();

	return 0;
}

static char**
_parse_strarray(struct blob_attr *head, size_t len, int *n_entries)
{
	struct blob_attr *cur;
	int offset = 0;

	char **arr = calloc(len, sizeof(char*));
	if (!arr)
		return NULL;

	__blob_for_each_attr(cur, head, len) {
		if (blobmsg_type(cur) != BLOBMSG_TYPE_STRING)
			continue;

		arr[offset++] = blobmsg_get_string(cur);
	}

	*n_entries = offset;
	return arr;
}

enum {
	CREATPOL_BRIDGE,
	CREATPOL_PARENT,
	CREATPOL_VLAN,
	CREATPOL_OFCONTROLLERS,
	CREATPOL_FAILMODE,
	CREATPOL_SSLPRIVKEY,
	CREATPOL_SSLCERT,
	CREATPOL_SSLCACERT,
	CREATPOL_SSLBOOTSTRAP,
	__CREATPOL_MAX
};
static const struct blobmsg_policy create_policy[__CREATPOL_MAX] = {
	[CREATPOL_BRIDGE] = {
		.name = "name",
		.type = BLOBMSG_TYPE_STRING,
	},
	[CREATPOL_PARENT] = {
		.name = "parent",
		.type = BLOBMSG_TYPE_STRING,
	},
	[CREATPOL_VLAN] = {
		.name = "vlan",
		.type = BLOBMSG_TYPE_INT32,
	},
	[CREATPOL_OFCONTROLLERS] = {
		.name = "ofcontrollers",
		.type = BLOBMSG_TYPE_ARRAY,
	},
	[CREATPOL_FAILMODE] = {
		.name = "controller_fail_mode",
		.type = BLOBMSG_TYPE_STRING,
	},
	[CREATPOL_SSLPRIVKEY] = {
		.name = "ssl_private_key",
		.type = BLOBMSG_TYPE_STRING,
	},
	[CREATPOL_SSLCERT] = {
		.name = "ssl_cert",
		.type = BLOBMSG_TYPE_STRING,
	},
	[CREATPOL_SSLCACERT] = {
		.name = "ssl_ca_cert",
		.type = BLOBMSG_TYPE_STRING,
	},
	[CREATPOL_SSLBOOTSTRAP] = {
		.name = "ssl_bootstrap",
		.type =  BLOBMSG_TYPE_BOOL,
	},
};

static void
_notify_complete_cb(struct ubus_notify_request *req, int idx, int ret)
{
	free(req);
}

enum netifd_notification_type {
	NETIFD_NOTIFY_CREATE,
	NETIFD_NOTIFY_RELOAD,
	NETIFD_NOTIFY_FREE,

	NETIFD_NOTIFY_HOTPLUG_PREPARE,
	NETIFD_NOTIFY_HOTPLUG_ADD,
	NETIFD_NOTIFY_HOTPLUG_REMOVE,
	__NETIFD_NOTIFY_MAX
};
static const char *netifd_notification[__NETIFD_NOTIFY_MAX] = {
	[NETIFD_NOTIFY_CREATE] = "create",
	[NETIFD_NOTIFY_RELOAD] = "reload",
	[NETIFD_NOTIFY_FREE] = "free",

	[NETIFD_NOTIFY_HOTPLUG_PREPARE] = "prepare",
	[NETIFD_NOTIFY_HOTPLUG_ADD] = "add",
	[NETIFD_NOTIFY_HOTPLUG_REMOVE] = "remove",

};

static void
_notify_complete_cb(struct ubus_notify_request *req, int idx, int ret)
{
	free(req);
}

static void
_send_errormsg(struct ubus_request_data *req, const char *msg)
{
	blob_buf_init(&bbuf, 0);
	blobmsg_add_string(&bbuf, "message", msg);

	ubus_send_reply(ubus_ctx, req, bbuf.head);
}

static int
_notify_netifd(enum netifd_notification_type type, const char *bridge,
	const char *member)
{
	int ret;
	struct ubus_notify_request *req;

	req = calloc(1, sizeof(struct ubus_notify_request));
	if (!req)
		return -ENOMEM;

	req->complete_cb = _notify_complete_cb;

	blob_buf_init(&bbuf, 0);

	if (type < NETIFD_NOTIFY_HOTPLUG_ADD) {
		blobmsg_add_string(&bbuf, "name", bridge);
	} else {
		blobmsg_add_string(&bbuf, "bridge", bridge);
		blobmsg_add_string(&bbuf, "member", member);
	}

	ret = ubus_notify_async(ubus_ctx, &ovsd_obj, netifd_notification[type],
		bbuf.head, req);
	if (ret)
		fprintf(stderr, "%s notification failed: %s\n",
			netifd_notification[type], ubus_strerror(ret));

	return ret;
}

static int
_parse_ofcontroller_opts(struct blob_attr **tb,
	struct ovswitch_br_config *ovs_cfg)
{
	if (!tb[CREATPOL_OFCONTROLLERS])
		return 0;

	ovs_cfg->ofcontrollers = _parse_strarray(
		blobmsg_data(tb[CREATPOL_OFCONTROLLERS]),
		blobmsg_data_len(tb[CREATPOL_OFCONTROLLERS]),
		&ovs_cfg->n_ofcontrollers);

	if (!ovs_cfg->n_ofcontrollers || !ovs_cfg->ofcontrollers)
		return -1;

	if (tb[CREATPOL_FAILMODE]) {
		if (!strcmp(blobmsg_get_string(tb[CREATPOL_FAILMODE]), "standalone"))
			ovs_cfg->fail_mode = OVS_FAIL_MODE_STANDALONE;
		else if (!strcmp(blobmsg_get_string(tb[CREATPOL_FAILMODE]), "secure"))
			ovs_cfg->fail_mode = OVS_FAIL_MODE_SECURE;
		else
			return -1;
	} else {
		ovs_cfg->fail_mode = OVS_FAIL_MODE_STANDALONE;
	}

	return 0;
}

static int
_parse_ssl_opts(struct blob_attr **tb, struct ovswitch_br_config *cfg)
{
	if (!tb[CREATPOL_SSLPRIVKEY] || !tb[CREATPOL_SSLCERT] ||
		!tb[CREATPOL_SSLCACERT])
		return -1;

	if (tb[CREATPOL_SSLBOOTSTRAP])
		cfg->ssl_bootstrap = blobmsg_get_bool(tb[CREATPOL_SSLBOOTSTRAP]);

	cfg->ssl_privkey_file = blobmsg_get_string(tb[CREATPOL_SSLPRIVKEY]);
	cfg->ssl_cert_file = blobmsg_get_string(tb[CREATPOL_SSLCERT]);
	cfg->ssl_cacert_file = blobmsg_get_string(tb[CREATPOL_SSLCACERT]);

	return 0;
}

static enum ovsd_status
_parse_create_msg(struct blob_attr **tb, struct ovswitch_br_config *cfg)
{
	// parse name
	if (!tb[CREATPOL_BRIDGE])
		return OVSD_EINVALID_ARG;
	cfg->name = blobmsg_get_string(tb[CREATPOL_BRIDGE]);

	// parse fake bridge options
	if (tb[CREATPOL_PARENT] && tb[CREATPOL_VLAN]) {
		cfg->parent = blobmsg_get_string(tb[CREATPOL_PARENT]);
		cfg->vlan_tag = blobmsg_get_u32(tb[CREATPOL_VLAN]);
	}

	// parse SSL options
	_parse_ssl_opts(tb, cfg);

	// parse list of OF-controllers
	_parse_ofcontroller_opts(tb, cfg);
	return OVSD_OK;
}

static int
_handle_create(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__CREATPOL_MAX];
	struct ovswitch_br_config ovs_cfg = OVSWITCH_CONFIG_INIT;
	int ret;

	blobmsg_parse(create_policy, __CREATPOL_MAX, tb, blob_data(msg),
		blob_len(msg));

	ret = _parse_create_msg(tb, &ovs_cfg);
	if (ret)
		return ret;

	// create the device
	ret = ovs_create(&ovs_cfg);

	// free string array
	if (ovs_cfg.ofcontrollers)
		free(ovs_cfg.ofcontrollers);

	if (ret)
		goto error;

	return _notify_netifd(NETIFD_NOTIFY_CREATE, ovs_cfg.name, NULL);

error:
	fprintf(stderr, "Failed to create '%s': %s\n", ovs_cfg.name,
		ovs_strerror(ret));

	char errormsg[strlen("Failed to create : ") + strlen(ovs_cfg.name) +
				  strlen(ovs_strerror(ret)) + 1];

	sprintf(errormsg, "Failed to create %s: %s", ovs_cfg.name,
		ovs_strerror(ret));

	_send_errormsg(req, errormsg);

	return _ovs_error_to_ubus_error(ret);
}

static int
_handle_configure(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	return 0;
}

/* Reload a bridge. The bridge is deleted and re-created with the given config
 */
static int
_handle_reload(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	int ret;
	struct blob_attr *tb[__CREATPOL_MAX];
	struct ovswitch_br_config ovs_cfg = OVSWITCH_CONFIG_INIT;

	blobmsg_parse(create_policy, __CREATPOL_MAX, tb, blobmsg_data(msg),
		blobmsg_len(msg));

	ret = _parse_create_msg(tb, &ovs_cfg);
	if (ret)
		return ret;

	// delete and re-create the bridge
	ovs_delete(ovs_cfg.name);
	ret = ovs_create(&ovs_cfg);
	if (ret)
		fprintf(stderr, "Failed to re-create '%s': %s\n", ovs_cfg.name,
				ovs_strerror(ret));

	// free string arrays
	if (ovs_cfg.ofcontrollers)
		free(ovs_cfg.ofcontrollers);

	return _notify_netifd(NETIFD_NOTIFY_RELOAD, ovs_cfg.name, NULL);
}

enum {
	DUMP_INFO_POLICY_BRIDGE,
	__DUMP_INFO_POLICY_MAX,
};

static struct blobmsg_policy dump_info_policy[__DUMP_INFO_POLICY_MAX] = {
	[DUMP_INFO_POLICY_BRIDGE] = {
		.name = "bridge",
		.type = BLOBMSG_TYPE_STRING,
	},
};

static int
_handle_dump_info(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[1];

	blobmsg_parse(dump_info_policy, __DUMP_INFO_POLICY_MAX, tb,
		blobmsg_data(msg), blobmsg_len(msg));

	blob_buf_init(&bbuf, 0);
	ovs_dump_info(&bbuf, blobmsg_get_string(tb[0]));

	ubus_send_reply(ubus_ctx, req, bbuf.head);
	return 0;
}

static int
_handle_dump_stats(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	return 0;
}

enum {
	DELPOL_BRIDGE,
	__DELPOL_MAX
};
static const struct blobmsg_policy delete_policy[__DELPOL_MAX] = {
	[DELPOL_BRIDGE] = {
		.name = "bridge",
		.type = BLOBMSG_TYPE_STRING,
	},
};

static int
_handle_free(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__DELPOL_MAX];
	int ret;

	blobmsg_parse(delete_policy, __DELPOL_MAX, tb, blobmsg_data(msg),
		blobmsg_len(msg));

	if (!tb[DELPOL_BRIDGE])
		return UBUS_STATUS_INVALID_ARGUMENT;

	ret = ovs_delete(blobmsg_get_string(tb[DELPOL_BRIDGE]));

	if (ret)
		goto error;

	return _notify_netifd(NETIFD_NOTIFY_FREE,
		blobmsg_get_string(tb[DELPOL_BRIDGE]), NULL);

error:
	fprintf(stderr, "Failed to delete bridge '%s': %s\n",
		blobmsg_get_string(tb[DELPOL_BRIDGE]), ovs_strerror(ret));

	return _ovs_error_to_ubus_error(ret);
}

enum {
	CHECK_STATE_POLICY_BRIDGE,
	__CHECK_STATE_POLICY_MAX
};

static struct blobmsg_policy check_state_policy[__CHECK_STATE_POLICY_MAX] = {
	[CHECK_STATE_POLICY_BRIDGE] = {
			.name = "bridge",
			.type = BLOBMSG_TYPE_STRING
	},
};

static int
_handle_check_state(struct ubus_context *ctx, struct ubus_object *obj,
		struct ubus_request_data *req, const char *method,
		struct blob_attr *msg)
{
	struct blob_attr *tb[__CHECK_STATE_POLICY_MAX];

	blobmsg_parse(check_state_policy, __CHECK_STATE_POLICY_MAX, tb,
			blobmsg_data(msg), blobmsg_len(msg));

	if (!tb[CHECK_STATE_POLICY_BRIDGE])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (ovs_check_state(blobmsg_get_string(tb[CHECK_STATE_POLICY_BRIDGE])))
		return UBUS_STATUS_NOT_FOUND;

	return 0;
}

enum {
	HOTPLUG_ADDPOL_BRIDGE,
	HOTPLUG_ADDPOL_MEMBER,
	__HOTPLUG_ADDPOL_MAX
};

static struct blobmsg_policy hotplug_add_policy[__HOTPLUG_ADDPOL_MAX] = {
	[HOTPLUG_ADDPOL_BRIDGE] = { .name = "bridge", .type = BLOBMSG_TYPE_STRING },
	[HOTPLUG_ADDPOL_MEMBER] = { .name = "member", .type = BLOBMSG_TYPE_STRING },
};

static int
_handle_hotplug_add(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__HOTPLUG_ADDPOL_MAX];
	int ret;

	blobmsg_parse(hotplug_add_policy, __HOTPLUG_ADDPOL_MAX, tb,
		blob_data(msg), blob_len(msg));

	if (!tb[HOTPLUG_ADDPOL_BRIDGE] || !tb[HOTPLUG_ADDPOL_MEMBER])
		return UBUS_STATUS_INVALID_ARGUMENT;

	ret = ovs_add_port(blobmsg_get_string(tb[HOTPLUG_ADDPOL_BRIDGE]),
		blobmsg_get_string(tb[HOTPLUG_ADDPOL_MEMBER]));

	if (ret)
		goto error;

	return _notify_netifd(NETIFD_NOTIFY_HOTPLUG_ADD,
		blobmsg_get_string(tb[HOTPLUG_ADDPOL_BRIDGE]),
		blobmsg_get_string(tb[HOTPLUG_ADDPOL_MEMBER]));

error:
	fprintf(stderr, "%s: failed to add port %s: %s\n",
		blobmsg_get_string(tb[HOTPLUG_ADDPOL_BRIDGE]),
		blobmsg_get_string(tb[HOTPLUG_ADDPOL_MEMBER]), ovs_strerror(ret));

	return _ovs_error_to_ubus_error(ret);
}

enum {
	HOTPLUG_DELPOL_BRIDGE,
	HOTPLUG_DELPOL_MEMBER,
	__HOTPLUG_DELPOL_MAX,
};
static struct blobmsg_policy hotplug_del_policy[__HOTPLUG_DELPOL_MAX] = {
	[HOTPLUG_DELPOL_BRIDGE] = {
		.name = "bridge",
		.type = BLOBMSG_TYPE_STRING
	},
	[HOTPLUG_DELPOL_MEMBER] = {
		.name = "member",
		.type = BLOBMSG_TYPE_STRING
	},
};

static int
_handle_hotplug_remove(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__HOTPLUG_DELPOL_MAX];
	int ret;

	blobmsg_parse(hotplug_del_policy, __HOTPLUG_DELPOL_MAX, tb,
		blob_data(msg), blob_len(msg));

	if (!tb[HOTPLUG_DELPOL_BRIDGE] || !tb[HOTPLUG_DELPOL_MEMBER])
		return UBUS_STATUS_INVALID_ARGUMENT;

	ret = ovs_remove_port(blobmsg_get_string(tb[HOTPLUG_DELPOL_BRIDGE]),
		blobmsg_get_string(tb[HOTPLUG_DELPOL_MEMBER]));

	if (ret)
		goto error;

	_notify_netifd(NETIFD_NOTIFY_HOTPLUG_REMOVE,
		blobmsg_get_string(tb[HOTPLUG_DELPOL_BRIDGE]),
		blobmsg_get_string(tb[HOTPLUG_DELPOL_MEMBER]));

	return 0;

error:
	fprintf(stderr, "%s: failed to remove port %s: %s\n",
		blobmsg_get_string(tb[HOTPLUG_DELPOL_BRIDGE]),
		blobmsg_get_string(tb[HOTPLUG_DELPOL_MEMBER]),
		ovs_strerror(ret));

	return ret;
}

enum {
	HOTPLUG_PREPPOL_BRIDGE,
	__HOTPLUG_PREPPOL_MAX
};

static struct blobmsg_policy hotplug_prep_policy[__HOTPLUG_PREPPOL_MAX] = {
	[HOTPLUG_PREPPOL_BRIDGE] = {
		.name = "bridge",
		.type = BLOBMSG_TYPE_STRING
	},
};

static int
_handle_hotplug_prepare(struct ubus_context *ctx, struct ubus_object *obj,
	struct ubus_request_data *req, const char *method, struct blob_attr *msg)
{
	struct blob_attr *tb[__HOTPLUG_PREPPOL_MAX];

	blobmsg_parse(hotplug_prep_policy, __HOTPLUG_PREPPOL_MAX, tb,
			blobmsg_data(msg), blobmsg_len(msg));

	if (!tb[HOTPLUG_PREPPOL_BRIDGE])
		return UBUS_STATUS_INVALID_ARGUMENT;

	if (ovs_prepare_bridge(blobmsg_get_string(tb[HOTPLUG_PREPPOL_BRIDGE])))
		return UBUS_STATUS_NOT_FOUND;

	_notify_netifd(NETIFD_NOTIFY_HOTPLUG_PREPARE,
		blobmsg_get_string(tb[HOTPLUG_PREPPOL_BRIDGE]), NULL);

	return 0;
}

enum {
	// device handler interface
	METHOD_CREATE,
	METHOD_CONFIG_INIT,
	METHOD_RELOAD,
	METHOD_DUMP_INFO,
	METHOD_DUMP_STATS,
	METHOD_CHECK_STATE,
	METHOD_FREE,

	// hotplug ops
	METHOD_HOTPLUG_ADD,
	METHOD_HOTPLUG_REMOVE,
	METHOD_HOTPLUG_PREPARE,
	__METHODS_MAX
};
static struct ubus_method ubus_methods[__METHODS_MAX] = {
	// device handler interface
	[METHOD_CREATE] = UBUS_METHOD("create", _handle_create, create_policy),
	[METHOD_CONFIG_INIT] = UBUS_METHOD_NOARG("configure", _handle_configure),
	[METHOD_RELOAD] = UBUS_METHOD("reload", _handle_reload, create_policy),
	[METHOD_DUMP_INFO] = UBUS_METHOD("dump_info", _handle_dump_info,
		dump_info_policy),
	[METHOD_DUMP_STATS] = UBUS_METHOD_NOARG("dump_stats", _handle_dump_stats),
	[METHOD_CHECK_STATE] = UBUS_METHOD("check_state", _handle_check_state,
			check_state_policy),
	[METHOD_FREE] = UBUS_METHOD("free", _handle_free, delete_policy),

	// hotplug ops
	[METHOD_HOTPLUG_ADD] = UBUS_METHOD("add", _handle_hotplug_add,
		hotplug_add_policy),
	[METHOD_HOTPLUG_REMOVE] = UBUS_METHOD("remove", _handle_hotplug_remove,
		hotplug_del_policy),
	[METHOD_HOTPLUG_PREPARE] = UBUS_METHOD("prepare", _handle_hotplug_prepare,
		hotplug_prep_policy),
};

static struct ubus_object_type ovsd_obj_type =
	UBUS_OBJECT_TYPE("ovsd", ubus_methods);

static struct ubus_object ovsd_obj = {
	.name = "ovs",
	.type = &ovsd_obj_type,
	.methods = ubus_methods,
	.n_methods = __METHODS_MAX,
};
