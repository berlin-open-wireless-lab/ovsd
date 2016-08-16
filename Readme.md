# ovsd

ovsd is a daemon providing external device handler functionality for Open vSwitch devices in conjunction with netifd.
It allows to integrate Open vSwitch configuration into the network interface configuration file `/etc/config/network` in the OpenWRT/LEDE router operating system. 
Using the netifd external device handler extension, it receives commands to create and configure Open vSwitch devices from netifd and relays them to the Open vSwitch software using the ovs-vsctl command-line interface.

## Installation


Install this as a feed by adding the following line to `feeds.conf` in your OpenWRT/LEDE source tree:
```
src-git ovs https://gitlab.inet.tu-berlin.de/SDWN/packages-ovs.git
```
Next, run
```bash
scripts/feeds update ovs
scripts/feeds install ovsd
```
from the same directory. There should now be a package `ovsd` selectable in `Network` when you run
`make menuconfig`.

## Configuration

This example configuration demonstrates all the options ovsd understands:

```bash
# /etc/config/network

config interface 'lan'
    option ifname 'eth0'
    option type 'Open vSwitch'
    option proto 'static'
    option ipaddr '172.17.1.123'
    option gateway '172.17.1.1'
    option netmask '255.255.255.0'
    option ip6assign '60'
	option ofcontrollers 'tcp:1.2.3.4:5678'
	option controller_fail_mode 'standalone'
	option ssl_cert '/root/cert.pem'
	option ssl_private_key '/root/key.pem'
	option ssl_ca_cert '/root/cacert_bootstrap.pem'
	option ssl_bootstrap 'true'

config interface 'fake'
	option type 'Open vSwitch'
	option proto 'static'
	option ipaddr '172.17.1.124'
	option netmask '255.255.255.0'
	option parent 'ovs-lan'
	option vlan '2'
	option empty 'true'
```
This creates two Open vSwitch bridges, ovs-lan and ovs-fake. The prefix 'ovs-' is the default prefix for Open vSwitch devices defined in `/lib/netifd/ubusdev-config/ovsd.json`. 

These are the the options specific to ovsd that are given for ovs-lan:
 - **ofcontrollers** is a list of strings giving the addresses of the bridge's OpenFlow controllers. Please refer to the [ovs-vsctl manpage](http://manpages.ubuntu.com/manpages/trusty/man8/ovs-vsctl.8.html) for the exact format of the addresses.
 - **controller_fail_mode** can be set to either 'standalone' (default) or 'secure'. Standalone makes the bridge fall back to standard learning switch behavior in case of controller failure. Secure disables the installation of new flows while the controller is not connected.
 - **ssl_cert**, **ssl_private_key**, **ssl_ca_cert**: paths to PEM files containing an SSL certificate, SSL private key and CA certificate, respectively. To enable transport layer encryption, all three options must be given.
 - **ssl_bootstrap**: optional boolean flag enabling a trust-on-first-use controller connection to retrieve the CA cert. This facilitates setup but is vulnerable to man-in-the-middle attacks. Please refer to the [ovs-vsctl manpage](http://manpages.ubuntu.com/manpages/trusty/man8/ovs-vsctl.8.html) for further detail.

ovs-fake has some other options set:
- **parent**: Name of another non-fake Open vSwitch bridge. Setting this makes the bridge a fake-bridge or pseudo-bridge created on top of the parent bridge. Note, that the parent bridge is called 'ovs-lan' despite the interface being called 'lan'. This is due to the bridge device prefix given in `/lib/netifd/ubusdev-config/ovsd.json`.
- **vlan**: 802.1q VLAN tag for the fake-bridge. To create a fake bridge both the parent and VLAN options must be given.

## Contact

Please post to the Google group [ovsd-dev](https://groups.google.com/forum/#!forum/ovsd-dev) if you have problems with or suggestions for ovsd.

## Acknowledgements

This work was done as part of the 2016 Google Summer of Code. I would like to thank my advisor and mentor Julius Schulz-Zander for introducing me to GSoC and his counsel throughout the process.
This project started as a student project at Technische Universität Berlin under his guidance and was finished during GSoC.
A big thank you also to Felix Fietkau, author and maintainer of netifd, from whom I have learned a lot and who has given me feedback on my work. 
Thanks to Freifunk, who hosted the project and, of course, to Google for organizing GSoC.