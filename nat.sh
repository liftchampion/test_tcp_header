

#iptables -t raw -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j DNAT --to-destination 13.13.13.13
#iptables -t raw -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j DNAT --to-destination 13.13.13.13

#iptables -t raw -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 --jump ACCEPT
#iptables -t raw -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 --jump ACCEPT

#iptables -t raw -A PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j NOTRACK
#iptables -t raw -A PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j NOTRACK


#iptables -t mangle -A PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j TTL --ttl-set 234
#iptables -t mangle -A PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j TTL --ttl-set 234
#
#iptables -t mangle -A PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j MARK --set-mark 13
#iptables -t mangle -A PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j MARK --set-mark 13

#iptables -t mangle -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TTL --ttl-set 234
#iptables -t mangle -A OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TTL --ttl-set 234
#
#iptables -t mangle -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j MARK --set-mark 13
#iptables -t mangle -A OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j MARK --set-mark 13


#iptables -t filter -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j DNAT --to-destination 13.13.13.13
#iptables -t filter -A OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j DNAT --to-destination 13.13.13.13

#iptables -t mangle -A POSTROUTING -s 10.3.3.10 -d 10.3.3.20 -j TTL --ttl-set 234
#iptables -t mangle -A POSTROUTING -s 10.3.3.20 -d 10.3.3.10 -j TTL --ttl-set 234

iptables -t mangle -A POSTROUTING -s 10.3.3.10 -d 10.3.3.20 -j SNAT --to-source 13.13.13.13
iptables -t mangle -A POSTROUTING -s 10.3.3.20 -d 10.3.3.10 -j SNAT --to-source 13.13.13.13