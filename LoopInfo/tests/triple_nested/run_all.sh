for i in triple_nested_*; do echo -ne "$i:\t"; ./$i $1; done
