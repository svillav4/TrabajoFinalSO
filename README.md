# Trabajo Final de Sistemas Operativos

## Integrantes
- **Felipe Castro**  
- **Juan José Botero**  
- **Samuel Villa**

---

## Descripción del Proyecto

Este proyecto implementa un **simulador de kernel** en C++ que integra los principales componentes de un sistema operativo, incluyendo:

1. **Planificador Round Robin (RR)**  
   - Simula la ejecución de procesos con un *quantum* fijo.  
   - Permite crear, listar, ejecutar y finalizar procesos.  
   - Mide métricas como tiempo de espera, turnaround y utilización de CPU.

2. **Gestor de Memoria con Paginación**  
   - Implementa políticas de reemplazo **FIFO** y **LRU**.  
   - Muestra fallos de página, tasa de aciertos y trazas de accesos.  
   - Permite configurar el número de marcos de memoria dinámicamente.

3. **Mecanismos de Sincronización (Semáforos Simulados)**  
   - Se incluye una simulación del problema **Productor–Consumidor**.  
   - Permite producir y consumir elementos en un buffer con control de concurrencia simulado.

4. **CLI Básica (Interfaz de Línea de Comandos)**  
   - El simulador cuenta con una **CLI interactiva** que permite al usuario controlar el kernel mediante comandos escritos directamente en consola o cargados desde un archivo de script.  
   - Permite automatizar pruebas mediante archivos `.txt` que contienen secuencias de comandos.


---

## Funcionalidades Principales

| Comando | Descripción |
|----------|-------------|
| `new <tiempo> [llegada_offset]` | Crea un nuevo proceso con duración y llegada opcional. |
| `run <ticks>` | Ejecuta el planificador por N ticks. |
| `tick` | Avanza un tick del reloj del sistema. |
| `ps` | Lista los procesos con su estado actual. |
| `stats` | Muestra estadísticas generales de ejecución. |
| `kill <id>` | Termina un proceso manualmente. |
| `mem <pid> <pag>` | Simula el acceso a una página (memoria). |
| `memmode <fifo,lru> [marcos]` | Cambia la política de reemplazo y cantidad de marcos. |
| `memstat` | Muestra el estado actual de los marcos de memoria. |
| `memtrace` | Muestra las últimas trazas de acceso a memoria. |
| `memstats <pid>` | Muestra las estadísticas de memoria de un proceso. |
| `produce <x>` | Produce un ítem en el buffer (simulación de productor). |
| `consume` | Consume un ítem del buffer (simulación de consumidor). |
| `bufstat` | Muestra el estado actual del buffer. |
| `exit` | Finaliza la ejecución del simulador. |

---

## Ejecución

### Compilar el programa
```bash
g++ main.cpp -o main.exe
Get-Content demo.txt | ./main.exe
```
