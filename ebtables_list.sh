
#echo ''
#echo '############### RAW'
#echo ''
#ebtables -t raw -L --Ln --Lc
#
#echo ''
#echo '############### BROUTE'
#echo ''
#ebtables -t broute -L --Ln --Lc

echo ''
echo '############### NAT'
echo ''
ebtables -t nat -L --Ln --Lc

echo ''
echo '############### FILTER'
echo ''
ebtables -t filter -L --Ln --Lc