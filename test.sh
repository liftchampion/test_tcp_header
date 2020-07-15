GATE=192.168.40.254
REMOTE=192.168.40.103
LOCAL=192.168.40.108

echo 0 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/conf/enp0s31f6/rp_filter
echo 0 > /proc/sys/net/ipv4/conf/enp3s0f1np1/rp_filter

/sbin/sysctl -w net.ipv4.conf.all.accept_redirects=0
/sbin/sysctl -w net.ipv4.conf.all.send_redirects=0

ip rule add from all lookup local # add one more local table lookup rule with high pref
ip rule del pref 0 # delete default local table lookup rule
ip rout add ${REMOTE} via ${GATE} src ${LOCAL} table 100 # add correct route to some table
ip rout add ${LOCAL} via ${GATE} src ${REMOTE} table 100 # add correct route to some table
ip rule add from all lookup 100 pref 1001 # add rule to lookup new table before local table
