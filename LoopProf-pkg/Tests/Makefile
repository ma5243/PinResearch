OPT = -O0
DBG = -g
CFLAGS = -Wall $(OPT) $(DBG)

TESTS = hello threads calls simple

all: ${TESTS}

threads: threads.c
	gcc ${CFLAGS} -o threads -lpthread threads.c

clean:
	rm -f *.o ${TESTS} *~
