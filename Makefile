CC = gcc
CFLAGS := -ggdb3 -O2 -Wall -std=c11
CFLAGS += -Wno-unused-function -Wvla

# Flags for FUSE
LDLIBS := $(shell pkg-config fuse --cflags --libs)

# Name for the filesystem!
FS_NAME := fisopfs

all: $(FS_NAME)

# Regla para compilar fisopfs con todos los módulos
# Primero compila los .o de cada módulo y luego los enlaza con fisopfs.c
$(FS_NAME): fisopfs.c fs.o dir.o file.o
	$(CC) $(CFLAGS) fisopfs.c fs.o dir.o file.o $(LDLIBS) -o $(FS_NAME)

format: .clang-files .clang-format
	xargs -r clang-format -i <$<

build:
	./dock build

run:
	./dock run

exec:
	./dock exec

clean:
	rm -rf $(EXEC) *.o core vgcore.* $(FS_NAME)

test:
	@chmod +x tests/run_tests.sh tests/test_*.sh
	@./tests/run_tests.sh

.PHONY: all build clean format test docker-build docker-run docker-exec
