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
#include "fs.h"
#include "dir.h"
#include "file.h"

#define DEFAULT_FILE_DISK "persistence_file.fisopfs"
#define PATH_MAX 4096  // Longitud máxima de una ruta absoluta

// absolute path for persistence file used in
// `.init` and `.destroy` FUSE operations
static char filedisk_path[2 * PATH_MAX];

static int
fisopfs_getattr(const char *path, struct stat *st)
{
	printf("[debug] fisopfs_getattr - path: %s\n", path);

	// Delegar a la función de directorios
	return dir_getattr(path, st);
}

static int
fisopfs_readdir(const char *path,
                void *buffer,
                fuse_fill_dir_t filler,
                off_t offset,
                struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_readdir - path: %s\n", path);

	// Los pseudo-directorios '.' y '..' se agregan siempre
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);

	// Obteniene el nodo del directorio (sin pasar tipos de FUSE a dir.c)
	Node *dir_node = dir_list(path);

	if (dir_node == NULL) {
		return -ENOENT;  // El directorio no existe o no es un directorio
	}

	// Itera sobre los hijos del directorio (archivos y subdirectorios) y los agrega con filler()
	for (int i = 0; i < dir_node->child_count; i++) {
		filler(buffer, dir_node->children[i]->name, NULL, 0);
	}

	return 0;
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

	(void) fi;  // No usado por ahora

	// Delega a la función de archivos
	ssize_t bytes_read = file_read(path, buffer, size, offset);

	if (bytes_read < 0) {
		return (int) bytes_read;  // Retorna código de error negativo
	}

	return (int) bytes_read;  // Retorna cantidad de bytes leídos
}

static int
fisopfs_write(const char *path,
              const char *buf,
              size_t size,
              off_t offset,
              struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_write - path: %s, offset: %lu, size: %lu\n",
	       path,
	       (unsigned long) offset,
	       (unsigned long) size);
	(void) fi;
	ssize_t written = file_write(path, buf, size, offset);
	if (written < 0)
		return (int) written; /* return negative errno-like values */
	return (int) written;
}
static int
fisopfs_truncate(const char *path, off_t size)
{
	printf("[debug] fisopfs_truncate - %s -> %ld\n", path, (long) size);
	ssize_t res = file_truncate(path, size);
	return (res < 0) ? (int) res : 0;
}

static int
fisopfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	printf("[debug] fisopfs_mknod - path: %s, mode: %o\n", path, mode);
	(void) rdev;

	return file_create(path, mode);
}

/* Wrapper FUSE: create */
static int
fisopfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	printf("[debug] fisopfs_create - path: %s\n", path);
	(void) fi; /* por ahora no usamos fi */
	return file_create(path, mode);
}

static int
fisopfs_unlink(const char *path)
{
	int res = file_unlink(path);

	if (res < 0) {
		return res;
	}

	return 0;
}

static int
fisopfs_mkdir(const char *path, mode_t mode)
{
	printf("[debug] fisopfs_mkdir - path: %s\n", path);

	return dir_create(path);
}

static int
fisopfs_rmdir(const char *path)
{
	printf("[debug] fisopfs_rmdir - path: %s\n", path);

	return dir_remove(path);
}

static void *
fisopfs_init(struct fuse_conn_info *conn)
{
	// se intenta abrir el archivo
	FILE *f = fopen(filedisk_path, "rb");
	// si no existe, se crea el FS desde 0 en memoria
	// si existe, se lee desde disco y se reconstruye el FS guardado.
	if (f) {
		fs_load_from_disk(f);
		fclose(f);
	} else {
		printf("%s\n", strerror(errno));
	}
	return NULL;
}

static void
fisopfs_destroy(void *private_data)
{
	// logica de persistencia en disco
	FILE *f = fopen(filedisk_path, "wb");
	if (!f) {
		perror(strerror(errno));
	}
	fs_persist(f);
	fclose(f);
}

static struct fuse_operations operations = {
	.getattr = fisopfs_getattr,
	.readdir = fisopfs_readdir,
	.read = fisopfs_read,
	.mkdir = fisopfs_mkdir,
	.rmdir = fisopfs_rmdir,
	.create = fisopfs_create,
	.mknod = fisopfs_mknod,
	.write = fisopfs_write,
	.truncate = fisopfs_truncate,
	.unlink = fisopfs_unlink,
	.init = fisopfs_init,
	.destroy = fisopfs_destroy,
};

int
main(int argc, char *argv[])
{
	fs_init();  // Inicializo el filesystem en memoria
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

	char cwd[PATH_MAX];
	if (getcwd(cwd, PATH_MAX) == NULL) {
		perror("getcwd failed");
		return 1;
	}
	snprintf(filedisk_path, sizeof(filedisk_path), "%s/%s", cwd, filedisk_name);
	return fuse_main(argc, argv, &operations, NULL);
}
