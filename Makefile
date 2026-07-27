all:
	gcc main.c input.c output.c fileIO.c editor.c terminal.c row_operations.c syntax_highlighting.c -o main.elf

clean:
	rm -f *.o *.elf
