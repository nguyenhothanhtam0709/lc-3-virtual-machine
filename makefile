build:
	gcc main.c -std=c2x -o main

dev: build
	./main

.PHONY: build dev