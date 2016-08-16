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
#include <stdio.h>

#include "ovs.h"
#include "ovs-shell.h"

int
ovs_create(struct ovswitch_br_config *cfg)
{
	int ret = ovs_shell_create_bridge(cfg);

	if (ret)
		fprintf(stderr, "Could not create bridge '%s': %s\n",
			cfg->name ? cfg->name : "", ovs_strerror(ret));

	return ret;
}

int
ovs_delete(char *bridge)
{
	return ovs_shell_delete_bridge(bridge);
}

int
ovs_prepare_bridge(char *bridge)
{
	if (!ovs_shell_br_exists(bridge))
		return OVSD_ENOEXIST;

	return 0;
}

int
ovs_add_port(char *bridge, char *port)
{
	int ret = ovs_shell_add_port(bridge, port);
	if (ret)
		fprintf(stderr, "Could not add port '%s' to bridge %s: %s\n",
			port, bridge, ovs_strerror(ret));

	return ret;
}

int
ovs_remove_port(char *bridge, char *port)
{
	int ret = ovs_shell_remove_port(bridge, port);
	if (ret)
		fprintf(stderr, "Could not remove port '%s' to bridge %s: %s\n",
			port, bridge, ovs_strerror(ret));

	return ret;
}

int
ovs_check_state(char *bridge)
{
	if (!ovs_shell_br_exists(bridge))
		return OVSD_ENOEXIST;

	return 0;
}

int
ovs_dump_info(struct blob_buf *buf, char *bridge)
{
	char out_buf[64];
	int vlan_tag;

	ovs_shell_capture_list(ovs_cmd(CMD_GET_SSL), NULL, "ssl", buf, true);

	if (!bridge)
		return 0;

	if (!ovs_shell_br_exists(bridge))
		return OVSD_ENOEXIST;

	if (ovs_shell_br_to_parent(bridge, out_buf, 64) && strcmp(out_buf, bridge))
		blobmsg_add_string(buf, "parent", out_buf);


	vlan_tag = ovs_shell_br_to_vlan(bridge);
	if (vlan_tag > 0)
		blobmsg_add_u32(buf, "vlan", (uint32_t) vlan_tag);

	ovs_shell_capture_list(ovs_cmd(CMD_GET_OFCTL), bridge,
		"ofcontrollers", buf, false);
	ovs_shell_capture_string(ovs_cmd(CMD_GET_FAIL_MODE), bridge,
		"fail_mode", buf);
	ovs_shell_capture_list(ovs_cmd(CMD_LIST_PORTS), bridge, "ports",
		buf, false);

	return 0;
}

const char*
ovs_strerror(int error)
{
	switch (error) {
		case OVSD_ENOEXIST:
			return "does not exist";
		case OVSD_EINVALID_ARG:
			return "invalid argument";
		case OVSD_ENOPARENT:
			return "parent does not exist";
		case OVSD_EINVALID_VLAN:
			return "invalid VLAN tag";
		case OVSD_EUNKNOWN:
		default:
			return "unknown error";
	}
}
