#include "fs.h"
#include <string.h>
#include <errno.h>
#include <libgen.h>     // para dirname() y basename()
#include <sys/stat.h>   // para struct stat y macros S_IFDIR, S_IFREG

// Obtiene los atributos (metadatos) de un nodo dado su path
// Llena la estructura stat con la información del nodo
// Retorna: 0 en éxito, -errno en error
int dir_getattr(const char *path, struct stat *st) {
    // Busco el nodo en el árbol
    Node *node = fs_find_node(path);
    if (node == NULL) {
        return -ENOENT;  // El path no existe
    }

    // Lleno la estructura stat con los metadatos del nodo
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_size = node->size;
    st->st_atime = node->atime;
    st->st_mtime = node->mtime;
    st->st_ctime = node->ctime;
    // Configuro el tipo de nodo y permisos
    if (node->type == DIRECTORY_NODE) {
        st->st_mode = __S_IFDIR | node->mode;  // Directorio (usar S_IFDIR, no S_ISDIR)
        st->st_nlink = 2;  // Enlaces: '.' y '..'

        // suma 1 por cada subdirectorio
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i]->type == DIRECTORY_NODE)
                st->st_nlink++;
        }
    } else {
        st->st_mode = __S_IFREG | node->mode;  // Archivo regular (usar S_IFREG, no S_ISREG)
        st->st_nlink = 1;  //Enlaces: 1 por archivo
    }

    return 0;  // Éxito
}

int dir_create(const char *path){
    // Antes de crear un nuevo directorio, la función necesita separar el path en:
    // - parent_path: el directorio donde crear (ej: "/dir1")
    // - dir_name: el nombre del nuevo directorio (ej: "subdir2")
    
    // dirname() y basename() MODIFICAN el string que reciben, por eso necesita crear copias temporales
    char temp_for_dirname[MAX_NAME_LEN * 4];
    char temp_for_basename[MAX_NAME_LEN * 4];

    // Copia el path a la variable temporal para dirname
    strncpy(temp_for_dirname, path, sizeof(temp_for_dirname) - 1);
    temp_for_dirname[sizeof(temp_for_dirname) - 1] = '\0';

    // Copia el path a la variable temporal para basename
    strncpy(temp_for_basename, path, sizeof(temp_for_basename) - 1);
    temp_for_basename[sizeof(temp_for_basename) - 1] = '\0';
    
    // Extrae el directorio padre y el nombre
    char *parent_path = dirname(temp_for_dirname);   // ej: "/dir1"
    char *dir_name = basename(temp_for_basename);    // ej: "subdir2"
    
    // Y por último, se deben cumplir estas condiciones antes de crear el nuevo directorio: 
    // 1. Busca el nodo padre
    Node *parent = fs_find_node(parent_path);
    if (parent == NULL) {
        return -ENOENT;  // El directorio padre no existe
    }

    // 2. Verifica que el padre sea un directorio
    if (parent->type != DIRECTORY_NODE) {
        return -ENOTDIR;  // El padre no es un directorio (es un archivo)
    }

    // 3. Verifica que no exista ya un hijo con ese mismo nombre que el nuevo directorio
    for (int i = 0; i < parent->child_count; i++){
        if (strcmp(parent->children[i]->name, dir_name) == 0){
            return -EEXIST;  // Ya existe un archivo o directorio con ese mismo nombre
        }
    }
    
    // Ahora se crea el nuevo directorio
    Node *new_dir = fs_create_node(dir_name, DIRECTORY_NODE, parent);

    // Si no hay espacio para crear el nuevo nodo (MAX_NODES alcanzado):
    if (new_dir == NULL) {
        return -ENOMEM;
    }

    return 0;  // Fue creado exitosamente
}

Node *dir_list(const char *path) {
    // Busca el nodo del directorio en el árbol
    Node *node = fs_find_node(path);

    if (node == NULL) {
        return NULL;  // El path no existe
    }

    if (node->type != DIRECTORY_NODE) {
        return NULL;  // El path existe pero no es un directorio
    }

    // Retorna el nodo para que el llamador pueda iterar node->children[]
    return node;
}

int dir_remove(const char *path) {

    if (strcmp(path, "/") == 0) {
        return -EBUSY;  // Device or resource busy
    }

    // Busca el nodo a eliminar
    Node *node = fs_find_node(path);
    if (node == NULL) {
        return -ENOENT;  // El path no existe
    }

    if (node->type != DIRECTORY_NODE) {
        return -ENOTDIR;  // El path no es un directorio
    }

    if (node->child_count > 0) {
        return -ENOTEMPTY;  // Directory not empty
    }

    // Ahora se elimina el nodo del array de hijos del padre
    Node *parent = node->parent;
    
    // Busca el índice del nodo en el array de hijos del padre
    int found = 0;
    int i = 0;
    while (found == 0 && i < parent->child_count) {
        if (parent->children[i] == node) {
            // Mueve todos los hijos siguientes a una posición hacia atrás (compacta el array eliminando el hueco)
            for (int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }

            // Reduce el contador de hijos del padre
            parent->child_count--;

            found = 1;  // Marca que ya lo encontró
        }
        i++;
    }

    // Si no se encontró el nodo en los hijos del padre (error inesperado)
    if (found == 0) {
        return -EIO;  // I/O error (error genérico)
    }

    // Por las dudas limpia campos del nodo dentro del array estático
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

    return 0; // La operación fue un éxito
}