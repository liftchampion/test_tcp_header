ip route del table local 192.168.40.103 dev enp0s31f6
ip route add table local 192.168.40.103 dev enp3s0f1np1
ip route del table local 192.168.40.108 dev enp3s0f1np1
ip route add table local 192.168.40.108 dev enp0s31f6
ip route flush cache

echo 0 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/conf/enp0s31f6/rp_filter
echo 0 > /proc/sys/net/ipv4/conf/enp3s0f1np1/rp_filter

/sbin/sysctl -w net.ipv4.conf.all.accept_redirects=0
/sbin/sysctl -w net.ipv4.conf.all.send_redirects=0
