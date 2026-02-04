// fs.h: Estructuras y funciones principales del filesystem (árbol de directorios, nodos, etc.)
#ifndef FS_H
#define FS_H

#include <sys/types.h>  // para uid_t, gid_t, mode_t
#include <time.h>       // para time_t
#include <stdio.h>

// Límites del filesystem (arrays estáticos)
#define MAX_NODES 256       // Máxima cantidad de archivos y directorios en todo el FS
#define MAX_CHILDREN 64     // Máxima cantidad de hijos (archivos o directorios) adentro de cada directorio
#define MAX_NAME_LEN 32     // Longitud máxima del nombre del archivo o directorio
#define MAX_FILE_SIZE 4096  // Tamaño máximo de un archivo en bytes
// Tipo de nodo (archivo o directorio)
typedef enum NodeType {
    FILE_NODE,
    DIRECTORY_NODE
} NodeType;

// Nodo del filesystem, representa tanto archivos como directorios (equivalente a un "inodo" simplificado)
typedef struct Node {
    char name[MAX_NAME_LEN];               // Nombre del archivo/directorio
    NodeType type;                         // Tipo de nodo (FILE_NODE o DIRECTORY_NODE)
    
    // Jerarquía de directorios (árbol)
    struct Node* parent;                   // Nodo padre (NULL para raíz)
    struct Node* children[MAX_CHILDREN];   // Array de hijos (solo si es directorio)
    int child_count;                       // Cantidad de hijos actuales

    // Metadatos
    uid_t uid;        // User ID del dueño
    gid_t gid;        // Group ID del dueño
    mode_t mode;      // Permisos (ej: 0755 para dirs, 0644 para archivos)
    time_t atime;     // Último acceso (access time)
    time_t mtime;     // Última modificación (modification time)
    time_t ctime;     // Último cambio de metadata (change time)
    off_t size;       // Tamaño en bytes (0 para directorios)

    // Contenido del archivo (solo usado si type == FILE_NODE)
    char data[MAX_FILE_SIZE];  // Array estático de 4KB
} Node;

// FileSystem global (vive en memoria durante la ejecución)
typedef struct {
    Node nodes[MAX_NODES];  // Array estático de todos los nodos
    int node_count;         // Cantidad de nodos creados
    Node* root;             // Recorre el path del arbol de directorios empezando desde el nodo raíz ("/").
} FileSystem;

typedef struct {
    int node_count; // cantidad de nodos
    size_t size; // tamaño en bytes del archivo
} SuperBlock;


// Variable global del filesystem (definida en fs.c)
extern FileSystem filesystem;

/*** === FUNCIONES PRINCIPALES DEL FILESYSTEM === ***/
 
// fs_init: Inicializa el filesystem creando el nodo raíz "/"
// PRE: Ninguna
// POST: fs.root apunta al nodo raíz, fs.node_count == 1
void fs_init(void);

// fs_create_node: Crea un nuevo nodo en el filesystem
// PRE: - name != NULL y no debe exceder de MAX_NAME_LEN
//      - parent == NULL solo para la raíz, en cualquier otro caso parent debe ser un nodo válido
//      - Si parent != NULL, debe ser un DIRECTORY_NODE con espacio de memoria suficiente en la jerarquía (child_count < MAX_CHILDREN)
// POST: - Si hay espacio suficiente en los nodos (node_count < MAX_NODES): retorna puntero al nuevo nodo agregado al árbol
//       - Si no hay espacio: retorna NULL
//       - El nodo creado tiene metadatos inicializados (uid, gid, timestamps, permisos y tamaño)
Node *fs_create_node(const char *name, NodeType type, Node *parent);

// fs_find_node: Busca un nodo en el árbol dado su path absoluto (Ejemplo: "/dir1/archivo.txt")
// PRE: - path != NULL y debe ser un path absoluto (empezar con '/')
// POST: - Si el path existe: retorna puntero al nodo encontrado
//       - Si el path no existe: retorna NULL
//       - El árbol no se modifica (función de solo lectura)
Node *fs_find_node(const char *path);


/**** FUNCIONES PARA PERSISTENCIA EN DISCO ****/

// recupera del disco la estructura del filesystem
// POST: devuelve 0 en caso de exito.
int fs_load_from_disk(FILE *f);

// guarda en disco el filesystem para luego ser recuperado
void fs_persist(FILE *f);

size_t serialized_size(int n);

void save_node(char* buf, Node *node, size_t* sz);

void serialize_node(char *buf, Node *node, size_t* sz);

Node* load_node(char* buffer, size_t* offset, Node* parent);

void deserialize_node(Node* node, char* buffer, size_t* offset);

void write_buf(char* buf, size_t* final_sz, const void *src, size_t sz);

void read_buf(void* target, size_t* offset, const char* buf, size_t sz);



#endif