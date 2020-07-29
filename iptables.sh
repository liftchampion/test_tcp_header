

#iptables -t nat -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
#iptables -t nat -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE
#
#iptables -t nat -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
#iptables -t nat -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t raw -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t raw -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t mangle -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t mangle -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t nat -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t nat -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t mangle -I FORWARD -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t mangle -I FORWARD -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t filter -I FORWARD -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t filter -I FORWARD -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t mangle -I INPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t mangle -I INPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t filter -I INPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t filter -I INPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t raw -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t raw -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t mangle -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t mangle -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t nat -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t nat -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t filter -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t filter -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t mangle -I POSTROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t mangle -I POSTROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t nat -I POSTROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t nat -I POSTROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE








iptables -t security -I INPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t security -I INPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t security -I FORWARD -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t security -I FORWARD -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t security -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t security -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE








iptables -t nat -I INPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t nat -I INPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE