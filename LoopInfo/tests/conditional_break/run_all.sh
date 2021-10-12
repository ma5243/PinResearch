for i in conditional_break_*; do echo -ne "$i:\t"; ./$i $1; done
