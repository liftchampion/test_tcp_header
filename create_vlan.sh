RAW_IFACE=enp3s0f1np1
VLAN_NAME=enp3s0f1np1.1
#VLAN_NAME=V224
VLAN_ID=1

#modprobe 8021q

ip link add link ${RAW_IFACE} name ${VLAN_NAME} type vlan id ${VLAN_ID}
ip -d link show ${VLAN_NAME}

ip addr add 192.168.40.108/24 brd 192.168.40.255 dev ${VLAN_NAME}
ip link set dev ${VLAN_NAME} up

#ip link set dev ${VLAN_NAME} down
#ip link delete ${VLAN_NAME}
