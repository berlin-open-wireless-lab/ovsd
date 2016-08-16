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
#ifndef __OVSD_H
#define __OVSD_H

#include <libubus.h>

enum ovsd_status {
	OVSD_OK,
	OVSD_EUNKNOWN,
	OVSD_ENOEXIST,
	OVSD_ENOPARENT,
	OVSD_EINVALID_ARG,
	OVSD_EINVALID_VLAN,
};

enum ovsd_ovs_vsctl_status {
	OVS_VSCTL_STATUS_SUCCESS = 0,
	OVS_VSCTL_STATUS_ERROR = 1,
	OVS_VSCTL_STATUS_NONEXIST = 2,
};

enum {
	L_CRIT,
	L_WARNING,
	L_NOTICE,
	L_INFO,
	L_DEBUG
};

enum ovs_fail_mode {
	OVS_FAIL_MODE_STANDALONE,
	OVS_FAIL_MODE_SECURE,
};

struct ovswitch_br_config {
	char *name;

	// fake bridge args
	char *parent;
	unsigned int vlan_tag;

	// OpenFlow controller args
	char **ofcontrollers;
	int n_ofcontrollers;
	enum ovs_fail_mode fail_mode;

	// SSL args
	char *ssl_privkey_file;
	char *ssl_cert_file;
	char *ssl_cacert_file;
	bool ssl_bootstrap;
};

#define OVSWITCH_CONFIG_INIT {\
	.name = NULL,\
	.parent = NULL,\
	.vlan_tag = 0,\
	.ofcontrollers = NULL,\
	.n_ofcontrollers = 0,\
	.ssl_privkey_file = NULL,\
	.ssl_cert_file = NULL,\
	.ssl_cacert_file = NULL,\
	.ssl_bootstrap = false,\
}


void ovsd_log_msg(int log_lvl, const char *format, ...);

#endif
