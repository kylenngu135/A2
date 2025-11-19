CC=gcc
CFLAGS=-pthread -I. -Wall -Wno-int-conversion -D_GNU_SOURCE -fcommon

#binaries=queueprodcons cpa pthread_mult
binaries=pcMatrix

all: $(binaries)

pcMatrix: counter.c prodcons.c matrix.c pcmatrix.c
	$(CC) $(CFLAGS) $^ -o ./bin/$@

clean:
	$(RM) -fr ./bin
	$(RM) -f *.o
