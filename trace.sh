
modprobe ipt_LOG
modprobe nf_log_ipv4
#sysctl net.netfilter.nf_log.2=nf_log_ipv4
sysctl net.netfilter.nf_log.2=nf_log_ipv4
sysctl -p

#iptables -t raw -I PREROUTING -j TRACE
iptables -t raw -I PREROUTING -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t raw -I PREROUTING -s 10.3.3.20 -d 10.3.3.10 -j TRACE

iptables -t raw -I OUTPUT -s 10.3.3.10 -d 10.3.3.20 -j TRACE
iptables -t raw -I OUTPUT -s 10.3.3.20 -d 10.3.3.10 -j TRACE




#iptables -t raw -L -n -v --line-numbers


## ENABLE KERNEL LOGGING
# edit /etc/rsyslog.conf

# reload
# systemctl restart rsyslog