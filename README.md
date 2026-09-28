# Fonda del Fork-lor

Simulador y planificador de actividades para las Fiestas Patrias del señor Loyola
(Tarea 1 - Sistemas Operativos). Cada actividad es un nodo de un DAG y se ejecuta como
un **proceso hijo** (`fork`), respetando las dependencias y un límite de concurrencia K.

**Integrantes:** _(completar nombres)_

## 1. Modo de uso (compilación y ejecución)

```bash
make                        # gcc -Wall -Wextra -std=c17 -g ... -lpthread
./planificador plan.txt K   # K = máximo de procesos hijos simultáneos
make clean
```

> `-lpthread` se incluye porque la rúbrica lo pide en la compilación estricta.
> El programa **no crea hilos** ni usa sincronización de hilos: solo `fork`, `pipe`,
> `wait` y señales.

Simular fallos internos (por defecto 0 %, sin fallos):

```bash
PROB_FALLO=10 ./planificador plan.txt 3    # 10 % de probabilidad de fallo por tarea
```

Generar un plan grande para la prueba de estrés:

```bash
python3 gen_plan.py 10000 42 plan_grande.txt
./planificador plan_grande.txt 8
```

## 2. Formato de plan.txt

```
ID : nombre : tiempo_ms : dep1, dep2
```

Se acepta también `[dep1, dep2]`. Si `tiempo_ms` está vacío, se asigna un valor aleatorio
entre 100 y 5000 ms. Las líneas vacías y las que empiezan con `#` se ignoran; las líneas
malformadas, los IDs duplicados o las dependencias inexistentes generan un aviso por `stderr`.

## 3. Funciones implementadas

| Función | Archivo | Descripción |
|---|---|---|
| `cargar_plan` | `parser.c` | Lee el archivo, crea el arreglo de nodos y construye el DAG. |
| `limpiar_memoria` | `parser.c` | Libera las listas de dependientes y el arreglo de nodos. |
| `iniciar_planificador` | `main.c` | Bucle principal: lanza tareas listas, espera terminaciones, propaga mensajes y aísla fallos. |
| `manejador_seremi` | `main.c` | Handler de SIGINT (Ctrl+C). |

## 4. Decisiones de diseño

- **DAG con contador de dependencias.** Cada nodo tiene `unresolved_dependencies` y una
  lista `next_tasks` de dependientes. Al terminar una tarea, el padre decrementa el
  contador de cada dependiente; con 0 la tarea queda lista.
- **Parseo en 3 pasadas con tabla hash.** Contar líneas, leer datos base y enlazar
  dependencias. Un mapa ID → índice (hash con direccionamiento abierto) evita la búsqueda
  lineal, por lo que cargar 10000 nodos es O(n). `getline()` evita truncar líneas largas.
- **Un proceso por tarea y límite K.** El padre lanza tareas mientras `activos < K`.
  Cuando llega a K, o no hay nada nuevo que lanzar, se bloquea en `wait()`.
- **Sin busy-waiting.** La única espera del padre es `wait()` bloqueante; el `nanosleep`
  ocurre solo en los hijos para simular trabajo.
- **Sin race conditions.** No hay memoria compartida: el padre es el único que modifica el
  grafo y los hijos se comunican solo por pipes. El handler de SIGINT solo escribe una
  variable `volatile sig_atomic_t` y usa `write()` (async-signal-safe). Se instala sin
  `SA_RESTART` para que `wait()` retorne con `EINTR` y el padre reaccione de inmediato.
- **Paso de mensajes con dos pipes por tarea.**
  - Hijo → padre: al terminar, la tarea escribe un mensaje acotado (100 bytes).
  - Padre → hijo: el padre acumula los mensajes de las dependencias terminadas de cada
    tarea y, antes del `fork`, los deja en un pipe que el hijo lee al iniciar (insumos
    acotados a 512 bytes). Así el mensaje de una actividad llega efectivamente a sus
    dependientes. El padre actúa de intermediario porque un hijo no puede conocer los
    pipes de procesos que aún no existen.
  - Hay como máximo K pipes de lectura abiertos a la vez (se cierran al terminar cada
    tarea), lo que permite 10000 actividades sin agotar descriptores de archivo.
- **Aislamiento de errores.** Si un hijo sale con código distinto de 0, solo se marcan como
  abortados sus dependientes (contador en -1); por transitividad, sus descendientes nunca
  se lanzan. El resto del plan sigue. Al final se informa cuántas tareas no se ejecutaron.
- **Inspección de la Seremi (Ctrl+C).** El padre marca la bandera, sale del bucle, envía
  `SIGKILL` a todos los hijos vivos y hace `wait()` hasta cosecharlos: no quedan zombies.
  Los hijos restauran `SIGINT` por defecto.
- **Robustez.** `fflush(stdout)` antes de cada `fork` y `_exit()` en los hijos (evita
  salida duplicada al redirigir); `srand` propio en cada hijo (los fallos aleatorios son
  independientes); aviso de ciclos en el plan (algoritmo de Kahn); validación de `K >= 1`.

## 5. Pruebas realizadas

- Plan de ejemplo con K = 3: respeta K, propaga insumos y termina con `SIMULACION FINALIZADA`.
- Plan de 10000 actividades, K = 8, sin fallos: se ejecutan las 10000 (~6 s).
- Con `PROB_FALLO` > 0: se abortan solo las ramas afectadas y el programa termina limpio.
- Ctrl+C durante la ejecución: no quedan procesos hijos vivos ni zombies.
- Salida redirigida a un archivo: sin líneas duplicadas.