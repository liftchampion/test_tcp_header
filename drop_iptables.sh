iptables -t raw -F
iptables -t raw -X

iptables -t nat -F
iptables -t nat -X

iptables -t mangle -F
iptables -t mangle -X

iptables -t security -F
iptables -t security -X

iptables -t filter -F
iptables -t filter -X