CC=gcc
CFLAGS=-Wall
FILE = a1.c
EXE = image_tagger


all:
	$(CC) $(CFLAGS) $(FILE) -o $(EXE)

clean:
	rm $(EXE)
