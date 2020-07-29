RAW_IFACE=enp0s31f6
#VLAN_NAME=enp3s0f1np1.1
VLAN_NAME=ivlan420
VLAN_ID=420

#modprobe 8021q

ip link add link ${RAW_IFACE} name ${VLAN_NAME} type vlan id ${VLAN_ID}
ip -d link show ${VLAN_NAME}

ip addr add 10.5.5.10/24 brd 10.5.5.255 dev ${VLAN_NAME}
ip link set dev ${VLAN_NAME} up

#ip link set dev ${VLAN_NAME} down
#ip link delete ${VLAN_NAME}
