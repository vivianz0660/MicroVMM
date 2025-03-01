all: main guest_obj_nasm guest_bin

main:
	gcc main.c -lncurses -o vmm

guest_obj_nasm:
	nasm -f elf32 guest_nasm.asm -o guest.o

guest_bin:
	ld -m elf_i386 -Ttext 0x1000 --oformat binary -o guest guest.o

guest_obj_gas:
	as --32 -o guest.o  guest_gas.s


