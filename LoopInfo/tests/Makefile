# Makefile
#
# Add program names to "TARGETS" for them to get copiled when running "make"
#
# You can also do:
# 	make <program_name>
# Which will compile just the targeted program

CC = gcc
CFLAGS  = -g -Wall
TARGETS = simple simple2 simple_nested triple_nested oscillate conditional_oscillate simple_function_call simple_function_call2 conditional_break conditional_function_call conditional_function_call2 nested_conditional_cont nested_change_iter

all: $(TARGETS)

.c:
	mkdir -p $@; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=1 -O0 $< -o $@/$@_for_O0; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=1 -O1 $< -o $@/$@_for_O1; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=1 -O2 $< -o $@/$@_for_O2; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=1 -O3 $< -o $@/$@_for_O3; \
	\
	$(CC) $(CFLAGS) -DLOOP_TYPE=2 -O0 $< -o $@/$@_while_O0; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=2 -O1 $< -o $@/$@_while_O1; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=2 -O2 $< -o $@/$@_while_O2; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=2 -O3 $< -o $@/$@_while_O3; \
	\
	$(CC) $(CFLAGS) -DLOOP_TYPE=3 -O0 $< -o $@/$@_do_O0; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=3 -O1 $< -o $@/$@_do_O1; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=3 -O2 $< -o $@/$@_do_O2; \
	$(CC) $(CFLAGS) -DLOOP_TYPE=3 -O3 $< -o $@/$@_do_O3; \
	echo "for i in $@_*; do echo -ne \"\$$i:\\t\"; ./\$$i \$$@; done" > $@/run_all.sh; \
	chmod +x $@/run_all.sh

clean: 
	$(RM) -rf $(TARGETS)
