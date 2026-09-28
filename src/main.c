#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include "dag.h"

// Variables globales para el manejador de señales
volatile sig_atomic_t seremi_detectada = 0;
node_t *grafo_global = NULL;
int nodos_global = 0;

void manejador_seremi(int sig) {
    (void)sig; // Evitar warning de variable sin uso
    seremi_detectada = 1;
    printf("\n[!] SEREMI DETECTADA (Ctrl+C) - Abortando todas las actividades...\n");
}

node_t* cargar_plan(const char *ruta_archivo, int *cantidad_nodos);
void limpiar_memoria(node_t *grafo, int total_nodos);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <archivo.txt> <K>\n", argv[0]);
        return 1;
    }

    int K = atoi(argv[2]);
    
    // Configurar el manejador de la señal SIGINT (Ctrl+C)
    struct sigaction sa;
    sa.sa_handler = manejador_seremi;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    grafo_global = cargar_plan(argv[1], &nodos_global);
    if (!grafo_global) {
        printf("Error cargando %s\n", argv[1]);
        return 1;
    }

    int tareas_terminadas = 0;
    int procesos_activos = 0;
    
    int fd[2];
    if (pipe(fd) == -1) {
        perror("Error al crear el pipe");
        return 1;
    }

    printf("--- INICIANDO PLANIFICADOR (Límite K = %d) ---\n", K);

    while (tareas_terminadas < nodos_global && !seremi_detectada) {
        int lanzo_tarea = 0;

        for (int i = 0; i < nodos_global && procesos_activos < K; i++) {
            // unresolved_dependencies == 0 significa lista para ejecutar
            if (grafo_global[i].unresolved_dependencies == 0 && grafo_global[i].process_id == 0) {
                
                pid_t pid = fork();

                if (pid == 0) {
                    close(fd[0]); 
                    
                    printf("[HIJO] Tarea '%s' iniciada (Duracion: %d ms)\n", 
                           grafo_global[i].name, grafo_global[i].duration_ms);
                    
                    usleep(grafo_global[i].duration_ms * 1000);
                    
                    // Simulador de fallo: 10% de probabilidad de fallar para probar el requisito 4.1
                    // En un caso real, la tarea fallaría por un error de ejecución.
                    if (rand() % 100 < 10) {
                        printf("[HIJO] Tarea '%s' FALLO INTERNAMENTE.\n", grafo_global[i].name);
                        close(fd[1]);
                        exit(1); // Código de error
                    }
                    
                    char mensaje[100] = {0};
                    snprintf(mensaje, sizeof(mensaje), "La tarea '%s' termino exitosamente.", grafo_global[i].name);
                    write(fd[1], mensaje, sizeof(mensaje));
                    
                    close(fd[1]);
                    exit(0); // Éxito
                    
                } else if (pid > 0) {
                    grafo_global[i].process_id = pid;
                    procesos_activos++;
                    lanzo_tarea = 1;
                }
            }
        }

        if (procesos_activos == K || (!lanzo_tarea && procesos_activos > 0)) {
            int status;
            pid_t pid_terminado = wait(&status);
            
            // Si wait es interrumpido por Ctrl+C, salimos del ciclo
            if (pid_terminado == -1) {
                break;
            }
            
            procesos_activos--;
            tareas_terminadas++;

            for (int i = 0; i < nodos_global; i++) {
                if (grafo_global[i].process_id == pid_terminado) {
                    grafo_global[i].process_id = -1; // Marcada como finalizada
                    
                    // REQUISITO 4.1: AISLAMIENTO DE ERRORES
                    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                        printf("[PADRE] La tarea '%s' reporto un error. Abortando rama dependiente.\n", grafo_global[i].name);
                        // Marcamos las tareas dependientes con -1 para que nunca se ejecuten
                        dep_t *actual = grafo_global[i].next_tasks;
                        while (actual != NULL) {
                            actual->task->unresolved_dependencies = -1; 
                            actual = actual->next;
                        }
                    } else {
                        // Flujo normal exitoso
                        char buffer[100];
                        read(fd[0], buffer, sizeof(buffer));
                        printf("[PADRE] Mensaje recibido por IPC: %s\n", buffer);
                        
                        dep_t *actual = grafo_global[i].next_tasks;
                        while (actual != NULL) {
                            // Solo restamos dependencias si no fue cancelada (-1) previamente
                            if (actual->task->unresolved_dependencies > 0) {
                                actual->task->unresolved_dependencies--;
                            }
                            actual = actual->next;
                        }
                    }
                    break;
                }
            }
        }
        
        // Si no lanzamos tareas y no hay procesos activos, significa que las restantes fueron canceladas por un fallo previo
        if (!lanzo_tarea && procesos_activos == 0 && tareas_terminadas < nodos_global) {
            printf("[!] Flujo de tareas detenido. Las tareas restantes fueron abortadas por un fallo en su dependencia.\n");
            break;
        }
    }

    // REQUISITO 4.2: LIMPIEZA TRAS CTRL+C
    if (seremi_detectada) {
        // Matar a todos los hijos activos
        for (int i = 0; i < nodos_global; i++) {
            if (grafo_global[i].process_id > 0) {
                kill(grafo_global[i].process_id, SIGKILL);
            }
        }
        // Esperar a que los hijos mueran para evitar procesos zombie
        while (wait(NULL) > 0);
    } else {
        printf("--- SIMULACION FINALIZADA ---\n");
    }
    
    close(fd[0]);
    close(fd[1]);
    limpiar_memoria(grafo_global, nodos_global);
    return 0;
}