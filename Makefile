
all: spy_on read_page cycle_jump

spy_on: spy_on.c
	gcc -o spy_on spy_on.c


read_page: read_page.c
	gcc -o read_page read_page.c


cycle_jump: cycle_jump.c
	gcc -o cycle_jump cycle_jump.c

clean:
	rm -f spy_on read_page cycle_jump