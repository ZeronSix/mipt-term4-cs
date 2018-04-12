#!/bin/bash
MC=11
rm output
# echo "N   real   user   %" >> output
for ((k = 1; k < 2; k++))
do
	echo -n "1, " >> output
	ONETIME=`(time -p ./integral 1 ) |& grep "real" | awk '{print $2}'`
	echo ${ONETIME} >> output
	echo -e "[1 / ${MC}]\e[32mDone\e[0m"

	for ((i = 2; i < ${MC}+1; i++))
	do
		echo -n "$i, " >> output
		(time -p ./integral $i ) |& grep "real" | awk '{print $2, " ", '$ONETIME'/('$i'*($2))}' >> output
		echo -e "[$i / ${MC}]\e[32mDone\e[0m"
	done
	echo "$k"
    cat output
done
rm output
