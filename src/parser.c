#include "dag.h"
#include <stdlib.h>

node_t* cargar_plan(const char *ruta_archivo, int *cantidad_nodos) {
    // Silenciar warning de variable sin usar mientras armas el parseo
    (void)ruta_archivo; 

    // Aquí deberás implementar fopen() para leer el archivo de texto
    // y asignar tiempos aleatorios (100 a 5000 ms) si falta la duración.
    
    *cantidad_nodos = 0;
    return NULL; // Retornarás el arreglo dinámico (malloc) cuando lo implementes
}

void limpiar_memoria(node_t *grafo, int total_nodos) {
    // Silenciar warning de variable sin usar por ahora
    (void)total_nodos; 

    if (grafo != NULL) {
        // Aquí deberás iterar sobre cada nodo (de 0 a total_nodos)
        // y liberar (free) la lista enlazada 'next_tasks' de cada uno
        // antes de liberar el arreglo principal.
        free(grafo);
    }
}
