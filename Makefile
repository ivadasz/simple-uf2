BINARY=mkuf2

${BINARY}: mkuf2.c
	gcc -O2 -lelf -o $@ mkuf2.c

clean:
	rm -f ${BINARY}
