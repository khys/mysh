mysh: main.o
	gcc -o mysh main.o

main.o: main.c
	gcc -c main.c

clean:
	\rm mysh *.o
