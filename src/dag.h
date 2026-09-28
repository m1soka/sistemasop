#ifndef DAG_H
#define DAG_H

#include <sys/types.h>

typedef struct Dependency {
    struct Node *task;
    struct Dependency *next;
} dep_t;

typedef struct Node {
    char id[16];
    char name[64];
    int duration_ms;
    int unresolved_dependencies;
    dep_t *next_tasks;           
    pid_t process_id;
} node_t;

// Declaraciones de las funciones principales a implementar en main.c / utils.c
node_t* cargar_plan(const char *ruta_archivo, int *cantidad_nodos);
void iniciar_planificador(node_t *grafo, int total_nodos, int limite_k);
void limpiar_memoria(node_t *grafo, int total_nodos);

#endif
