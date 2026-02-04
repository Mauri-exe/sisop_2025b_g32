#include "fs.h"
#include <string.h>
#include <unistd.h>  // para getuid(), getgid()
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Variable global del filesystem
FileSystem filesystem;

// Flag para indicar que el fs esta en proceso de ser cargado desde el disco.
static int loading_from_disk = 0;
// Tamaño maximo de un nodo serializado
#define PER_NODE_SERIALIZED_SIZE ( \
    MAX_NAME_LEN + sizeof(NodeType) + sizeof(int) + \
    sizeof(uid_t) + sizeof(gid_t) + sizeof(mode_t) + \
    3 * sizeof(time_t) + sizeof(off_t) + MAX_FILE_SIZE )
// Tamaño maximo estimado de la serializacion
#define MAX_SERIALIZED_SIZE (sizeof(SuperBlock) + (MAX_NODES * PER_NODE_SERIALIZED_SIZE))

Node *fs_find_node(const char *path) {
    // Caso especial: la raíz "/"
    if (strcmp(path, "/") == 0) {
        // Devuelve el nodo raíz
        return filesystem.root;
    }

    // Empieza desde la raíz y va bajando por el árbol
    Node *current = filesystem.root;
    
    // Copia el path (sin el primer '/') para poder dividir los componentes de la ruta
    char temp[MAX_NAME_LEN * 4];
    strncpy(temp, path + 1, sizeof(temp));  // Saltea el '/' inicial
    temp[sizeof(temp) - 1] = '\0'; // Asegura que termine con null

    // Divide el path por '/' y busca cada componente
    char *token = strtok(temp, "/");

    while (token != NULL){
        int found = 0;
        int finished = 0;
        
        // Busca el token entre los hijos del directorio actual
        int i = 0;
        while (i < current->child_count && !finished){
            if (strcmp(current->children[i]->name, token) == 0){
                current = current->children[i]; // Baja un nivel
                found = 1; // Marca que ya lo encontró
                finished = 1;
            }
            i++;
        }
                    
        // Si no se encontró el componente, el path no existe
        if (!found) {
            return NULL;
        }
        
        // Avanza al siguiente componente del path
        token = strtok(NULL, "/");  
    }
    
    return current;
}

Node *fs_create_node(const char *name, NodeType type, Node *parent){
    // Verifica que no se exceda el límite de nodos
    if (filesystem.node_count >= MAX_NODES) {
        return NULL;
    }

    // Crea el nuevo nodo en la siguiente ranura disponible del array estático
    Node *new_node = &filesystem.nodes[filesystem.node_count++];
    
    // Inicializa el nombre y su tipo
    strncpy(new_node->name, name, MAX_NAME_LEN);
    new_node->name[MAX_NAME_LEN - 1] = '\0';  // Se asegura que termine con null
    new_node->type = type;
    new_node->parent = parent;
    memset(new_node->children, 0, sizeof(new_node->children));
    new_node->child_count = 0;

    // Inicializa los metadatos del nuevo nodo con valores por defecto
    new_node->uid = getuid();   // Usuario actual del sistema
    new_node->gid = getgid();   // Grupo actual del sistema
    new_node->size = 0;         // Tamaño inicial 0 (los archivos siempre empiezan siendo vacíos)

    // Timestamps: todos se inicializan al momento de creación
    time_t now = time(NULL);
    new_node->atime = now;  // Access time: último acceso
    new_node->mtime = now;  // Modification time: última modificación
    new_node->ctime = now;  // Change time: último cambio de metadata
    
    // Permisos por defecto según el tipo
    if (type == DIRECTORY_NODE) {
        new_node->mode = 0755;  // rwxr-xr-x (directorios navegables)
    } else {
        new_node->mode = 0644;  // rw-r--r-- (archivos normales)
    }

    // Inicializa buffer de datos a ceros (solo relevante para archivos)
    memset(new_node->data, 0, MAX_FILE_SIZE);

    // Agrega el nodo como hijo del padre (si tiene padre)
    if (parent != NULL && parent->child_count < MAX_CHILDREN && !loading_from_disk){
        parent->children[parent->child_count++] = new_node;
    }

    return new_node;
}

void fs_init(void){
    filesystem.node_count = 0;
    // Crea el directorio raíz
    filesystem.root = fs_create_node("/", DIRECTORY_NODE, NULL);
}

// copia en el buffer src, para luego actualizar el final size
void write_buf(char* buf, size_t* final_sz, const void *src, size_t sz){
    // buf: buffer destino
    // final_sz: offset tamaño buffer
    // src: dato a copiar
    // sz: tamaño dato a copiar
    memcpy(buf + *final_sz, src, sz);
    *final_sz += sz;
}

void read_buf(void* target, size_t* offset, const char* buf, size_t sz){
    memcpy(target, buf + *offset, sz);
    *offset += sz;
}

// escribe la metadata del nodo en el buffer
void serialize_node(char *buf, Node *node, size_t* sz){
    write_buf(buf, sz, node->name, MAX_NAME_LEN);
    write_buf(buf, sz, &node->type, sizeof(node->type));
    write_buf(buf, sz, &node->child_count, sizeof(int));
    write_buf(buf, sz, &node->uid, sizeof(uid_t));
    write_buf(buf, sz, &node->gid, sizeof(gid_t));
    write_buf(buf, sz, &node->mode, sizeof(mode_t));
    write_buf(buf, sz, &node->atime, sizeof(time_t));
    write_buf(buf, sz, &node->mtime, sizeof(time_t));
    write_buf(buf, sz, &node->ctime, sizeof(time_t));
    write_buf(buf, sz, &node->size, sizeof(off_t));
    if (node->type == FILE_NODE) {
        if (node->size > 0) {
            write_buf(buf, sz, node->data, (size_t)node->size);
        }
    }
}
// funcion recursiva para el guardado de nodos en el buffer
void save_node(char* buf, Node *node, size_t* sz){
    if (!node) return;
    // guardo nodo
    serialize_node(buf, node, sz);
    // si es de tipo directorio, guardo sus hijos de manera contigua
    if (node->type == DIRECTORY_NODE) {
        for (int i = 0; i < node->child_count; i++) {
            save_node(buf, node->children[i], sz);
        }
    }
}

void fs_persist(FILE *f){

    if (!f) return;

    SuperBlock sb;
    sb.node_count = filesystem.node_count;
    sb.size = 0; /* se calculará después */

    /* buffer estático máximo */
    char buffer[MAX_SERIALIZED_SIZE];
    size_t final_size = 0;

    /* Serializo todo empezando desde root en preorder */
    save_node(buffer, filesystem.root, &final_size);

    /* Actualizo superbloque con tamaño real (bytes de payload) */
    sb.size = final_size + sizeof(SuperBlock); /* opcional: size total archivo */

    /* Escribo SuperBlock y luego payload */
    if (fseek(f, 0, SEEK_SET) != 0) {
        perror("fs_persist: fseek");
        return;
    }

    if (fwrite(&sb, sizeof(SuperBlock), 1, f) != 1) {
        perror("fs_persist: fwrite(superblock)");
        return;
    }

    if (final_size > 0) {
        if (fwrite(buffer, sizeof(char), final_size, f) != final_size) {
            perror("fs_persist: fwrite(buffer)");
            return;
        }
    }

    fflush(f);
}

void deserialize_node(Node* node, char* buffer, size_t* offset){
    // aca se lee a partir del campo de child_count
    // si es de tipo FILE_NODE, entonces recupero la data
    read_buf(&node->child_count, offset, buffer, sizeof(int));
    read_buf(&node->uid, offset, buffer, sizeof(uid_t));
    read_buf(&node->gid, offset, buffer, sizeof(gid_t));
    read_buf(&node->mode, offset, buffer, sizeof(mode_t));
    read_buf(&node->atime, offset, buffer, sizeof(time_t));
    read_buf(&node->mtime, offset, buffer, sizeof(time_t));
    read_buf(&node->ctime, offset, buffer, sizeof(time_t));
    read_buf(&node->size, offset, buffer, sizeof(off_t));
    if(node->type == FILE_NODE){
        if (node->size > 0) {
            if ((size_t)node->size > MAX_FILE_SIZE) {
                fprintf(stderr, "deserialize_node: size too large\n");
                node->size = 0;
            } else {
                read_buf(node->data, offset, buffer, (size_t)node->size);
            }
        }
    }
}

Node* load_node(char* buffer, size_t* offset, Node* parent){
    char name[MAX_NAME_LEN];
    NodeType type;

    read_buf(name, offset, buffer, MAX_NAME_LEN);
    read_buf(&type, offset, buffer, sizeof(NodeType));

    Node* n = fs_create_node(name, type, parent);
    deserialize_node(n, buffer, offset);

    if(n->type == DIRECTORY_NODE){
        for(int i = 0; i < n->child_count; i++){
            Node* child = load_node(buffer, offset, n);
            n->children[i] = child;
        }
    }
    return n;
}

int fs_load_from_disk(FILE* f){
    // Si el archivo no es valido, no se puede cargar el fs.
    if (!f) return 1;

    // Aviso a la logica interna que estamos cargando desde disco
    // para evitar que ciertas funciones modifiquen metadatos mientras cargamos
    loading_from_disk = 1;

    // Me posiciono al final del archivo para obtener su tamaño total
    if (fseek(f, 0, SEEK_END) != 0) {
        perror("fs_load_from_disk: fseek end");
        loading_from_disk = 0;
        return 1;
    }

    // Obtengo el tamaño del archivo del disco
    long file_size = ftell(f);

    // Si el archivo es mas chico que un superbloque, no puede contener un fs valido
    if (file_size < (long)sizeof(SuperBlock)) {
        rewind(f);
        loading_from_disk = 0;
        return 1;
    }

    // Regreso al inicio para comenzar a leer
    rewind(f);

    SuperBlock sb;

    // Leo el superbloque desde el disco
    if (fread(&sb, sizeof(SuperBlock), 1, f) != 1) {
        perror("fs_load_from_disk: fread(superblock)");
        loading_from_disk = 0;
        return 1;
    }

    // Calculo bytes que quedan luego del superbloque
    size_t payload_size = (size_t)file_size - sizeof(SuperBlock);

    // Si no hay mas bytes, el fs está vacío o corrupto -> error
    if (payload_size == 0) {
        loading_from_disk = 0;
        return 1;
    }

    // Verifico que el tamaño restante no exceda nuestro limite de serialización
    if (payload_size > MAX_SERIALIZED_SIZE) {
        fprintf(stderr, "fs_load_from_disk: payload too large (%zu)\n", payload_size);
        loading_from_disk = 0;
        return 1;
    }

    // Creo un buffer temporal para leer el resto de los nodos serializados
    char buffer[MAX_SERIALIZED_SIZE];

    // Leo la data serializada completa (todos los nodos)
    size_t read_bytes = fread(buffer, 1, payload_size, f);
    if (read_bytes != payload_size) {
        perror("fs_load_from_disk: fread(payload)");
        loading_from_disk = 0;
        return 1;
    }

    // Antes de reconstruir el árbol en memoria, limpio el estado actual
    filesystem.node_count = 0;
    filesystem.root = NULL;

    // Reconstruyo el nodo raíz y recursivamente toda la jerarquía de hijos
    // El padre del root es NULL
    size_t offset = 0;
    Node *root = load_node(buffer, &offset, NULL);
    if (!root) {
        fprintf(stderr, "fs_load_from_disk: failed to load root\n");
        loading_from_disk = 0;
        return 1;
    }

    // Guardo la raiz restaurada en nuestro filesystem en memoria
    // Ya termine la carga, vuelvo el flag a 0
    filesystem.root = root;
    loading_from_disk = 0; 
    return 0; // carga exitosa
}

size_t serialized_size(int n){
    size_t sz = sizeof(SuperBlock);
    sz += (size_t)n * PER_NODE_SERIALIZED_SIZE;
    return sz;
}
