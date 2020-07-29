echo ''
echo '### RAW ###'
echo ''
iptables -t raw -L -n -v --line-numbers
echo ''
echo '### NAT ###'
echo ''
iptables -t nat -L -n -v --line-numbers
echo ''
echo '### MANGLE ###'
echo ''
iptables -t mangle -L -n -v --line-numbers
echo ''
echo '### SECURITY ###'
echo ''
iptables -t security -L -n -v --line-numbers
echo ''
echo '### FILTER ###'
echo ''
iptables -t filter -L -n -v --line-numbers
