#define _GNU_SOURCE

#include "file.h"
#include "fs.h"
#include <libgen.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

int file_create(const char *path, mode_t mode) {
    if (path == NULL || path[0] != '/') {
        return -EINVAL;
    }

    /* dirname() y basename() modifican el string → hay que copiarlos */
    char temp_for_dirname[MAX_NAME_LEN * 4];
    char temp_for_basename[MAX_NAME_LEN * 4];

    strncpy(temp_for_dirname, path, sizeof(temp_for_dirname) - 1);
    temp_for_dirname[sizeof(temp_for_dirname) - 1] = '\0';

    strncpy(temp_for_basename, path, sizeof(temp_for_basename) - 1);
    temp_for_basename[sizeof(temp_for_basename) - 1] = '\0';

    char *parent_path = dirname(temp_for_dirname);
    char *file_name   = basename(temp_for_basename);

    /* 1) Busca el padre */
    Node *parent = fs_find_node(parent_path);
    if (parent == NULL) {
        return -ENOENT;
    }

    /* 2) Verifica que el padre sea un directorio */
    if (parent->type != DIRECTORY_NODE) {
        return -ENOTDIR;
    }

    /* 3) Verifica que no exista ya un hijo con ese nombre */
    for (int i = 0; i < parent->child_count; i++) {
        if (strcmp(parent->children[i]->name, file_name) == 0) {
            return -EEXIST;
        }
    }

    /* 4) Crea nodo de archivo */
    Node *new_file = fs_create_node(file_name, FILE_NODE, parent);
    if (new_file == NULL) {
        return -ENOMEM;
    }

    /*
       NO necesitamos:
       - setear mode (ya lo hace fs_create_node → 0644)
       - setear size (ya lo hace → 0)
       - agregar como hijo (ya lo hace)
       - timestamps (ya lo hace)
    */

    return 0;
}
// Lee 'size' bytes desde el archivo en 'path' comenzando en 'offset'.
// Escribe en 'buffer' y devuelve la cantidad de bytes leídos.
ssize_t file_read(const char *path, char *buffer, size_t size, off_t offset) {
    // Busca el nodo
    Node *node = fs_find_node(path);
    if (node == NULL)
        return -ENOENT;  // Archivo no existe

    if (node->type != FILE_NODE)
        return -EISDIR;  // No es un archivo (es un directorio)

    // No podés leer más allá del archivo
    if (offset >= node->size)
        return 0; // Offset fuera del archivo, no hay nada para leer

    size_t max_read = node->size - offset;
    if (size > max_read)
        size = max_read;

    // Copia del array estático
    memcpy(buffer, node->data + offset, size);

    // Actualiza atime
    node->atime = time(NULL);

    return size;
}
ssize_t file_write(const char* path, const char* buf, size_t size, off_t offset)
{
    Node* node = fs_find_node(path);
    if (!node) return -ENOENT;
    if (node->type != FILE_NODE) return -EISDIR;

    // Verifica que no se exceda el tamaño máximo del archivo
    if (offset + size > MAX_FILE_SIZE) {
        // Trunca la escritura al espacio disponible
        if (offset >= MAX_FILE_SIZE) {
            return -EFBIG;  // Offset ya está fuera del límite
        }
        size = MAX_FILE_SIZE - offset;  // Ajustar tamaño
    }
    // Si escribimos más allá del tamaño actual, rellenamos el "hole" con ceros
    if (offset > node->size) {
        memset(node->data + node->size, 0, offset - node->size);
    }

    // Copia datos al array estático
    memcpy(node->data + offset, buf, size);

    // Actualiza tamaño del archivo si creció
    if (offset + size > node->size) {
        node->size = offset + size;
    }

    // Actualiza metadata
    node->mtime = time(NULL);
    node->ctime = time(NULL);

    return size;
}
ssize_t file_truncate(const char *path, off_t new_size) {
    Node *node = fs_find_node(path);
    if (!node) return -ENOENT;

    if (node->type != FILE_NODE)
        return -EISDIR;

    if (new_size < 0)
        return -EINVAL;

    if (new_size > MAX_FILE_SIZE)
        return -EFBIG;  // Archivo demasiado grande

    // Si agranda el archivo → rellenar con ceros
    if (new_size > node->size) {
        memset(node->data + node->size, 0, new_size - node->size);
    }

    // Actualiza tamaño y timestamps 
    node->size = new_size;
    node->mtime = time(NULL);
    node->ctime = time(NULL);

    return 0;
}


int file_unlink(const char *path) {
    // No permitir borrar la raíz por equivocación
    if (strcmp(path, "/") == 0) {
        return -EBUSY;
    }

    // 1) Buscar el nodo
    Node *node = fs_find_node(path);
    if (node == NULL) {
        return -ENOENT; // no existe
    }

    // 2) Verificar que sea un archivo
    if (node->type != FILE_NODE) {
        return -EISDIR; // es un directorio
    }

    // 3) Obtener el padre
    Node *parent = node->parent;
    if (parent == NULL) {
        return -EINVAL; // nodo sin padre (no debería ocurrir salvo raíz)
    }

    // 4) Buscar el índice del nodo en parent->children
    int found = 0;
    int i = 0;
    while (!found && i < parent->child_count) {
        if (parent->children[i] == node) {
            // compactar: mover todos los siguientes una posición atrás
            for (int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            found = 1;
        }
        i++;
    }

    if (!found) {
        return -EIO; // error inesperado: no estaba entre los hijos del padre
    }

    // Limpia el buffer de datos (opcional, por seguridad)
    memset(node->data, 0, MAX_FILE_SIZE);

    // Limpia campos del nodo dentro del array estático
    node->name[0] = '\0';    // marcar nombre vacío (ranura libre visible)
    node->parent = NULL;
    node->child_count = 0;
    node->size = 0;
    node->mode = 0;
    node->uid = 0;
    node->gid = 0;
    node->atime = node->mtime = node->ctime = 0;

    // Actualiza el contador
    filesystem.node_count--;
    return 0;
}