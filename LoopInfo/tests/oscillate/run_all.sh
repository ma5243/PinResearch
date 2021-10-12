for i in oscillate_*; do echo -ne "$i:\t"; ./$i $1; done
