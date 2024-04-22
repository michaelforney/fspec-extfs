.POSIX:

-include config.mk

EXTFS_CFLAGS?=$$(pkg-config --cflags ext2fs com_err)
EXTFS_LDFLAGS?=$$(pkg-config --libs-only-L --libs-only-other ext2fs com_err)
EXTFS_LDLIBS?=$$(pkg-config --libs-only-l ext2fs com_err)

CFLAGS+=-Wall -Wpedantic

COMMON_OBJ=fatal.o parse.o reallocarray.o

.PHONY: all
all: fspec-extfs

fspec-extfs.o: fspec-extfs.c
	$(CC) $(CFLAGS) $(EXTFS_CFLAGS) -c -o $@ fspec-extfs.c

fspec-extfs: fspec-extfs.o
	$(CC) $(LDFLAGS) $(EXTFS_LDFLAGS) -o $@ fspec-extfs.o $(EXTFS_LDLIBS)

.PHONY: clean
clean:
	rm -f fspec-extfs fspec-extfs.o
