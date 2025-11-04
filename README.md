# Trabajo Final de Sistemas Operativos

## Integrantes
- **Felipe Castro**  
- **Juan José Botero**  
- **Samuel Villa**

---

## Descripción del Proyecto

Este proyecto implementa un **simulador de kernel** en C++ que integra los principales componentes de un sistema operativo, incluyendo:

### 1. Gestión de Procesos

Permite la simulación completa del ciclo de vida de los procesos (creación, suspensión, reanudación y terminación).

* **Ciclo de Vida:** Creación, suspensión, reanudación y terminación de procesos simulados.
* **Algoritmos de Planificación:**
    * **Round Robin (RR):** Con **quantum configurable** (por defecto: 2).
    * **Shortest Job First (SJF):** Selección por el **menor tiempo restante** (`SJF Preemptivo`).
* **Métricas Calculadas:**
    * **Tiempo de Espera Promedio.**
    * **Tiempo de Retorno (Turnaround Time).**
    * **Utilización del CPU.**
* **Características:** Soporte para procesos con **llegada diferida** y **finalización manual** (`kill <id>`).

---

### 2. Memoria Virtual y Paginación

Implementación de un gestor de marcos configurable para simular la memoria virtual y la paginación.

* **Gestor de Marcos:** Políticas de reemplazo configurables:
    * **FIFO** (First-In First-Out).
    * **LRU** (Least Recently Used).
    * **Working Set** (ventana configurable).
* **Configuración:** Permite cambiar el **número de marcos** y la **política** en tiempo de ejecución.
* **Estadísticas Generadas:**
    * Total de **accesos y fallos de página**.
    * **Tasa de fallos** (`Page Fault Rate`).
    * **Trazas de accesos** con tiempo simulado.
* **Visualización en Consola (`memview`):** Tabla de marcos con códigos de color:
    * **Verde:** `HIT` (acierto de página).
    * **Rojo:** `MISS` (fallo de página).
    * **Gris:** Marco `libre`.

---

### 3. Sincronización de Procesos

Incluye la simulación de problemas clásicos de concurrencia utilizando **semáforos**.

#### a. Productor–Consumidor

* Simula un **buffer compartido** de capacidad limitada.
* **Control de Concurrencia:** Utiliza los semáforos `sem_item` (para ítems producidos) y `sem_vacio` (para espacios disponibles).
* **Comandos:**
    * `produce <valor>`: Produce un ítem en el buffer.
    * `consume`: Consume un ítem del buffer.
    * `bufstat`: Muestra el estado actual del buffer.

#### b. Cena de los Filósofos

* Simula la competencia por **recursos compartidos (tenedores)** usando semáforos binarios.
* **Comando:**
    * `filosofos [num_filosofos] [rondas]`: Permite ejecutar rondas personalizadas.
* **Salida:** Muestra el estado de cada filósofo (pensando, bloqueado, comiendo).

---

### 4. Entrada/Salida y Manejo de Recursos

#### a. Dispositivos Simulados

* **Dispositivos:** Tres tipos disponibles: **disco**, **red**, **usb**.
* **Solicitud de E/S:** Contiene:
    * **Duración simulada.**
    * **Prioridad** (menor número = mayor prioridad).
* **Reinserción:** Al completar la E/S, el proceso vuelve a la cola de listos.
* **Comandos:**
    * `io <dispositivo> <pid> <duracion> <prioridad>`: Genera una solicitud de E/S.
    * `iotick`: Avanza un tick en la simulación de E/S.
    * `iostat`: Muestra el estado de los dispositivos de E/S.

#### b. Impresora Simulada

* **Buffer:** Capacidad limitada (por defecto: 3 trabajos).
* **Bloqueo:** Los procesos quedan bloqueados si la cola de impresión está llena.
* **Comandos:**
    * `print <pid> <texto>`: Proceso solicita enviar un trabajo a la impresora.
    * `printproc`: Impresora procesa el siguiente trabajo en la cola.
    * `printstat`: Muestra el estado actual de la cola de impresión.

---

### 5. Planificación de Disco

Implementación de algoritmos para simular el movimiento del cabezal del disco.

* **Algoritmos Incluidos:**
    * **FCFS** (First Come First Serve).
    * **SSTF** (Shortest Seek Time First).
    * **SCAN** (Elevator).
    * **Métricas:** Muestra el **recorrido del cabezal** y el **movimiento total de cilindros**.
   * **Visualización:** ASCII del trayecto y la línea de cilindros.
   * **Comando:**
    * `disk <fcfs|sstf|scan> <lista_de_solicitudes> [dir=up|down]`: Ejecuta la simulación de disco.

---

### 6. Interfaz de Usuario (CLI)

El simulador se opera mediante una **consola interactiva**.

* **Modos de Operación:**
    * Ejecución directa en terminal.
    * Lectura de scripts de comandos.
    * Comandos Principales (Ver listado de comandos).


---

## Funcionalidades Principales

| Comando | Descripción |
|:----------|:-------------|
| `new <tiempo> [llegada_offset]` | Crea proceso. |
| `run <ticks>` | Ejecuta planificador N ticks. |
| `tick` | Avanza 1 tick. |
| `ps` | Lista procesos. |
| `stats` | Métricas generales. |
| `kill <id>` | Termina proceso manualmente. |
| `modo <rr,sjf>` | Cambia planificador (Round Robin o Shortest Job First). |
| `mem <pid> <pag>` | Acceder página (usa GestorMarcos). |
| `memmode <fifo,lru,ws> [marcos]` | Cambia política de reemplazo y opcionalmente la cantidad de marcos. |
| `filosofos` | Simular la cena de los filósofos. |
| `impresora <pid> <texto>` | Proceso solicita imprimir. |
| `printproc` | Impresora procesa un trabajo. |
| `printstat` | Mostrar estado actual de la cola de impresión. |
| `memstat` | Mostrar estado memoria. |
| `memtrace` | Mostrar trazas memoria. |
| `memstats <pid>` | Mostrar stats de un proceso (accesos/fallos). |
| `sem_signal <name>` | (Simulado) signal en semáforo predefinido. |
| `produce <x>` | Producir ítem en buffer (simulado). |
| `consume` | Consumir ítem del buffer (simulado). |
| `bufstat` | Estado buffer sincronización. |
| `memview` | Vista ASCII de marcos de memoria (color HIT/MISS). |
| `disk <fcfs,sstf,scan>` | Ejecuta simulación de disco y visualización ASCII. |
| `exit` | Finaliza la ejecución. |

---

## Ejecución

### Compilar el programa
```bash
g++ main.cpp -o main.exe
```

### Ejecutar los scripts
```bash
Get-Content .\scripts\proc_test.txt | ./main.exe

Get-Content .\scripts\mem_test.txt | ./main.exe

Get-Content .\scripts\disk_test.txt | ./main.exe
```
