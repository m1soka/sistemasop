#!/usr/bin/env python3
"""Genera un plan.txt valido (DAG) para pruebas de estres.

Uso: python3 gen_plan.py [N] [semilla] [salida]
Ejemplo: python3 gen_plan.py 10000 42 plan_grande.txt

Cada nodo solo depende de nodos con ID menor, asi que nunca hay ciclos.
Mezcla tres formas:
  - raices (sin dependencias)
  - cadenas (depende del nodo anterior)
  - ramas/uniones (depende de 2 o 3 nodos anteriores cercanos)
"""
import random
import sys

n = int(sys.argv[1]) if len(sys.argv) > 1 else 10000
semilla = int(sys.argv[2]) if len(sys.argv) > 2 else 42
salida = sys.argv[3] if len(sys.argv) > 3 else "plan_grande.txt"

random.seed(semilla)

with open(salida, "w") as f:
    for i in range(1, n + 1):
        dur = random.randint(1, 5)          # ms chicos para que la prueba sea rapida
        deps = []
        if i > 1:
            r = random.random()
            if r < 0.10:
                pass                                    # raiz
            elif r < 0.60:
                deps = [i - 1]                          # cadena
            else:
                k = random.choice([2, 3])
                ventana = range(max(1, i - 50), i)      # dependencias cercanas
                deps = sorted(random.sample(list(ventana), min(k, len(ventana))))
        f.write(f"{i} : tarea_{i} : {dur} : {', '.join(map(str, deps))}\n")

print(f"Plan de {n} actividades escrito en {salida}")