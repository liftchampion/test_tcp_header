arp -d 10.3.3.10 -i enp0s31f6.420
arp -d 10.3.3.20 -i vlan420

arp -s -D -i enp0s31f6.420 10.3.3.10  vlan420
arp -s -D -i vlan420       10.3.3.20  enp0s31f6.420