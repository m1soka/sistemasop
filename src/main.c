#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "dag.h"

int main(int argc, char **argv) {
    // Se exige invocar con archivo y límite K de concurrencia
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <archivo_plan.txt> <K_concurrencia>\n", argv[0]);
        return 1;
    }

    srand((unsigned)time(NULL));

    // Convertir el argumento K a entero
    int limite_k = atoi(argv[2]);
    if (limite_k <= 0) {
        fprintf(stderr, "Error: K debe ser un número entero mayor a 0.\n");
        return 1;
    }

    int cantidad_nodos = 0;
    
    // cargar_plan reemplaza a dag_load
    node_t *grafo = cargar_plan(argv[1], &cantidad_nodos);
    if (grafo == NULL) {
        fprintf(stderr, "Error cargando %s\n", argv[1]);
        return 1;
    }

    // Iterar e imprimir usando los nuevos nombres de las variables
    for (int i = 0; i < cantidad_nodos; i++) {
        printf("%s (%s) %dms deps_iniciales=%d\n", 
               grafo[i].id, 
               grafo[i].name,
               grafo[i].duration_ms, 
               grafo[i].unresolved_dependencies);
    }

    // Llamada a la ejecución principal (descomentar cuando la implementes)
    // iniciar_planificador(grafo, cantidad_nodos, limite_k);

    // limpiar_memoria reemplaza a dag_free
    limpiar_memoria(grafo, cantidad_nodos);
    
    return 0;
}
