# fisopfs - Sistema de Archivos FUSE

Este documento describe la implementación del sistema de archivos **fisopfs**, un filesystem tipo FUSE que opera completamente en memoria RAM utilizando estructuras de datos estáticas.

---

## Índice

1. [Diseño e Implementación](#diseño-e-implementación)
   - [Arquitectura General](#arquitectura-general)
   - [Estructuras de Datos](#estructuras-de-datos)
   - [Operaciones sobre Archivos](#operaciones-sobre-archivos)
   - [Operaciones sobre Directorios](#operaciones-sobre-directorios)
   - [Algoritmo de Búsqueda de Paths](#algoritmo-de-búsqueda-de-paths)
   - [Modularización](#modularización)
   - [Persistencia en Disco](#persistencia-en-disco)
2. [Pruebas y Validación](#pruebas-y-validación)
   - [Tests Automatizados](#tests-automatizados)
   - [Pruebas Manuales](#pruebas-manuales)

---

## Diseño e Implementación

### Arquitectura General

El sistema de archivos **fisopfs** está diseñado para operar enteramente en memoria RAM durante su ejecución. La estructura fundamental es un **árbol de nodos** donde cada nodo puede representar tanto un archivo como un directorio. Esta implementación utiliza **arrays estáticos** para evitar la complejidad del manejo dinámico de memoria y garantizar un rendimiento predecible.

**Ejemplo de un diagrama conceptual del árbol de directorios:**

```
                (nodo raíz)
                   / | \
                  /  |  \
              dir1 dir2 archivo.txt
               |     |
               |     |
            subdir  archivo2.txt
```

El filesystem soporta:
- ✅ Creación y eliminación de directorios (`mkdir`, `rmdir`)
- ✅ Listado de contenido de directorios (`ls`, `readdir`)
- ✅ Obtención de metadatos (`stat`, `getattr`)
- ✅ Soporte para subdirectorios (al menos un nivel de recursión)

---

### Estructuras de Datos

La implementación se basa en dos estructuras principales definidas en `fs.h`:

#### 1. Estructura `Node`

Representa tanto archivos como directorios en el filesystem. Cada nodo contiene:

```c
typedef struct Node {
    char name[MAX_NAME_LEN];               // Nombre del archivo/directorio
    NodeType type;                         // Tipo de nodo (FILE_NODE o DIRECTORY_NODE)
    
    // Jerarquía de directorios (árbol)
    struct Node* parent;                   // Nodo padre (NULL para raíz)
    struct Node* children[MAX_CHILDREN];   // Array de hijos (solo si es directorio)
    int child_count;                       // Cantidad de hijos actuales

    // Metadatos
    uid_t uid;        // Usuario ID del dueño
    gid_t gid;        // Grupo ID del dueño
    mode_t mode;      // Permisos (ej: 0755 para dirs, 0644 para archivos)
    time_t atime;     // Último acceso (access time)
    time_t mtime;     // Última modificación (modification time)
    time_t ctime;     // Último cambio de metadata (change time)
    off_t size;       // Tamaño en bytes (0 para directorios)

    // Contenido del archivo (solo usado si type == FILE_NODE)
    char data[MAX_FILE_SIZE];  // Array estático de 4KB
} Node;
```

**Decisión de diseño:** Se utiliza un array estático `children[MAX_CHILDREN]` en lugar de listas enlazadas o memoria dinámica. Esto simplifica la gestión de memoria y evita fugas (memory leaks), al costo de limitar cada directorio a un máximo de 16 hijos.

#### 2. Estructura `FileSystem`

Contiene el estado global del filesystem:

```c
typedef struct {
    Node nodes[MAX_NODES];  // Array estático de TODOS los nodos (máx. 256)
    int node_count;         // Contador de nodos actualmente usados
    Node* root;             // Recorre el path del arbol de directorios empezando desde el nodo raíz ("/").
} FileSystem;
```

**Decisión de diseño:** El array `nodes[MAX_NODES]` almacena todos los nodos (archivos y directorios) en un solo lugar. Esto permite:
- Acceso O(1) por índice
- Límite claro de capacidad (128 nodos totales)
- Serialización sencilla para persistencia en disco

**Constantes del sistema:**

```c
#define MAX_NODES 256       // Máximo de archivos + directorios en total
#define MAX_CHILDREN 64     // Máximo de hijos por directorio
#define MAX_NAME_LEN 32     // Longitud máxima de nombre
```

Cuando las operaciones se sobrepasan alguno de estos límites, retornan el código de error `-ENOMEM`.

---
### Operaciones sobre Archivos

Se implementaron 5 operaciones principales para el manejo de archivos:

#### 1. file_create() - Crear Archivos
* Valida que el padre exista y sea directorio
* Verifica que no exista ya un archivo con ese nombre
* Usa fs_create_node() con tipo FILE_NODE
* Error codes: -ENOENT, -ENOTDIR, -EEXIST, -ENOMEM

#### 2. file_read() - Leer Contenido
* Copia datos desde node->data al buffer del usuario
* Maneja offsets y trunca si se lee más allá del tamaño
* Actualiza atime (access time)
* Retorna cantidad de bytes leídos o código de error

#### 3. file_write() - Escribir Datos
* Soporta escrituras con offset (modificar en el medio)
* Rellena "holes" con ceros cuando offset > size
* Limita escrituras a MAX_FILE_SIZE (4096 bytes)
* Actualiza mtime y ctime

#### 4. file_truncate() - Cambiar Tamaño
* Permite agrandar (rellena con ceros) o achicar archivos
* Valida límites (0 <= size <= MAX_FILE_SIZE)
* Actualiza metadata

#### 5. file_unlink() - Eliminar Archivos
* Remueve el nodo del array de hijos del padre
* Limpia el buffer de datos (memset)
* Marca la ranura como libre (name[0] = '\0')
* NO permite borrar directorios (retorna -EISDIR)

---

### Operaciones sobre Directorios

Se implementaron cuatro operaciones principales para el manejo de directorios, definidas en `dir.h` e implementadas en `dir.c`:

#### 1. `dir_getattr()` - Obtiene los metadatos de un archivo/directorio

**Propósito:** Devuelve los atributos de un archivo o directorio (equivalente a la syscall `stat(2)`).

La función `dir_getattr()` (implementada en `dir.c`) es responsable de llenar la estructura `struct stat` de POSIX con los metadatos de archivos y directorios. Esta operación es fundamental para que comandos como `ls -l`, `stat`, y otras herramientas del sistema puedan consultar información sobre los nodos del filesystem.

**Flujo de ejecución:**
1. **Búsqueda del nodo**: Utiliza `fs_find_node()` para localizar el nodo en el árbol
2. **Validación**: Retorna `-ENOENT` si el path no existe
3. **Población de struct stat**: Llena todos los campos relevantes de la estructura

**Implementación de Estadísticas (Metadatos):**

La implementación cumple con los requisitos de la consigna respecto a estadísticas, utilizando las syscalls `getuid(2)` y `getgid(2)` para obtener el usuario y grupo actual del sistema, y manteniendo timestamps de acceso y modificación.

**1. Asignación de UID/GID (Usuario y Grupo)**

Cuando se crea cualquier nodo (archivo o directorio) mediante `fs_create_node()` en `fs.c`, se asignan automáticamente el UID y GID del proceso actual:

```c
new_node->uid = getuid();   // Usuario actual del sistema
new_node->gid = getgid();   // Grupo actual del sistema
```

Esto garantiza que todos los archivos y directorios creados pertenezcan al usuario que ejecuta el filesystem FUSE.

**2. Manejo de Timestamps (Fechas de acceso y modificación)**

El filesystem mantiene tres timestamps según el estándar POSIX:

- **`atime` (access time)**: Última vez que se accedió al contenido del archivo
  - Se inicializa al momento de creación en `fs_create_node()`
  - Se actualiza cada vez que se lee un archivo en `file_read()`
    ```c
    node->atime = time(NULL);
    ```

- **`mtime` (modification time)**: Última vez que se modificó el contenido del archivo
  - Se inicializa al momento de creación
  - Se actualiza en operaciones de escritura:
    - `file_write()`
    - `file_truncate()`

- **`ctime` (change time)**: Última vez que se modificaron los metadatos del archivo
  - Se inicializa al momento de creación
  - Se actualiza junto con `mtime` en operaciones que modifican el archivo:
    ```c
    node->mtime = time(NULL);
    node->ctime = time(NULL);
    ```

**3. Asignación de Permisos**

Los permisos se asignan automáticamente según el tipo de nodo en `fs_create_node()`:

```c
if (type == DIRECTORY_NODE) {
    new_node->mode = 0755;  // rwxr-xr-x (directorios navegables)
} else {
    new_node->mode = 0644;  // rw-r--r-- (archivos normales)
}
```

- **Directorios**: `0755` permite que el owner pueda (propietario) leer, escribir y ejecutar (navegar), mientras que otros solo pueden leer y navegar.
- **Archivos**: `0644` permite que el owner (propietario) pueda leer y escribir, mientras que otros solo pueden leer.

**Código de referencia:**

```c
int dir_getattr(const char *path, struct stat *st) {
    Node *node = fs_find_node(path);
    if (node == NULL) {
        return -ENOENT;
    }

    // Metadatos básicos
    st->st_uid = node->uid;
    st->st_gid = node->gid;
    st->st_size = node->size;
    st->st_atime = node->atime;
    st->st_mtime = node->mtime;
    st->st_ctime = node->ctime;

    // Tipo y permisos
    if (node->type == DIRECTORY_NODE) {
        st->st_mode = __S_IFDIR | node->mode;
        st->st_nlink = 2;
    } else {
        st->st_mode = __S_IFREG | node->mode;
        st->st_nlink = 1;
    }

    return 0;
}
```

#### 2. `dir_create()`

**Propósito:** Crea un nuevo directorio en el path especificado (equivalente a `mkdir(2)`).

**Flujo de ejecución:**
1. Separa el path en **directorio padre** y **nombre** usando `dirname()` y `basename()`
   - Ejemplo: `/dir1/subdir` --> padre: `/dir1`, nombre: `subdir`
2. Busca el nodo padre con `fs_find_node()`
3. **Validaciones:**
   - Si el nodo padre no existe --> retorna `-ENOENT`
   - Si el nodo padre no es directorio --> retorna `-ENOTDIR`
   - Si ya existe el nodo hijo con el mismo nombre --> retorna `-EEXIST`
4. Crea el nodo con `fs_create_node()`
5. Si no hay espacio --> retorna `-ENOMEM`

**Dato importante:** Las funciones `dirname()` y `basename()` **modifican** el string que reciben, por eso se crean copias temporales antes de llamarlas:

```c
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
```

#### 3. `dir_list()`

**Propósito:** Obtiene el nodo de un directorio para iterar sobre sus hijos (usado por `readdir`).

**Flujo de ejecución:**
1. Busca el nodo con `fs_find_node()`
2. Verifica que sea un directorio
3. Retorna el puntero al nodo (o `NULL` si no existe o no es directorio)

**Decisión de modularización:** Esta función NO recibe ni retorna tipos de FUSE (`fuse_fill_dir_t`, etc.). En su lugar, retorna un puntero a `Node` para respetar la separación de responsabilidades. El código en `fisopfs.c` se encarga de iterar el array `children[]` y llamar a `filler()`:

```c
// Obteniene el nodo del directorio (sin pasar tipos de FUSE a dir.c)
Node *dir_node = dir_list(path);

if (dir_node == NULL) {
	return -ENOENT;  // El directorio no existe o no es un directorio
}

// Itera sobre los hijos del directorio (archivos y subdirectorios) y los agrega con filler()
for (int i = 0; i < dir_node->child_count; i++) {
	filler(buffer, dir_node->children[i]->name, NULL, 0);
}
```

#### 4. `dir_remove()`

**Propósito:** Elimina un directorio vacío (equivalente a `rmdir(2)`).

**Flujo de ejecución:**
1. **Validación especial:** No se puede borrar la raíz `/` --> retorna `-EBUSY`
2. Busca el nodo con `fs_find_node()`
3. **Validaciones:**
   - Si no existe --> retorna `-ENOENT`
   - Si no es un directorio --> retorna `-ENOTDIR`
   - Si tiene hijos (`child_count > 0`) --> retorna `-ENOTEMPTY`
4. **Eliminación del array del padre:**
   - Busca el índice del nodo en `parent->children[]`
   - Compacta el array moviendo todos los elementos siguientes una posición atrás
   - Decrementa `parent->child_count`

**Algoritmo de eliminación de un inodo directorio y compactación del array de nodos del filesystem:**

```c
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
```

---

### Algoritmo de Búsqueda de Paths

La función **`fs_find_node()`** es fundamental para todas las operaciones del filesystem. Dado que un path absoluto (ejemplo: `/dir1/subdir/`) debe encontrar el nodo correspondiente recorriendo el árbol.

**Algoritmo:**

1. **Caso base:** Si el path es `"/"`, retorna el nodo raíz directamente
2. **Tokenización:** Divide el path en componentes usando `strtok()` con delimitador `'/'`
   - Ejemplo: `/dir1/subdir` --> tokens: `["dir1", "subdir"]`
3. **Recorrido del árbol:**
   - Empieza desde la raíz (`current = filesystem.root`)
   - Por cada token:
     - Busca entre los hijos del nodo actual (`current->children[]`)
     - Si encuentra un hijo con ese nombre, avanza a ese nodo (`current = hijo`)
     - Si no lo encuentra, retorna `NULL` (el path no existe)
   - Continúa hasta procesar todos los tokens
4. **Retorno:** Devuelve el nodo encontrado (o `NULL` si algún componente no existía)

**Ejemplo visual:**

Para buscar `/dir1/subdir`:

```
Paso 1: Token = "dir1"
   current = root --> busca "dir1" en root->children[] --> encuentra dir1 --> current = dir1

Paso 2: Token = "subdir"  
   current = dir1 --> busca "subdir" en dir1->children[] --> encuentra subdir --> current = subdir

Paso 3: No hay más tokens
   Retorna current (apunta a subdir)
```

**Complejidad temporal:** O(n × m) donde:
- n = número de componentes en el path
- m = número promedio de hijos por directorio (máx. 64)

**Nota de implementación:** Se usa `strtok()` que modifica el string, por eso se hace una copia temporal del path:

```c
// Empieza desde la raíz y va bajando por el árbol
Node *current = filesystem.root;

// Copia el path (sin el primer '/') para poder dividir los componentes de la ruta
char temp[MAX_NAME_LEN * 4];
strncpy(temp, path + 1, sizeof(temp));  // Saltea el '/' inicial
temp[sizeof(temp) - 1] = '\0'; // Asegura que termine con null

// Divide el path por '/' y busca cada componente
char *token = strtok(temp, "/");

while (token != NULL) {
    // Busca el token entre los hijos...
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

    // Avanza al siguiente componente del path
    token = strtok(NULL, "/");  
}
```

---

### Modularización

El código sigue el principio de **separación de responsabilidades** dividiendo la implementación en tres capas:

#### Capa 1: Primitivas del Filesystem (`fs.c` / `fs.h`)

**Responsabilidad:** Operaciones básicas sobre el árbol de nodos.

**Funciones:**
- `fs_init()`: Inicializa el filesystem creando el nodo raíz
- `fs_create_node()`: Crea un nuevo nodo en el árbol
- `fs_find_node()`: Busca un nodo dado un path

**Características:**
- No conoce nada sobre FUSE
- Opera únicamente con tipos propios (`Node`, `FileSystem`)
- Puede ser testeado de forma independiente

#### Capa 2: Operaciones de Directorios (`dir.c` / `dir.h`)

**Responsabilidad:** Lógica de las operaciones específicas de directorios.

**Funciones:**
- `dir_getattr()`: Obtiene metadatos
- `dir_create()`: Crea directorios con validaciones
- `dir_list()`: Lista contenido
- `dir_remove()`: Elimina directorios vacíos

**Características:**
- **Abstracción de FUSE:** NO incluye `<fuse.h>` ni usa tipos como `fuse_fill_dir_t`
- Retorna estructuras estándar de POSIX (`struct stat`) o tipos propios (`Node*`)
- Maneja validaciones y errores según semántica POSIX

#### Capa 3: Integración FUSE (`fisopfs.c`)

**Responsabilidad:** Actúa como la **capa de integracíon** entre el FUSE y el filesystem en memoria (las estructuras Node y FileSystem). Recibe las llamadas del kernel de Linux (a través de FUSE) y las traduce a operaciones sobre las estructuras internas del filesystem. Y por último, define un **conjunto de funciones** (callbacks) que FUSE invoca cuando el usuario realiza operaciones sobre archivos/directorios.

**Funciones (Callbacks FUSE implementados):**
- `fisopfs_getattr()` --> `dir_getattr()` - Obtiene los metadatos de archivos/directorios
- `fisopfs_readdir()` --> `dir_list()` - Lista el contenido de directorios (usa `filler()`)
- `fisopfs_mkdir()` --> `dir_create()` - Crea directorios
- `fisopfs_rmdir()` --> `dir_remove()` - Elimina directorios vacíos
- `fisopfs_create()` / `fisopfs_mknod()` --> `file_create()` - Crea archivos
- `fisopfs_read()` --> `file_read()` - Lee contenido de archivos
- `fisopfs_write()` --> `file_write()` - Escribe datos en archivos
- `fisopfs_truncate()` --> `file_truncate()` - Cambia el tamaño de archivos
- `fisopfs_unlink()` --> `file_unlink()` - Elimina archivos
- `fisopfs_init()` --> `fs_load_from_disk()` - Inicialización con persistencia
- `fisopfs_destroy()` --> `fs_persist()` - Guardar estado al desmontar

**Características:**
- Única capa que conoce tipos de FUSE (`struct fuse_file_info`, `fuse_fill_dir_t`, etc.)
- Traduce parámetros de FUSE a tipos nativos de C y viceversa
- Maneja inicialización con `fs_init()` en `main()` y persistencia con `fs_persist()` al desmontar
- Retorna códigos de error POSIX (las funciones de `dir.c` y `file.c` ya retornan `-ENOENT`, `-EISDIR`, etc.)

**Ventajas de esta arquitectura:**
1. **Testabilidad:** Se pueden probar funciones como `dir_create()`, `file_write()` o `file_read()` directamente sin montar FUSE
2. **Reutilización:** Las funciones de `dir.c` y `file.c` podrían usarse en otro contexto (no solo FUSE)
3. **Mantenibilidad:** Los cambios en FUSE no afectan la lógica del filesystem (ni operaciones de archivos ni de directorios)

---

### Persistencia en Disco

El filesystem implementa un mecanismo de **persistencia** que permite guardar el estado completo en disco al desmontar y recuperarlo al volver a montar. Esto se logra mediante la serialización del árbol de nodos en un archivo binario.

#### Estructura de persistencia

**1. Estructura SuperBlock:**

  **Propósito del superbloque:**
El SuperBlock actúa como cabecera del archivo de persistencia, almacenando metadatos esenciales para la reconstrucción del filesystem. Es la primera estructura leída durante la carga y permite validar la integridad del archivo.

  **Campos iniciales:** 
El SuperBlock tiene dos campos principales: el `node_count` que representa la cantidad total de nodos almacenados en el filesystem y el`size` que indica el tamaño total en bytes ocupadas por el filesystem serializado en el disco.

  **Validaciòn durante la carga:** 
Antes de reconstruir el árbol, se verifica que el archivo sea válido y que el tamaño del payload coincida con lo esperado:
```c
// Verificacion de tamaño minimo
if (file_size < (long)sizeof(SuperBlock)) {
    return 1; 
}

// Verificacion de limites de tamaño
if (payload_size > MAX_SERIALIZED_SIZE) {
    return 1; // Payload excede la capacidad maxima
}
```

**2. Funciones de serialización:**

  Las funciones de serialización se encargan de transformar el árbol de directorios y archivos en una secuencia lineal de bytes para escribirla al disco.

- **`fs_persist(FILE *f)`**: 
  Esta función guarda el filesystem completo al desmontar. El funcionamiento consiste en crear y escribir el SuperBlock indicando tamaño total y cantidad de nodos, serializar recursivamente la raíz mediante save_node(), escribir el buffer completo al archivo y cerrar el archivo.

  Cada nodo persistido contiene nombre de archivo/directorio,tipo de nodo, cantidad de hijo, propietario y grupo del archivo, tiempo de acceso/modificacion/creacion y tamaño en bytes.

  Asimismo, la funcion `save_node()` guarda los nodos de forma contigua, respetando la jerarquía: primero el directorio, luego sus hijos en orden recursivo.

- **`fs_load_from_disk(FILE *f)`**: 
  Del modo contrario, esta función reconstruye el filesystem en memoria cuando el usuario monta el disco.
  El mecanismo consta de lectura y validación del SuperBlock, copia del resto del archivo a un buffer, reconstrucción del àrbol con `load_node()` y manejo de fallos en caso de archivo corrupto o incompleto y si falla la carga se inicializa con `fs_init()`.

**3. Funciones auxiliares:**
- **`serialize_node()` / `deserialize_node()`**: Escribe los metadatos y contenido de un nodo en un buffer e interpreta los bytes del buffer y reconstruye un nodo.
- **`save_node()` / `load_node()`**: Recorre el àrbol en DFS grabando cada nodo recursivamente y reconstruye el àrbol leyendo nodos en el mismo orden
- **`write_buf()` / `read_buf()`**: son helpers que manejan offsets de lectura/escritura

**4. Integración con FUSE:**
La persistencia se integra directamente con los callbacks de montaje y desmontaje del filesystem.
- **`fisopfs_init()`**: El flujo de ejecución de esta función es el siguiente:
  - Si el archivo existe → se llama a `fs_load_from_disk()`
  - Si no existe → se inicializa un filesystem vacío mediante `fs_init()`.
  
- **`fisopfs_destroy()`**: De forma similar, el flujo de esta función es:
  - Apertura(o creación) del archivo de persistencia
  - Llamada a `fs_persist()`para guardar el estado actual
  - Cierre del archivo

**Código de referencia (para documentar):**

```c
// Inicialización con persistencia
static void *fisopfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
    FILE *f = fopen(filedisk_path, "rb");
    if (f) {
        fs_load_from_disk(f);
        fclose(f);
    } else {
        fs_init();
    }
    return NULL;
}

// Guardado al desmontar
static void fisopfs_destroy(void *private_data) {
    FILE *f = fopen(filedisk_path, "wb");
    if (!f) return;
    fs_persist(f);
    fclose(f);
}
```

**Consideraciones importantes:**
- El archivo de persistencia se llama `persistence_file.fisopfs` por defecto
- La serialización preserva la estructura completa del árbol (jerarquía padre-hijo)
- Los metadatos (uid, gid, timestamps, permisos) se guardan para cada nodo
- El contenido de los archivos (buffer `data[]`) también se persiste
- El tamaño máximo está acotado para evitar corrupción del archivo.
---

## Pruebas y Validación

### Tests Automatizados

Se implementó un **framework de testing** basado en scripts bash que ejecuta pruebas de caja negra sobre el filesystem montado. Los tests se encuentran en el directorio `tests/` y se ejecutan con el comando `make test`.

#### Estructura del framework
El framework consta de dos componentes principales:

**1. Script maestro:** `tests/run_tests.sh`

- Monta el filesystem con FUSE
- Descubre automáticamente todos los tests (`test_*.sh`)
- Ejecuta cada test y captura su exit code
- Genera un reporte con colores (verde = pasó, rojo = falló)
- Retorna exit code 0 si todos pasan, 1 si alguno falla

**2. Tests individuales:** 9 tests implementados

- `test_mkdir.sh` - Creación de directorios
- `test_readdir.sh` - Listado de contenido (con `.` y `..`)
- `test_rmdir.sh` - Eliminación de directorios vacíos
- `test_filecreate.sh` - Creación de archivos
- `test_filerw.sh` - Lectura y escritura con offsets
- `test_stat.sh` - Validación de metadatos (UID/GID, permisos y timestamps)
- `test_unlink.sh` - Eliminación de archivos
- `test_persist-root.sh` - Montaje y desmontaje básico
- `test_persist-simple.sh` - Persistencia de archivos entre sesiones

Cada test:
- Monta el filesystem en un punto de montaje temporal (`/tmp/fisopfs_test_*`)
- Ejecuta comandos del shell (mkdir, ls, rmdir, cat, stat, etc.)
- Verifica condiciones con `[ -d ... ]`, `grep`, `stat`, etc.
- Retorna `exit 0` si pasó, `exit 1` si falló
- **Cleanup automático:** Usa `trap` para desmontar y limpiar al salir

---

#### Tests implementados

**Test 1: `test_mkdir.sh`**

**Objetivo:** Verifica que `mkdir` crea directorios correctamente.

**Pasos:**
1. Monta el filesystem en `/tmp/fisopfs_test_mkdir`
2. Ejecuta: `mkdir /tmp/fisopfs_test_readdir/test_directory`
3. Verifica: `[ -d /tmp/fisopfs_test_readdir/test_directory ]`
4. Desmonta y limpia

**Salida esperada:** Exit code 0 (directorio creado exitosamente)

---

**Test 2: `test_readdir.sh`**

**Objetivo:** Verifica que el comando `ls` lista correctamente el contenido de directorios, incluyendo sus subdirectorios.

**Pasos:**
1. Monta el filesystem
2. Crea estructura:
```
/
|-- dir1/
│     L--subdir/
L-- dir2/
```
3. Ejecuta: `ls /tmp/fisopfs_test_readdir` y captura output
4. Verifica con `grep` que aparecen `dir1` y `dir2`
5. Ejecuta: `ls /tmp/fisopfs_test_readdir/dir1` y captura output
6. Verifica con `grep` que aparece `subdir`

**Salida esperada:** Exit code 0 todos los directorios listados correctamente)

---

**Test 3: `test_rmdir.sh`**

**Objetivo:** Verifica que `rmdir` elimina directorios vacíos.

**Pasos:**
1. Monta el filesystem
2. Crea estructura:
```
/
|-- empty_dir/ (vacío)
L-- no_empty_dir/
        L-- subdir/
```
3. **Sub-test 1:** Borra `empty_dir` (debe funcionar)
   - Ejecuta: `rmdir /tmp/fisopfs_test_readdir/empty_dir`
   - Verifica: `[ ! -d empty_dir ]` (ya no existe)
4. **Sub-test 2:** Intenta borrar `no_empty_dir` (debe fallar)
   - Ejecuta: `rmdir /tmp/fisopfs_test_readdir/no_empty_dir`
   - Verifica: `[ -d no_empty_dir ]` (todavía existe)
5. **Sub-test 3:** Verifica que `subdir` sigue existiendo
   - Verifica: `[ -d no_empty_dir/subdir ]`
6. **Sub-test 4:** Borra recursivamente (hijo primero, luego padre)
   - Ejecuta: `rmdir /tmp/fisopfs_test_readdir/no_empty_dir/subdir`
   - Ejecuta: `rmdir /tmp/fisopfs_test_readdir/no_empty_dir`
   - Verifica: `[ ! -d no_empty_dir ]` (padre borrado exitosamente)

**Salida esperada:** Exit code 0 (todas las validaciones pasaron)

---
**Test 4: `test_filecreate.sh`**

**Objetivo:** Verifica que `touch` crea archivos correctamente en el filesystem.

**Pasos:**
1. Monta el filesystem en `/tmp/fisopfs_test`
2. Ejecuta: `touch /tmp/fisopfs_test/a.txt`
3. Verifica: `[ -f /tmp/fisopfs_test/a.txt ]` (el archivo debe existir)
4. Desmonta y limpia

**Salida esperada:** Exit code 0 (archivo creado exitosamente)

---

**Test 5: `test_filerw.sh`**

**Objetivo:** Verifica lectura y escritura básica sobre archivos existentes.

**Pasos:**
1. Monta el filesystem
2. Crea archivo:
   - Ejecuta: `echo "hola mundo" > /tmp/fisopfs_test/a.txt`
3. Lee contenido:
   - Ejecuta: `cat /tmp/fisopfs_test/a.txt`
   - Verifica con `grep` que aparece `hola mundo`
4. Sobrescribe contenido:
   - Ejecuta: `echo "segundo texto" > /tmp/fisopfs_test/a.txt`
5. Verifica lectura nuevamente:
   - Ejecuta: `cat /tmp/fisopfs_test/a.txt`
   - Verifica con `grep` que aparece `segundo texto`
6. Desmonta y limpia

**Salida esperada:** Exit code 0 (lectura y escritura funcionando correctamente)

---

**Test 6: `test_unlink.sh`**

**Objetivo:** Verifica que `rm` elimina archivos correctamente.

**Pasos:**
1. Monta el filesystem
2. Crea archivo:
   - Ejecuta: `touch /tmp/fisopfs_test/a.txt`
   - Verifica: `[ -f /tmp/fisopfs_test/a.txt ]`
3. Borra archivo:
   - Ejecuta: `rm /tmp/fisopfs_test/a.txt`
4. Verifica eliminación:
   - Ejecuta: `[ ! -f /tmp/fisopfs_test/a.txt ]`
5. Desmonta y limpia

**Salida esperada:** Exit code 0 (archivo eliminado exitosamente)

---

**Test 7: `test_stat.sh`**

**Objetivo:** Valida las estadísticas y metadatos implementados en `fisopfs_getattr()`, verificando que UID, GID, permisos, timestamps (atime, mtime, ctime) y tamaños se manejan correctamente según la consigna.

**Pasos:**

**Sub-test 1: UID/GID del usuario actual**

1. Obtiene el UID/GID del usuario que ejecuta el test (`id -u`, `id -g`)
2. Crea un archivo en el filesystem montado: `touch stat_test.txt`
3. Obtiene las estadísticas con `stat -c "%u %g" stat_test.txt`
4. Verifica que UID/GID coinciden con los del usuario (implementado con `getuid()`/`getgid()`)

**Validaciones:**
- UID correcto (debe coincidir con `getuid()`)
- GID correcto (debe coincidir con `getgid()`)

**Sub-test 2: Permisos por defecto**

1. Crea un archivo: `touch perms_file.txt`
2. Crea un directorio: `mkdir perms_dir`
3. Obtiene permisos con `stat -c "%a" <path>`
4. Verifica que archivos tienen `0644` y directorios `0755`

**Validaciones:**
- Permisos de archivo correctos (`0644`)
- Permisos de directorio correctos (`0755`)

**Sub-test 3: Actualización de atime (access time)**

1. Crea archivo con contenido: `echo "test" > atime_test.txt`
2. Espera 2 segundos: `sleep 2`
3. Captura atime inicial: `stat -c "%X" atime_test.txt`
4. Lee el archivo: `cat atime_test.txt > /dev/null`
5. Captura atime final: `stat -c "%X" atime_test.txt`
6. Verifica que atime aumentó (se actualiza en `file_read()`)

**Validaciones:**
- atime se actualiza correctamente al leer (`time(NULL)` en `file_read()`)

**Sub-test 4: Actualización de mtime/ctime (modification/change time)**

1. Crea archivo: `echo "initial" > mtime_test.txt`
2. Espera 2 segundos: `sleep 2`
3. Captura mtime/ctime iniciales: `stat -c "%Y %Z" mtime_test.txt`
4. Modifica archivo: `echo "modified" > mtime_test.txt`
5. Captura mtime/ctime finales: `stat -c "%Y %Z" mtime_test.txt`
6. Verifica que ambos timestamps aumentaron (se actualizan en `file_write()`)

**Validaciones:**
- mtime se actualiza correctamente al escribir (`time(NULL)` en `file_write()`)
- ctime se actualiza correctamente al modificar (`time(NULL)` en `file_write()`)

**Sub-test 5: Tamaño de archivo correcto**

1. Crea archivo con contenido de 10 bytes: `echo -n "0123456789" > size_test.txt`
2. Crea directorio: `mkdir size_dir`
3. Obtiene tamaños con `stat -c "%s" <path>`
4. Verifica que el archivo reporta 10 bytes y el directorio 0 bytes

**Validaciones:**
- Tamaño de archivo correcto (10 bytes)
- Tamaño de directorio correcto (0 bytes)

**Salida esperada:** Exit code 0 (todas las 9 validaciones pasaron)

---

**Test 8: `test_persist-root.sh`**

**Propósito:** Verifica que el filesystem puede montarse y desmontarse correctamente, validando la accesibilidad del nodo raíz.

**Funcionamiento:**

1. Prepara el punto de montaje: `mkdir -p /tmp/fisopfs_test_mount`
2. Monta el filesystem en background: `./fisopfs -f /tmp/fisopfs_test_mount &`
3. Espera a que FUSE esté listo: `sleep 1`
4. Verifica que el directorio raíz esté accesible: `[ -d /tmp/fisopfs_test_mount ]`
5. Desmonta con `fusermount -u` y limpia recursos

**Validaciones:**
- El punto de montaje es un directorio válido
- El filesystem responde correctamente a operaciones básicas de acceso
- El proceso FUSE se ejecuta sin errores fatales

**Comandos clave:**
```bash
./fisopfs -f "$MOUNT_POINT" &    # Monta en foreground (para testing)
[ -d "$MOUNT_POINT" ]            # Verifica directorio accesible
fusermount -u "$MOUNT_POINT"     # Desmonta limpiamente
```

**Salida esperada:** Exit code 0 con mensaje "Test PASÓ: FS montado correctamente"

---

**Test 9: `test_persist-simple.sh`**

**Propósito:** Verifica la **persistencia en disco** del filesystem. Confirma que los archivos creados sobreviven al desmontaje y pueden recuperarse en un nuevo montaje.

**Funcionamiento:**

1. Prepara el punto de montaje: `mkdir -p /tmp/fisopfs_test_mount`
2. Crea archivo de prueba temporal: `echo "Hola FUSE" > /tmp/hello.txt`
3. **Primer montaje** con archivo de persistencia específico:
   ```bash
   ./fisopfs /tmp/fisopfs_test_mount --filedisk test.fisopfs &
   ```
4. Copia archivo dentro del FS montado: `cp /tmp/hello.txt $MOUNT_POINT/hello.txt`
5. **Desmonta para forzar persistencia**: `fusermount -u /tmp/fisopfs_test_mount`
   - Esto dispara `fisopfs_destroy()` que llama a `fs_persist()` para guardar en `test.fisopfs`
6. **Segundo montaje** con el mismo archivo de disco: `./fisopfs /tmp/fisopfs_test_mount --filedisk test.fisopfs &`
   - Esto dispara `fisopfs_init()` que llama a `fs_load_from_disk()` para restaurar desde `test.fisopfs`
7. Verifica que el archivo persiste: `[ -f $MOUNT_POINT/hello.txt ]`

**Validaciones:**
- Archivo se crea correctamente en el primer montaje
- `fs_persist()` serializa el filesystem al desmontar
- `fs_load_from_disk()` reconstruye el filesystem en el segundo montaje
- El archivo `hello.txt` existe y es accesible después de remontar
- El flag `loading_from_disk` funciona correctamente para prevenir duplicación de nodos

**Comandos clave:**
```bash
./fisopfs MOUNT --filedisk FILE  # Monta con persistencia en FILE
fusermount -u MOUNT               # Desmonta y guarda estado
[ -f "$MOUNT/file" ]              # Verifica archivo después de remontar
```

**Mecanismo de persistencia:**
- Al **desmontar**: `fisopfs_destroy()` → `fs_persist(FILE)` → serializa árbol a binario
- Al **montar**: `fisopfs_init()` → `fs_load_from_disk(FILE)` → deserializa y reconstruye árbol
- **Formato**: SuperBlock (metadata) + árbol serializado recursivamente (DFS)

**Salida esperada:** Exit code 0 con mensaje "Test PASÓ: archivo persistió"

---

#### Resultados de ejecución

Al ejecutar `make test`, se obtiene la siguiente salida con **9 tests pasando exitosamente**:

```bash
$ make test
