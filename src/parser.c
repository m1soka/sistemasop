#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "dag.h"

/* ---------- utilidades ---------- */

static char *trim(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *fin = s + strlen(s);
    while (fin > s && isspace((unsigned char)fin[-1])) *--fin = '\0';
    return s;
}

/* Separa "ID : nombre : tiempo : deps" (modifica la linea).
 * Retorna 1 = valida, 0 = vacia/comentario, -1 = malformada. */
static int dividir_linea(char *linea, char **id, char **nombre,
                         char **tiempo, char **deps) {
    char *t = trim(linea);
    if (*t == '\0' || *t == '#') return 0;

    char *p1 = strchr(t, ':');
    char *p2 = p1 ? strchr(p1 + 1, ':') : NULL;
    char *p3 = p2 ? strchr(p2 + 1, ':') : NULL;
    if (!p1 || !p2 || !p3) return -1;

    *p1 = *p2 = *p3 = '\0';
    *id = trim(t);
    *nombre = trim(p1 + 1);
    *tiempo = trim(p2 + 1);
    *deps = p3 + 1;
    if ((*id)[0] == '\0') return -1;
    return 1;
}

/* ---------- tabla hash id -> indice (direccionamiento abierto) ---------- */

typedef struct { const char *id; int idx; } entrada_t;

typedef struct {
    entrada_t *e;
    size_t cap;              /* potencia de 2 */
} tabla_t;

static size_t hash_str(const char *s) {
    size_t h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

static int tabla_init(tabla_t *t, int n) {
    size_t cap = 16;
    while (cap < (size_t)n * 2) cap <<= 1;
    t->cap = cap;
    t->e = calloc(cap, sizeof(entrada_t));
    return t->e != NULL;
}

/* Retorna 1 si inserto, 0 si el id ya existia. */
static int tabla_insertar(tabla_t *t, const char *id, int idx) {
    size_t i = hash_str(id) & (t->cap - 1);
    while (t->e[i].id) {
        if (strcmp(t->e[i].id, id) == 0) return 0;
        i = (i + 1) & (t->cap - 1);
    }
    t->e[i].id = id;
    t->e[i].idx = idx;
    return 1;
}

static int tabla_buscar(const tabla_t *t, const char *id) {
    size_t i = hash_str(id) & (t->cap - 1);
    while (t->e[i].id) {
        if (strcmp(t->e[i].id, id) == 0) return t->e[i].idx;
        i = (i + 1) & (t->cap - 1);
    }
    return -1;
}

/* ---------- deteccion de ciclos (Kahn), solo avisa ---------- */

static void avisar_ciclos(node_t *g, int n) {
    int *grado = malloc(n * sizeof(int));
    int *cola = malloc(n * sizeof(int));
    if (!grado || !cola) { free(grado); free(cola); return; }

    int ini = 0, fin = 0, visitados = 0;
    for (int i = 0; i < n; i++) {
        grado[i] = g[i].unresolved_dependencies;
        if (grado[i] == 0) cola[fin++] = i;
    }
    while (ini < fin) {
        int u = cola[ini++];
        visitados++;
        for (dep_t *d = g[u].next_tasks; d; d = d->next) {
            int v = (int)(d->task - g);
            if (--grado[v] == 0) cola[fin++] = v;
        }
    }
    if (visitados < n)
        fprintf(stderr, "Aviso: el plan tiene ciclos; %d tareas nunca podran ejecutarse.\n",
                n - visitados);
    free(grado);
    free(cola);
}

/* ---------- API publica ---------- */

void limpiar_memoria(node_t *grafo, int total_nodos) {
    if (!grafo) return;
    for (int i = 0; i < total_nodos; i++) {
        dep_t *actual = grafo[i].next_tasks;
        while (actual) {
            dep_t *sig = actual->next;
            free(actual);
            actual = sig;
        }
    }
    free(grafo);
}

node_t *cargar_plan(const char *ruta_archivo, int *cantidad_nodos) {
    *cantidad_nodos = 0;
    FILE *f = fopen(ruta_archivo, "r");
    if (!f) return NULL;

    char *linea = NULL;
    size_t cap_linea = 0;
    char *id, *nombre, *tiempo, *deps;
    int r;

    /* Pasada 1: contar lineas validas */
    int n = 0, nro = 0;
    while (getline(&linea, &cap_linea, f) != -1) {
        nro++;
        r = dividir_linea(linea, &id, &nombre, &tiempo, &deps);
        if (r == 1) n++;
        else if (r < 0)
            fprintf(stderr, "Aviso: linea %d malformada, se ignora.\n", nro);
    }
    if (n == 0) { free(linea); fclose(f); return NULL; }

    node_t *g = calloc(n, sizeof(node_t));
    tabla_t tabla = {0};
    if (!g || !tabla_init(&tabla, n)) {
        free(g); free(tabla.e); free(linea); fclose(f);
        return NULL;
    }

    /* Pasada 2: datos base (id, nombre, tiempo) */
    rewind(f);
    int i = 0;
    while (i < n && getline(&linea, &cap_linea, f) != -1) {
        if (dividir_linea(linea, &id, &nombre, &tiempo, &deps) != 1) continue;

        strncpy(g[i].id, id, sizeof(g[i].id) - 1);
        strncpy(g[i].name, nombre, sizeof(g[i].name) - 1);

        char *fin_num;
        long val = strtol(tiempo, &fin_num, 10);
        if (*tiempo != '\0' && fin_num != tiempo && val >= 0)
            g[i].duration_ms = (int)val;
        else
            g[i].duration_ms = 100 + rand() % 4901;   /* 100..5000 ms */

        if (!tabla_insertar(&tabla, g[i].id, i))
            fprintf(stderr, "Aviso: ID duplicado '%s'; las dependencias usaran la primera.\n",
                    g[i].id);
        i++;
    }

    /* Pasada 3: dependencias */
    rewind(f);
    i = 0;
    while (i < n && getline(&linea, &cap_linea, f) != -1) {
        if (dividir_linea(linea, &id, &nombre, &tiempo, &deps) != 1) continue;

        char *save = NULL;
        for (char *tok = strtok_r(deps, " \t,[]\r\n", &save); tok;
             tok = strtok_r(NULL, " \t,[]\r\n", &save)) {
            int j = tabla_buscar(&tabla, tok);
            if (j < 0) {
                fprintf(stderr, "Aviso: '%s' depende de '%s', que no existe.\n",
                        g[i].id, tok);
                continue;
            }
            if (j == i) {
                fprintf(stderr, "Aviso: '%s' depende de si misma, se ignora.\n", g[i].id);
                continue;
            }
            dep_t *nueva = malloc(sizeof(dep_t));
            if (!nueva) {
                fprintf(stderr, "Error: sin memoria.\n");
                limpiar_memoria(g, n);
                free(tabla.e); free(linea); fclose(f);
                return NULL;
            }
            nueva->task = &g[i];
            nueva->next = g[j].next_tasks;
            g[j].next_tasks = nueva;
            g[i].unresolved_dependencies++;
        }
        i++;
    }

    free(tabla.e);
    free(linea);
    fclose(f);

    avisar_ciclos(g, n);
    *cantidad_nodos = n;
    return g;
}