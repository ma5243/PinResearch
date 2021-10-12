for i in simple_*; do echo -ne "$i:\t"; ./$i $1; done
