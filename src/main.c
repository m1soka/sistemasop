#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "dag.h"

#define TAM_MSG   100    /* mensaje acotado de una tarea al terminar */
#define TAM_INBOX 512    /* insumos acumulados para una tarea dependiente */

static volatile sig_atomic_t seremi_detectada = 0;
static int prob_fallo = 0;   /* % de fallo interno; se cambia con PROB_FALLO=N */

static void manejador_seremi(int sig) {
    (void)sig;
    seremi_detectada = 1;
    const char msg[] = "\n[!] SEREMI DETECTADA (Ctrl+C) - Abortando todas las actividades...\n";
    ssize_t r = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)r;
}

static void dormir_ms(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { }
}

/*
 * Planificador. Dos pipes por tarea:
 *   p: hijo -> padre  (mensaje de termino)
 *   q: padre -> hijo  (insumos de sus dependencias, escritos antes del fork)
 * Sin busy-waiting: el padre solo espera en wait() (bloqueante).
 */
void iniciar_planificador(node_t *g, int n, int K) {
    int *pipe_rd = malloc(n * sizeof(int));
    char (*inbox)[TAM_INBOX] = calloc(n, TAM_INBOX);
    if (!pipe_rd || !inbox) {
        perror("malloc");
        free(pipe_rd); free(inbox);
        return;
    }
    int terminadas = 0, activos = 0;

    while (terminadas < n && !seremi_detectada) {
        int lanzo = 0;

        for (int i = 0; i < n && activos < K; i++) {
            if (g[i].unresolved_dependencies != 0 || g[i].process_id != 0) continue;

            int p[2], q[2];
            if (pipe(p) == -1) { perror("pipe"); break; }
            if (pipe(q) == -1) { perror("pipe"); close(p[0]); close(p[1]); break; }

            /* el padre deja los insumos en el pipe (cabe de sobra en el buffer) */
            size_t len = strlen(inbox[i]);
            if (len > 0) {
                ssize_t w = write(q[1], inbox[i], len);
                (void)w;
            }
            close(q[1]);

            fflush(stdout);                  /* evita salida duplicada en el hijo */
            pid_t pid = fork();

            if (pid == 0) {
                close(p[0]);
                for (int j = 0; j < n; j++)  /* no heredar extremos de otros hijos */
                    if (g[j].process_id > 0) close(pipe_rd[j]);
                signal(SIGINT, SIG_DFL);
                srand(getpid() ^ (unsigned)time(NULL));

                char entrada[TAM_INBOX] = {0};
                ssize_t r = read(q[0], entrada, sizeof(entrada) - 1);
                close(q[0]);

                printf("[HIJO] Tarea '%s' iniciada (Duracion: %d ms)\n",
                       g[i].name, g[i].duration_ms);
                if (r > 0)
                    printf("[HIJO] Tarea '%s' recibio insumos: %s\n", g[i].name, entrada);
                fflush(stdout);

                dormir_ms(g[i].duration_ms);

                if (rand() % 100 < prob_fallo) {
                    printf("[HIJO] Tarea '%s' FALLO INTERNAMENTE.\n", g[i].name);
                    fflush(stdout);
                    close(p[1]);
                    _exit(1);
                }
                char msg[TAM_MSG] = {0};
                snprintf(msg, sizeof(msg), "La tarea '%s' termino exitosamente.", g[i].name);
                ssize_t w = write(p[1], msg, sizeof(msg));
                (void)w;
                close(p[1]);
                _exit(0);
            } else if (pid > 0) {
                close(p[1]);
                close(q[0]);
                pipe_rd[i] = p[0];
                g[i].process_id = pid;
                activos++;
                lanzo = 1;
            } else {
                perror("fork");
                close(p[0]); close(p[1]); close(q[0]);
                break;
            }
        }

        if (!lanzo && activos == 0) {
            printf("[!] Flujo detenido. Las tareas restantes fueron abortadas por fallo en su dependencia.\n");
            break;
        }

        if (activos == K || (!lanzo && activos > 0)) {
            int status;
            pid_t fin = wait(&status);
            if (fin == -1) { if (errno == EINTR) continue; break; }
            activos--; terminadas++;

            for (int i = 0; i < n; i++) {
                if (g[i].process_id != fin) continue;
                g[i].process_id = -1;

                if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                    char buf[TAM_MSG] = {0};
                    if (read(pipe_rd[i], buf, sizeof(buf) - 1) > 0)
                        printf("[PADRE] Mensaje recibido por IPC: %s\n", buf);
                    for (dep_t *d = g[i].next_tasks; d; d = d->next) {
                        int idx = (int)(d->task - g);
                        printf("[PADRE] Propagando insumo de '%s' a '%s'\n",
                               g[i].name, d->task->name);
                        size_t usado = strlen(inbox[idx]);
                        if (usado + 1 < TAM_INBOX)
                            snprintf(inbox[idx] + usado, TAM_INBOX - usado, "%s%s",
                                     usado ? " | " : "", buf);
                        if (d->task->unresolved_dependencies > 0)
                            d->task->unresolved_dependencies--;
                    }
                } else {
                    printf("[PADRE] La tarea '%s' reporto un error. Abortando rama dependiente.\n",
                           g[i].name);
                    for (dep_t *d = g[i].next_tasks; d; d = d->next)
                        d->task->unresolved_dependencies = -1;
                }
                close(pipe_rd[i]);
                break;
            }
        }
    }

    if (seremi_detectada) {
        for (int i = 0; i < n; i++)
            if (g[i].process_id > 0) {
                kill(g[i].process_id, SIGKILL);
                close(pipe_rd[i]);
            }
        while (wait(NULL) > 0) { }
    } else {
        int no_ejecutadas = 0;
        for (int i = 0; i < n; i++) if (g[i].process_id == 0) no_ejecutadas++;
        if (no_ejecutadas) printf("[!] %d tareas no se ejecutaron.\n", no_ejecutadas);
        printf("--- SIMULACION FINALIZADA ---\n");
    }
    free(pipe_rd);
    free(inbox);
}

int main(int argc, char *argv[]) {
    if (argc != 3) { printf("Uso: %s <archivo.txt> <K>\n", argv[0]); return 1; }
    int K = atoi(argv[2]);
    if (K < 1) { printf("K debe ser >= 1\n"); return 1; }

    const char *pf = getenv("PROB_FALLO");
    if (pf) prob_fallo = atoi(pf);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = manejador_seremi;     /* sin SA_RESTART: wait() sale con EINTR */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    srand((unsigned)time(NULL));
    int n = 0;
    node_t *g = cargar_plan(argv[1], &n);
    if (!g) { printf("Error cargando %s\n", argv[1]); return 1; }

    printf("--- INICIANDO PLANIFICADOR (Límite K = %d) ---\n", K);
    iniciar_planificador(g, n, K);
    limpiar_memoria(g, n);
    return 0;
}