
ip rule add from all lookup local # add one more local table lookup rule with high pref
ip rule del pref 0 # delete default local table lookup rule


ip rule add from 10.50.0.1 lookup 100 pref 100 # add rule to lookup new table before local table
ip route add 10.50.1.1  dev enp0s31f6.420 proto static scope link table 100


#ip rule add from 10.3.3.20 lookup 101 pref 101 # add rule to lookup new table before local table
#ip route add 10.3.3.10 dev enp0s31f6.420 proto static scope link src 10.3.3.20 table 101
#ip route add 10.3.3.255 dev enp0s31f6.420 proto static scope link src 10.3.3.20 table 101
#ip route add 10.3.3.0 dev enp0s31f6.420 proto static scope link src 10.3.3.20 table 101


ip route flush cache