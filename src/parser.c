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
    rewind(archivo);

    // 2. Crear el arreglo dinámico para los nodos
    node_t *grafo = calloc(lineas, sizeof(node_t));
    *cantidad_nodos = lineas;

    // 3. Extraer los datos base (ID, nombre, tiempo)
    int i = 0;
    while (fgets(buffer, sizeof(buffer), archivo)) {
        if (strlen(buffer) < 3) continue; 

        // Encontrar los delimitadores de forma segura
        char *p1 = strchr(buffer, ':');
        char *p2 = p1 ? strchr(p1 + 1, ':') : NULL;
        char *p3 = p2 ? strchr(p2 + 1, ':') : NULL;

        if (!p1 || !p2 || !p3) continue;

        *p1 = '\0';
        *p2 = '\0';
        *p3 = '\0';

        char *id_str = buffer;
        char *nombre_str = p1 + 1;
        char *tiempo_str = p2 + 1;

        sscanf(id_str, " %15s", grafo[i].id);
        sscanf(nombre_str, " %63s", grafo[i].name);

        int tiempo = 0;
        if (sscanf(tiempo_str, " %d", &tiempo) == 1) {
            grafo[i].duration_ms = tiempo;
        } else {
            // Tiempo aleatorio si el campo está vacío
            grafo[i].duration_ms = 100 + rand() % 4901; 
        }

        grafo[i].unresolved_dependencies = 0;
        grafo[i].next_tasks = NULL;
        grafo[i].process_id = 0;

        i++;
    }

    // 4. Segunda pasada: procesar las dependencias (el DAG)
    rewind(archivo);
    i = 0;
    while (fgets(buffer, sizeof(buffer), archivo)) {
        if (strlen(buffer) < 3) continue;

        char *p1 = strchr(buffer, ':');
        char *p2 = p1 ? strchr(p1 + 1, ':') : NULL;
        char *p3 = p2 ? strchr(p2 + 1, ':') : NULL;
        if (!p1 || !p2 || !p3) continue;

        char *deps_str = p3 + 1;
        
        char *dep_id = strtok(deps_str, " ,\n");
        while (dep_id != NULL) {
            for (int j = 0; j < *cantidad_nodos; j++) {
                if (strcmp(grafo[j].id, dep_id) == 0) {
                    // Sumamos una dependencia a la tarea actual
                    grafo[i].unresolved_dependencies++;
                    
                    // Agregamos la tarea actual a la lista "next_tasks" de su predecesora
                    dep_t *nueva_dep = malloc(sizeof(dep_t));
                    nueva_dep->task = &grafo[i];
                    nueva_dep->next = grafo[j].next_tasks;
                    grafo[j].next_tasks = nueva_dep;
                    break;
                }
            }
            dep_id = strtok(NULL, " ,\n");
        }
        i++;
    }
    
    fclose(archivo);
    return grafo;
}

void limpiar_memoria(node_t *grafo, int total_nodos) {
    if (grafo != NULL) {
        for (int i = 0; i < total_nodos; i++) {
            // Liberar la memoria de las listas enlazadas creadas con malloc
            dep_t *actual = grafo[i].next_tasks;
            while (actual != NULL) {
                dep_t *siguiente = actual->next;
                free(actual);
                actual = siguiente;
            }
        }
        free(grafo);
    }
}
