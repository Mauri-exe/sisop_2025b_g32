#ifndef DIR_H
#define DIR_H

#include <sys/types.h>
#include <sys/stat.h>  // para struct stat

/*** === OPERACIONES SOBRE DIRECTORIOS === ***/

// dir_getattr: Obtiene los atributos (metadatos) de un nodo
// PRE: - path != NULL y debe ser un path absoluto válido
// POST: - Si el path existe: llena st con los metadatos y retorna 0
//       - Si el path no existe: retorna -ENOENT
int dir_getattr(const char *path, struct stat *st);

// dir_create: Crea un nuevo directorio en el path especificado
// PRE: - path != NULL y debe ser un path absoluto válido (ej: "/dir1/subdir1")
//      - El directorio padre debe existir y ser un DIRECTORY_NODE
//      - No debe existir ya un nodo con ese mismo nombre que el nuevo directorio en el padre
//      - Debe haber espacio disponible tanto en el array de nodos (node_count < MAX_NODES) como en la jerarquía del padre (parent->child_count < MAX_CHILDREN)
// POST: - Si se cumplen las precondiciones: retorna 0 y el directorio se agrega al árbol
//       - Si el padre no existe: retorna -ENOENT (ENOENT: No such file or directory)
//       - Si el padre no es directorio: retorna -ENOTDIR (ENOTDIR: Not a directory)
//       - Si ya existe: retorna -EEXIST (EEXIST: File exists)
//       - Si no hay espacio: retorna -ENOMEM (ENOMEM: Out of memory)
int dir_create(const char *path);

// dir_list: Obtiene un puntero al nodo del directorio para iterar sus hijos
// PRE: - path != NULL y debe ser un path absoluto válido (ej: "/dir1")
// POST: - Si el path existe y es un directorio: retorna puntero al nodo
//       - Si el path no existe: retorna NULL
//       - Si el path no es un directorio: retorna NULL
Node *dir_list(const char *path);

// dir_remove: Elimina un directorio vacío del filesystem
// PRE: - path != NULL y debe ser un path absoluto válido (ej: "/dir1")
//      - El directorio debe estar vacío (child_count == 0)
//      - No se puede eliminar la raíz "/"
// POST: - Si se cumplen las precondiciones: retorna 0 y el directorio se elimina del árbol
//       - Si el path no existe: retorna -ENOENT (ENOENT: No such file or directory)
//       - Si el path no es un directorio: retorna -ENOTDIR (ENOTDIR: Not a directory)
//       - Si el directorio NO está vacío: retorna -ENOTEMPTY (ENOTEMPTY: Directory not empty)
//       - Si se intenta borrar la raíz: retorna -EBUSY (EBUSY: Device or resource busy)
int dir_remove(const char *path);

#endif
