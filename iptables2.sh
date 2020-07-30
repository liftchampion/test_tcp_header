

# nat source IP 10.50.0.1 -> 10.60.0.1 when going to 10.60.1.1 #NICE
iptables -t nat -A POSTROUTING -s 10.50.0.1 -d 10.60.1.1 -j SNAT --to-source 10.60.0.1

# nat inbound 10.60.0.1 -> 10.50.0.1 #NICE
iptables -t nat -A PREROUTING -d 10.60.0.1 -j DNAT --to-destination 10.50.0.1

# nat source IP 10.50.1.1 -> 10.60.1.1 when going to 10.60.0.1
#iptables -t nat -A POSTROUTING -s 10.50.1.1 -d 10.60.0.1 -j SNAT --to-source 10.60.1.1

# nat inbound 10.60.1.1 -> 10.50.1.1
#iptables -t nat -A PREROUTING -d 10.60.1.1 -j DNAT --to-destination 10.50.1.1