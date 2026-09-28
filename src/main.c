#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "dag.h"

// Prototipos de las funciones que programaste en parser.c
node_t* cargar_plan(const char *ruta_archivo, int *cantidad_nodos);
void limpiar_memoria(node_t *grafo, int total_nodos);

int main(int argc, char *argv[]) {
    // Validar argumentos
    if (argc != 3) {
        printf("Uso: %s <archivo.txt> <K>\n", argv[0]);
        return 1;
    }

    int K = atoi(argv[2]);
    int cantidad_nodos = 0;
    
    // 1. Cargar el DAG en memoria
    node_t *grafo = cargar_plan(argv[1], &cantidad_nodos);
    if (!grafo) {
        printf("Error cargando %s\n", argv[1]);
        return 1;
    }

    int tareas_terminadas = 0;
    int procesos_activos = 0;

    printf("--- INICIANDO PLANIFICADOR (Límite K = %d) ---\n", K);

    // Bucle principal: se ejecuta hasta que todas las actividades terminen
    while (tareas_terminadas < cantidad_nodos) {
        int lanzo_tarea = 0;

        // 2. Buscar tareas listas para ejecutarse
        for (int i = 0; i < cantidad_nodos && procesos_activos < K; i++) {
            // Una tarea está lista si no debe esperar a nadie y aún no ha sido iniciada
            if (grafo[i].unresolved_dependencies == 0 && grafo[i].process_id == 0) {
                
                pid_t pid = fork(); // ¡Nace un nuevo proceso!

                if (pid == 0) {
                    // --- CÓDIGO DEL PROCESO HIJO ---
                    printf("[HIJO] Tarea '%s' iniciada (Duracion: %d ms)\n", 
                           grafo[i].name, grafo[i].duration_ms);
                    
                    // Simulamos el trabajo durmiendo el proceso los milisegundos indicados
                    usleep(grafo[i].duration_ms * 1000);
                    
                    printf("[HIJO] Tarea '%s' finalizada.\n", grafo[i].name);
                    exit(0); // El hijo muere aquí, no continúa el bucle
                    
                } else if (pid > 0) {
                    // --- CÓDIGO DEL PROCESO PADRE ---
                    grafo[i].process_id = pid; // Guardamos el PID para saber quién es
                    procesos_activos++;
                    lanzo_tarea = 1;
                }
            }
        }

        // 3. Control de Concurrencia (El límite K)
        // Si topamos el límite K, o si no pudimos lanzar nada nuevo pero hay procesos corriendo, 
        // el padre DEBE esperar a que alguien termine.
        if (procesos_activos == K || (!lanzo_tarea && procesos_activos > 0)) {
            int status;
            pid_t pid_terminado = wait(&status); // El padre se duerme hasta que un hijo haga exit(0)
            
            procesos_activos--;
            tareas_terminadas++;

            // 4. Buscar qué tarea acaba de terminar por su PID
            for (int i = 0; i < cantidad_nodos; i++) {
                if (grafo[i].process_id == pid_terminado) {
                    grafo[i].process_id = -1; // La marcamos como finalizada
                    
                    // 5. Notificar a las tareas dependientes
                    dep_t *actual = grafo[i].next_tasks;
                    while (actual != NULL) {
                        actual->task->unresolved_dependencies--; // Le restamos 1 candado
                        actual = actual->next;
                    }
                    break;
                }
            }
        }
    }

    printf("--- TODAS LAS TAREAS FINALIZADAS ---\n");
    limpiar_memoria(grafo, cantidad_nodos);
    return 0;
}
