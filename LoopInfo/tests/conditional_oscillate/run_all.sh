for i in conditional_oscillate_*; do echo -ne "$i:\t"; ./$i $1; done
