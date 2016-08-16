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
#ifndef OVSD_OVS_SHELL_H
#define OVSD_OVS_SHELL_H

#include <stdbool.h>

#include "ovsd.h"

#define OVS_VSCTL "/usr/bin/ovs-vsctl"

#define SHELL_OUTPUT_LINE_MAXSIZE 256

enum ovs_vsctl_cmd {
	CMD_CREATE_BR,
	CMD_DEL_BR,
	CMD_ADD_PORT,
	CMD_DEL_PORT,
	CMD_BR_EXISTS,
	CMD_BR_TO_VLAN,
	CMD_BR_TO_PARENT,

	CMD_GET_OFCTL,
	CMD_SET_OFCTL,
	CMD_DEL_OFCTL,
	CMD_SET_FAIL_MODE,
	CMD_DEL_FAIL_MODE,
	CMD_GET_FAIL_MODE,

	CMD_SET_SSL,
	CMD_DEL_SSL,
	CMD_GET_SSL,

	CMD_LIST_PORTS,

	MODIFIER_MAY_EXIST,
	MODIFIER_IF_EXISTS,
	MODIFIER_SSL_BOOTSTRAP,

/* ovs-vsctl allows to combine commands to a single atomic transaction against
 * the database. The individual commands need to be separated by '--'.
 * If options are used for any command, they need to be separated from the
 * global options, too. (e.g. ovs-vsctl -- --if-exists del-br br0). */
	ATOMIC_CMD_SEPARATOR,
	__CMD_MAX
};

char * const ovs_cmd(enum ovs_vsctl_cmd);
int ovs_vsctl(char * const *argv);

void ovs_shell_capture_string(const char *cmd, const char *bridge,
	const char *name, struct blob_buf *buf);

void ovs_shell_capture_list(const char *cmd, const char *bridge,
	const char *list_name, struct blob_buf *buf, bool table);

bool ovs_shell_br_exists(char *name);
int ovs_shell_br_to_vlan(char *bridge);
size_t ovs_shell_br_to_parent(char *bridge, char *buf, size_t n);
int ovs_shell_create_bridge(struct ovswitch_br_config *cfg);
int ovs_shell_delete_bridge(char *bridge);
int ovs_shell_add_port(char *bridge, char *port);
int ovs_shell_remove_port(char *bridge, char *port);

#endif //OVSD_OVS_SHELL_H
