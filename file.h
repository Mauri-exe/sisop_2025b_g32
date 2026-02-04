#ifndef FILE_H
#define FILE_H

#include <sys/types.h> /* mode_t */

/* Crea un archivo en la ruta `path` con permisos `mode`.
 * Devuelve 0 en éxito o -errno en caso de error.
 */
int file_create(const char *path, mode_t mode);
/* Lee hasta `size` bytes del archivo en `path`, comenzando desde `offset`,
 * copiándolos en `buffer`.
 * Devuelve la cantidad de bytes leídos, o un valor negativo en caso de error.
 */
ssize_t file_read(const char *path, char *buffer, size_t size, off_t offset);
ssize_t file_write(const char* path, const char* buf, size_t size,
                         off_t offset);
ssize_t file_truncate(const char *path, off_t new_size);
int file_unlink(const char *path);
#endif /* FILE_H */
