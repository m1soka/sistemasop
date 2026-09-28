#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include "dag.h"

node_t* cargar_plan(const char *ruta_archivo, int *cantidad_nodos);
void limpiar_memoria(node_t *grafo, int total_nodos);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <archivo.txt> <K>\n", argv[0]);
        return 1;
    }

    int K = atoi(argv[2]);
    int cantidad_nodos = 0;
    
    node_t *grafo = cargar_plan(argv[1], &cantidad_nodos);
    if (!grafo) {
        printf("Error cargando %s\n", argv[1]);
        return 1;
    }

    int tareas_terminadas = 0;
    int procesos_activos = 0;
    
    // --- 1. CREACIÓN DEL PIPE (TUBERÍA) ---
    int fd[2];
    if (pipe(fd) == -1) {
        perror("Error al crear el pipe");
        return 1;
    }

    printf("--- INICIANDO PLANIFICADOR (Límite K = %d) ---\n", K);

    while (tareas_terminadas < cantidad_nodos) {
        int lanzo_tarea = 0;

        for (int i = 0; i < cantidad_nodos && procesos_activos < K; i++) {
            if (grafo[i].unresolved_dependencies == 0 && grafo[i].process_id == 0) {
                
                pid_t pid = fork();

                if (pid == 0) {
                    // --- CÓDIGO DEL HIJO ---
                    close(fd[0]); // El hijo solo escribe, cerramos el lado de lectura
                    
                    printf("[HIJO] Tarea '%s' iniciada (Duracion: %d ms)\n", 
                           grafo[i].name, grafo[i].duration_ms);
                    
                    usleep(grafo[i].duration_ms * 1000);
                    
                    // 2. EL HIJO ESCRIBE EN EL PIPE ANTES DE MORIR
                    char mensaje[100] = {0}; // Llenamos de ceros para tamaño fijo
                    snprintf(mensaje, sizeof(mensaje), "La tarea '%s' termino exitosamente.", grafo[i].name);
                    write(fd[1], mensaje, sizeof(mensaje));
                    
                    close(fd[1]); // Cerramos escritura y terminamos el proceso
                    exit(0);
                    
                } else if (pid > 0) {
                    grafo[i].process_id = pid;
                    procesos_activos++;
                    lanzo_tarea = 1;
                }
            }
        }

        if (procesos_activos == K || (!lanzo_tarea && procesos_activos > 0)) {
            int status;
            pid_t pid_terminado = wait(&status);
            
            // --- 3. EL PADRE LEE EL MENSAJE DEL PIPE ---
            char buffer[100];
            read(fd[0], buffer, sizeof(buffer));
            printf("[PADRE] Mensaje recibido por IPC: %s\n", buffer);
            
            procesos_activos--;
            tareas_terminadas++;

            for (int i = 0; i < cantidad_nodos; i++) {
                if (grafo[i].process_id == pid_terminado) {
                    grafo[i].process_id = -1;
                    
                    dep_t *actual = grafo[i].next_tasks;
                    while (actual != NULL) {
                        actual->task->unresolved_dependencies--;
                        actual = actual->next;
                    }
                    break;
                }
            }
        }
    }

    printf("--- TODAS LAS TAREAS FINALIZADAS ---\n");
    
    // Cerramos los descriptores en el padre al terminar todo
    close(fd[0]);
    close(fd[1]);
    limpiar_memoria(grafo, cantidad_nodos);
    return 0;
}