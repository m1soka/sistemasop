#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dag.h"

node_t* cargar_plan(const char *ruta_archivo, int *cantidad_nodos) {
    FILE *archivo = fopen(ruta_archivo, "r");
    if (!archivo) return NULL;

    // 1. Contar las líneas del archivo para saber cuánta memoria pedir
    int lineas = 0;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), archivo)) {
        if (strlen(buffer) > 2) lineas++;
    }
    rewind(archivo); // Volver al inicio del archivo

    // 2. Crear el arreglo dinámico para los nodos
    node_t *grafo = calloc(lineas, sizeof(node_t));
    *cantidad_nodos = lineas;

    // 3. Extraer los datos línea por línea
    int i = 0;
    while (fgets(buffer, sizeof(buffer), archivo)) {
        // strtok separa la línea de texto usando los dos puntos ':' como delimitador
        char *id_str = strtok(buffer, ":");
        char *nombre_str = strtok(NULL, ":");
        char *tiempo_str = strtok(NULL, ":");
        char *deps_str = strtok(NULL, "\n");

        // Limpiar espacios y guardar en la estructura
        sscanf(id_str, " %15[^ ]", grafo[i].id);
        sscanf(nombre_str, " %63[^ ]", grafo[i].name);

        // Evaluar si existe un tiempo definido
        int tiempo = 0;
        // Si sscanf logra leer un número, lo asigna. Si falla (ej. está vacío), genera el aleatorio.
        if (tiempo_str != NULL && sscanf(tiempo_str, " %d", &tiempo) == 1) {
            grafo[i].duration_ms = tiempo;
        } else {
            // Asigna aleatorio en un rango entre 100 y 5000 milisegundos
            grafo[i].duration_ms = 100 + rand() % 4901; 
        }

        grafo[i].unresolved_dependencies = 0;
        grafo[i].next_tasks = NULL;
        grafo[i].process_id = 0;

        i++;
    }
    
    fclose(archivo);
    
    // (Punto pendiente para ti: Faltaría hacer un segundo ciclo FOR aquí 
    // para procesar 'deps_str' y armar las listas enlazadas de dependencias)

    return grafo;
}

void limpiar_memoria(node_t *grafo, int total_nodos) {
    if (grafo != NULL) {
        // Aquí deberás liberar los mallocs de las listas enlazadas más adelante
        free(grafo);
    }
}
