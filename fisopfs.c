#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define DEFAULT_FILE_DISK "persistence_file.fisopfs"

// absolute path for persistence file used in
// `.init` and `.destroy` FUSE operations
static char filedisk_path[2 * PATH_MAX];

static int
fisopfs_getattr(const char *path, struct stat *st)
{
	printf("[debug] fisopfs_getattr - path: %s\n", path);

	if (strcmp(path, "/") == 0) {
		st->st_uid = 1717;
		st->st_mode = __S_IFDIR | 0755;
		st->st_nlink = 2;
	} else if (strcmp(path, "/fisop") == 0) {
		// TODO: remove hardcoded file
		st->st_uid = 1818;
		st->st_mode = __S_IFREG | 0644;
		st->st_size = 2048;
		st->st_nlink = 1;
	} else {
		return -ENOENT;
	}

	return 0;
}

static int
fisopfs_readdir(const char *path,
                void *buffer,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_readdir - path: %s\n", path);

	// pseudo directories '.' y '..'
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	// TODO: remove hardcoded file
	if (strcmp(path, "/") == 0) {
		filler(buffer, "fisop", NULL, 0);
		return 0;
	}

	return -ENOENT;
}

static int
fisopfs_read(const char *path,
             char *buffer,
             size_t size,
             off_t offset,
             struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_read - path: %s, offset: %lu, size: %lu\n",
	       path,
	       offset,
	       size);

	// TODO: remove hardcoded file
	static char fisop_file_contenidos[100] = "hola fisopfs!\n";

	// TODO: remove hardcoded file
	if (strcmp(path, "/fisop") != 0)
		return -ENOENT;

	if (offset + size > strlen(fisop_file_contenidos))
		size = strlen(fisop_file_contenidos) - offset;

	size = size > 0 ? size : 0;

	memcpy(buffer, fisop_file_contenidos + offset, size);

	return size;
}

static struct fuse_operations operations = {
	.getattr = fisopfs_getattr,
	.readdir = fisopfs_readdir,
	.read = fisopfs_read,
};

int
main(int argc, char *argv[])
{
	char *filedisk_name = DEFAULT_FILE_DISK;

	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "--filedisk") == 0) {
			filedisk_name = argv[i + 1];

			// we remove the argument so that FUSE doesn't use our
			// argument or name as folder.
			//
			// equivalent to a pop.
			for (int j = i; j < argc - 1; j++) {
				argv[j] = argv[j + 2];
			}

			argc = argc - 2;
			break;
		}
	}

	// handle absolute path for persistence file
	// so background executions can work properly
	//
	// Hint: use `getcwd(3)` before `fuse_main`
	//
	// TODO: build absolute path in `filedisk_path`
	strcpy(filedisk_path, filedisk_name);

	return fuse_main(argc, argv, &operations, NULL);
}
