build: main.c
	gcc -std=c99 -o space_invaders main.c -Wall -Wextra -ggdb

run: build
	./space_invaders