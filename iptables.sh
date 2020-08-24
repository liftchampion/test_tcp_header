

iptables -t nat -A PREROUTING -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t nat -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t raw -A PREROUTING -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t mangle -A PREROUTING -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t mangle -A FORWARD -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t filter -A FORWARD -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t mangle -A INPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t filter -A INPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t raw -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t mangle -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t filter -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t mangle -A POSTROUTING -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t nat -A POSTROUTING -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t security -A INPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t security -A FORWARD -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t security -A OUTPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT

iptables -t nat -A INPUT -s 10.3.3.10 -d 10.3.3.20 -m length --length 1: -j ACCEPT