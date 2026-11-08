"""The rules that keep Windows away from this PC and the local network
(brief 5.3), in an nftables table of their own. libvirt's network only
adds NAT; a drop in any table is final, so these hold whatever else the
PC's firewall says.

Windows may reach the internet, and this PC's DNS and DHCP on the bridge.
This PC may reach Windows (Remote Desktop goes that way). With
file_sharing, Windows may also reach this PC's file sharing on the
bridge address only, for the shared folders over Samba (brief 5.7).
"""

import subprocess

TABLE = "wildos_win"
PRIVATE = "10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 169.254.0.0/16, 100.64.0.0/10, 224.0.0.0/4, 255.255.255.255"


def rules(bridge, host, guest, file_sharing=False):
    smb = (f'        iifname "{bridge}" ip saddr {guest} ip daddr {host} tcp dport 445 accept\n' if file_sharing else "")
    return f"""table inet {TABLE}
delete table inet {TABLE}
table inet {TABLE} {{
    chain input {{
        type filter hook input priority filter - 5; policy accept;
        iifname "{bridge}" ct state established,related accept
        iifname "{bridge}" udp sport 68 udp dport 67 accept
        iifname "{bridge}" ip daddr {host} udp dport 53 accept
        iifname "{bridge}" ip daddr {host} tcp dport 53 accept
{smb}        iifname "{bridge}" counter drop
    }}
    chain forward {{
        type filter hook forward priority filter - 5; policy accept;
        iifname "{bridge}" ct state established,related accept
        iifname "{bridge}" meta nfproto ipv6 counter drop
        iifname "{bridge}" ip daddr {{ {PRIVATE} }} counter drop
        oifname "{bridge}" ct state new counter drop
    }}
}}
"""


def apply(cfg, file_sharing=False):
    text = rules(cfg["bridge"], cfg["host_address"], cfg["guest_address"], file_sharing)
    p = subprocess.run(["nft", "-f", "-"], input=text, text=True, capture_output=True)
    return p.returncode == 0, p.stderr.strip()
