for i in simple_nested_*; do echo -ne "$i:\t"; ./$i $1; done
