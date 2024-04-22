#define _XOPEN_SOURCE 700
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ext2fs/ext2fs.h>

static void
usage(char *argv0)
{
	fprintf(stderr, "usage: %s filesystem\n", argv0 ? argv0 : "fspec-extfs");
	exit(1);
}

static void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		putchar(' ');
		perror(NULL);
	} else {
		putchar('\n');
	}
	exit(1);
}

static int
filetype(const char *type)
{
	if (strcmp(type, "reg") == 0)
		return S_IFREG;
	if (strcmp(type, "dir") == 0)
		return S_IFDIR;
	if (strcmp(type, "sym") == 0)
		return S_IFLNK;
	fatal("unknown file type '%s'", type);
	return 0;
}

static void
writefile(ext2_filsys fs, ext2_ino_t ino, int fd)
{
	ext2_file_t file;
	errcode_t err;
	char buf[8192], *pos;
	ssize_t ret;

	err = ext2fs_file_open(fs, ino, EXT2_FILE_WRITE, &file);
	if (err)
		fatal("ext2fs_file_open: %s", error_message(err));
	while ((ret = read(fd, buf, sizeof(buf))) > 0) {
		unsigned int n;

		pos = buf;
		while (ret > 0) {
			err = ext2fs_file_write(file, pos, ret, &n);
			if (err)
				fatal("ext2fs_file_write: %s", error_message(err));
			pos += n;
			ret -= n;
		}
	}
	if (ret != 0)
		fatal("read:");
	err = ext2fs_file_close(file);
	if (err)
		fatal("ext2fs_file_close: %s", error_message(err));
}

int
main(int argc, char **argv)
{
	ext2_filsys fs;
	errcode_t err;
	char *line = NULL;
	size_t len = 0;
	ssize_t n;

	if (argc != 2 && argc != 3)
		usage(argv[0]);
	if (argc == 3) {
		if (!freopen(argv[2], "r", stdin))
			fatal("open %s:", argv[2]);
	}

	add_error_table(&et_ext2_error_table);
	err = ext2fs_open(argv[1], EXT2_FLAG_RW, 0, 0, unix_io_manager, &fs);
	if (err)
		fatal("ext2fs_open: %s", error_message(err));
	err = ext2fs_read_bitmaps(fs);
	if (err)
		fatal("ext2fs_read_bitmaps: %s", error_message(err));

	do {
		ext2_ino_t dir, ino;
		char name[NAME_MAX], target[PATH_MAX], source[PATH_MAX], *end;
		mode_t mode = 0;
		int fd = -1;

		/* skip blank lines */
		while ((n = getline(&line, &len, stdin)) == 1)
			;
		if (n <= 0)
			break;
		if (line[n - 1] == '\n')
			line[n - 1] = '\0';
		end = strrchr(line, '/');
		if (end) {
			*end++ = '\0';
			fatal("todo");
		} else {
			end = line;
			dir = EXT2_ROOT_INO;
		}
		if (!memccpy(name, end, '\0', sizeof(name)))
			fatal("filename too long");
		while ((n = getline(&line, &len, stdin)) > 1) {
			if (line[n - 1] == '\n')
				line[n - 1] = '\0';
			if (strncmp(line, "uid=", 4) == 0) {
			} else if (strncmp(line, "gid=", 4) == 0) {
			} else if (strncmp(line, "perm=", 5) == 0) {
				mode |= strtol(line + 5, NULL, 8);
			} else if (strncmp(line, "mode=", 5) == 0) {
				mode |= filetype(line + 5);
			} else if (strncmp(line, "target=", 7) == 0) {
				if (!memccpy(target, line + 7, '\0', sizeof(target)))
					fatal("target too long");
			} else if (strncmp(line, "source=", 7) == 0) {
				if (!memccpy(source, line + 7, '\0', sizeof(source)))
					fatal("source too long");
			} else {
				fprintf(stderr, "unknown attribute line '%s'\n", line);
				exit(1);
			}
		}
		err = ext2fs_new_inode(fs, dir, mode, 0, &ino);
		if (err)
			fatal("ext2fs_new_inode: %s", error_message(err));
		switch (mode & S_IFMT) {
		struct stat st;
		struct ext2_inode inode;

		case S_IFREG:
			err = ext2fs_link(fs, dir, name, ino, EXT2_FT_REG_FILE);
			if (err)
				fatal("ext2fs_link: %s", error_message(err));
			ext2fs_inode_alloc_stats2(fs, ino, 1, 0);
			inode = (struct ext2_inode){
				.i_mode = LINUX_S_IFREG | mode,
				.i_links_count = 1,
			};
			fd = open(source, O_RDONLY);
			if (fd < 0)
				fatal("open %s:", source);
			if (fstat(fd, &st) != 0)
				fatal("stat %s:", source);
			err = ext2fs_write_new_inode(fs, ino, &inode);
			if (err)
				fatal("ext2fs_write_new_inode: %s", error_message(err));
			writefile(fs, ino, fd);
			close(fd);
			break;
		case S_IFDIR:
			err = ext2fs_mkdir(fs, dir, ino, name);
			if (err)
				fatal("ext2fs_mkdir: %s", error_message(err));
			break;
		case S_IFLNK:
			err = ext2fs_symlink(fs, dir, ino, name, target);
			if (err)
				fatal("ext2fs_symlink: %s", error_message(err));
			break;
		}
	} while (n != -1);

	if (ferror(stdin))
		fatal("read:");
	err = ext2fs_close(fs);
	if (err)
		fatal("ext2fs_close: %s", error_message(err));
	return 0;
}
