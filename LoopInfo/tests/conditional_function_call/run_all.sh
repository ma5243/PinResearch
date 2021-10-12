for i in conditional_function_call_*; do echo -ne "$i:\t"; ./$i $1; done
