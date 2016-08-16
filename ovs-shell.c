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
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include "ovs-shell.h"

#define CMD_LEN_MAX 65536
#define VLAN_TAG_MASK 0xfff

static char * const ovs_vsctl_cmd[__CMD_MAX] = {
	[CMD_CREATE_BR] 		= "add-br",
	[CMD_DEL_BR] 			= "del-br",
	[CMD_ADD_PORT] 			= "add-port",
	[CMD_DEL_PORT] 			= "del-port",
	[CMD_BR_EXISTS]			= "br-exists",
	[CMD_BR_TO_VLAN]		= "br-to-vlan",
	[CMD_BR_TO_PARENT]		= "br-to-parent",

	[CMD_SET_OFCTL] 		= "set-controller",
	[CMD_DEL_OFCTL] 		= "del-controller",
	[CMD_GET_OFCTL] 		= "get-controller",
	[CMD_SET_FAIL_MODE]		= "set-fail-mode",
	[CMD_DEL_FAIL_MODE]		= "del-fail-mode",
	[CMD_GET_FAIL_MODE]		= "get-fail-mode",
	[CMD_SET_SSL]			= "set-ssl",
	[CMD_DEL_SSL]			= "del-ssl",
	[CMD_GET_SSL]			= "get-ssl",

	[CMD_LIST_PORTS]		= "list-ports",

	[MODIFIER_MAY_EXIST]	= "--may-exist",
	[MODIFIER_IF_EXISTS]	= "--if-exists",
	[MODIFIER_SSL_BOOTSTRAP]= "--bootstrap",

	[ATOMIC_CMD_SEPARATOR] = "--",
};

char * const
ovs_cmd(enum ovs_vsctl_cmd cmd)
{
	if (cmd > __CMD_MAX)
		return NULL;

	return ovs_vsctl_cmd[cmd];
}

int
ovs_vsctl(char * const *argv)
{
	int rc, status;
	pid_t pid = fork();
	if (!pid)
		exit(execv(OVS_VSCTL, argv));
	if (pid < 0) {
		rc = -1;
	} else {
		while ((rc = waitpid(pid, &status, 0)) == -1 && errno == EINTR);
		rc = (rc == pid && WIFEXITED(status)) ? WEXITSTATUS(status) : -1;
	}
	return rc;
}

/* Remove leading and trailing whitespace from string
 */
static char *
_trim(char *str)
{
	char *end;

	while (isspace(*str))
		str++;


	if (*str == '\0')
		return NULL;

	end = str + strlen(str) - 1;
	while (end > str && isspace(*end))
		end--;

	*(end + 1) = '\0';

	return str;
}

/* Strip escape sequences from string.
 */
static char *
_strip(char *str)
{
	char *seek_ptr = str, *write_ptr = str;

	while (*seek_ptr != '\0') {
		if (*seek_ptr == '\\')
			seek_ptr++;

		*write_ptr = *seek_ptr;
		write_ptr++;
		seek_ptr++;
	}

	*write_ptr = '\0';
	return str;
}

static void
_lowercase(char *str)
{
	while (*str != '\0') {
		*str = (char) tolower(*str);
		str++;
	}
}

static void
_replace(char *str, char c, char other)
{
	while (*str != '\0') {
		if (*str == c)
			*str = other;
		str++;
	}
}

static char *
sanitize(char *str)
{
	// create temporary copy of string
	char edit_buf[SHELL_OUTPUT_LINE_MAXSIZE];
	char *ret;
	strcpy(edit_buf, str);

	// remove leading and trailing whitespace
	ret = _trim(edit_buf);

	// strip escape sequences
	ret = _strip(ret);

	// replace original string with sanitized one
	strcpy(str, ret);

	_lowercase(str);
	_replace(str, ' ', '_');
	return str;
}

void
ovs_shell_capture_string(const char *cmd, const char *bridge,
		const char *name, struct blob_buf *buf)
{
	char output[256];
	FILE *f;

	size_t cmd_len = strlen(OVS_VSCTL) + strlen(cmd) + strlen(bridge) + 3;

	if (cmd_len > CMD_LEN_MAX)
		return;

	char cmd_str[cmd_len];
	sprintf(cmd_str, "%s %s %s", OVS_VSCTL, cmd, bridge);

	if ((f = popen(cmd_str, "r")) == NULL)
		return;

	if (fgets(output, 256, f) == NULL)
		goto done;

	blobmsg_add_string(buf, name, sanitize(output));

done:
	pclose(f);
}

void
ovs_shell_capture_list(const char *cmd, const char *bridge,
	const char *list_name, struct blob_buf *buf, bool table)
{
	char *tmp, output[512];
	FILE *f;
	void *list;
	size_t cmd_len;

	cmd_len = strlen(OVS_VSCTL) + 1 + strlen(cmd) + 1;
	if (bridge)
		cmd_len += strlen(bridge) + 1;

	if (cmd_len > CMD_LEN_MAX)
		return;

	char cmd_str[cmd_len];
	sprintf(cmd_str, "%s %s %s", OVS_VSCTL, cmd, bridge ? bridge : "");

	if ((f = popen(cmd_str, "r")) == NULL)
		return;

	if (table)
		list = blobmsg_open_table(buf, list_name);
	else
		list = blobmsg_open_array(buf, list_name);

	while (fgets(output, 512, f) != NULL) {

		// in case of table, separate key and value
		if (table && (tmp = strchr(output, ':'))) {
			*tmp++ = '\0';
			blobmsg_add_string(buf, sanitize(output), sanitize(tmp));
		} else {
			blobmsg_add_string(buf, NULL, sanitize(output));
		}
	}

	if (table)
		blobmsg_close_table(buf, list);
	else
		blobmsg_close_array(buf, list);
	pclose(f);
}

bool
ovs_shell_br_exists(char *name)
{
	char * const argv[4] = {
		[0] = OVS_VSCTL,
		[1] = ovs_vsctl_cmd[CMD_BR_EXISTS],
		[2] = name,
		[3] = NULL,
	};

	if (ovs_vsctl(argv) == 0)
		return true;
	return false;
}

static int
_ovs_shell_get_output(const char *cmd, const char *bridge, char *buf, int n)
{
	FILE *f;
	size_t cmd_len;
	int ret = 0;

	cmd_len = strlen(OVS_VSCTL) + 1 + strlen(cmd) + 1;
	if (bridge)
		cmd_len += strlen(bridge) + 1;

	if (cmd_len > CMD_LEN_MAX)
		return -1;

	char cmd_str[cmd_len];
	sprintf(cmd_str, "%s %s %s", OVS_VSCTL, cmd, bridge ? bridge : "");

	if ((f = popen(cmd_str, "r")) == NULL)
		return -1;

	if (fgets(buf, n, f) == NULL)
		ret = -1;

	pclose(f);
	return ret;
}

int
ovs_shell_br_to_vlan(char *bridge)
{
	char buf[5];

	if (!ovs_shell_br_exists(bridge))
		return -1;

	if (_ovs_shell_get_output(ovs_cmd(CMD_BR_TO_VLAN), bridge, buf, 5))
		return -1;

	return atoi(buf);
}

size_t
ovs_shell_br_to_parent(char *bridge, char *buf, size_t n)
{
	char out_buf[64];

	if (!ovs_shell_br_exists(bridge))
		return 0;

	if (_ovs_shell_get_output(ovs_cmd(CMD_BR_TO_PARENT), bridge, out_buf, 64))
		return 0;

	sanitize(out_buf);
	strncpy(buf, out_buf, n);
	return strnlen(out_buf, 64);
}

int
ovs_shell_create_bridge(struct ovswitch_br_config *cfg)
{
	bool fake_br = false;
	size_t ovs_vsctl_nargs = 2;	// program name and terminating NULL
	size_t cur_arg;

	// 1: add-br 2: --may-exist 3: br-name
	ovs_vsctl_nargs += 3;

	// in case of fake bridge, check args
	if (cfg->parent && (cfg->vlan_tag >= 0)) {

		// check 802.1q compliance
		if (cfg->vlan_tag > 0 && ((cfg->vlan_tag & VLAN_TAG_MASK) == 0xfff))
			return OVSD_EINVALID_VLAN;

		// check if parent bridge exists
		if (!ovs_shell_br_exists(cfg->parent))
			return OVSD_ENOPARENT;

		ovs_vsctl_nargs += 2;
		fake_br = true;
	}

	// 1: atomic cmd separator, 2: set-controller cmd,
	// 3: bridge, 4...: controllers
	if (!fake_br && cfg->ofcontrollers) {
		ovs_vsctl_nargs += 3 + cfg->n_ofcontrollers;
		// 1: atomic cmd separator, 2: set-fail-mode, 3: bridge, 4: fail-mode
		ovs_vsctl_nargs += 4;

		// SSL options: 5 or 6 options
		// separator, (--bootstrap), set-ssl, private key, cert, CA cert
		if (cfg->ssl_privkey_file) {
			if (cfg->ssl_bootstrap)
				ovs_vsctl_nargs += 6;
			else
				ovs_vsctl_nargs += 5;
		}
	}

	// build argv for ovs-vsctl
	char *ovs_vsctl_argv[ovs_vsctl_nargs];

	cur_arg = 0;

	// program name
	ovs_vsctl_argv[cur_arg++] = OVS_VSCTL;

	// create bridge command w/ may exist modifier
	ovs_vsctl_argv[cur_arg++] = ovs_cmd(MODIFIER_MAY_EXIST);
	ovs_vsctl_argv[cur_arg++] = ovs_cmd(CMD_CREATE_BR);

	// bridge name
	ovs_vsctl_argv[cur_arg++] = cfg->name;

	// fake bridge parameters
	if (fake_br) {
		ovs_vsctl_argv[cur_arg++] = cfg->parent;
		ovs_vsctl_argv[cur_arg] = alloca(6 * sizeof(char));
		sprintf(ovs_vsctl_argv[cur_arg++], "%hu", cfg->vlan_tag);
	} else if (cfg->ofcontrollers) {
		ovs_vsctl_argv[cur_arg++] = ovs_cmd(ATOMIC_CMD_SEPARATOR);
		ovs_vsctl_argv[cur_arg++] = ovs_cmd(CMD_SET_OFCTL);
		ovs_vsctl_argv[cur_arg++] = cfg->name;
		for (int i = 0; i < cfg->n_ofcontrollers; i++)
			ovs_vsctl_argv[cur_arg++] = cfg->ofcontrollers[i];

		// fail mode in case of OF controller unavailability
		switch (cfg->fail_mode) {
			case OVS_FAIL_MODE_SECURE:
				ovs_vsctl_argv[cur_arg++] = ovs_cmd(ATOMIC_CMD_SEPARATOR);
				ovs_vsctl_argv[cur_arg++] = ovs_cmd(CMD_SET_FAIL_MODE);
				ovs_vsctl_argv[cur_arg++] = cfg->name;
				ovs_vsctl_argv[cur_arg++] = "secure";
				break;
			case OVS_FAIL_MODE_STANDALONE:
				ovs_vsctl_argv[cur_arg++] = ovs_cmd(ATOMIC_CMD_SEPARATOR);
				ovs_vsctl_argv[cur_arg++] = ovs_cmd(CMD_SET_FAIL_MODE);
				ovs_vsctl_argv[cur_arg++] = cfg->name;
				ovs_vsctl_argv[cur_arg++] = "standalone";
				break;
			default: break;
		}

		// SSL options
		if (cfg->ssl_privkey_file) {
			ovs_vsctl_argv[cur_arg++] = ovs_cmd(ATOMIC_CMD_SEPARATOR);
			if (cfg->ssl_bootstrap)
				ovs_vsctl_argv[cur_arg++] = ovs_cmd(MODIFIER_SSL_BOOTSTRAP);
			ovs_vsctl_argv[cur_arg++] = ovs_cmd(CMD_SET_SSL);
			ovs_vsctl_argv[cur_arg++] = cfg->ssl_privkey_file;
			ovs_vsctl_argv[cur_arg++] = cfg->ssl_cert_file;
			ovs_vsctl_argv[cur_arg++] = cfg->ssl_cacert_file;
		}
	}

	// execv needs terminating NULL in argv
	ovs_vsctl_argv[cur_arg] = NULL;

	return ovs_vsctl(ovs_vsctl_argv);
}

int
ovs_shell_delete_bridge(char *bridge)
{
	char * const argv[5] = {
		[0] = OVS_VSCTL,
		[1] = ovs_vsctl_cmd[MODIFIER_IF_EXISTS],
		[2] = ovs_vsctl_cmd[CMD_DEL_BR],
		[3] = bridge,
		[4] = NULL,
	};

	return ovs_vsctl(argv);
}

int
ovs_shell_add_port(char *bridge, char *port)
{
	if (!ovs_shell_br_exists(bridge))
		return OVSD_ENOEXIST;

	char * const argv[6] = {
		[0] = OVS_VSCTL,
		[1] = ovs_cmd(MODIFIER_MAY_EXIST),
		[2] = ovs_cmd(CMD_ADD_PORT),
		[3] = bridge,
		[4] = port,
		[5] = NULL,
	};

	return ovs_vsctl(argv);
}

int
ovs_shell_remove_port(char *bridge, char *port)
{
	if (!ovs_shell_br_exists(bridge))
		return OVSD_OK;

	char * const argv[6] = {
		[0] = OVS_VSCTL,
		[1] = ovs_cmd(MODIFIER_IF_EXISTS),
		[2] = ovs_cmd(CMD_DEL_PORT),
		[3] = bridge,
		[4] = port,
		[5] = NULL
	};

	return ovs_vsctl(argv);
}
