#include <iostream>
#include <queue>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <sstream>
#include <deque>
#include <optional>
#include <iomanip>
using namespace std;


void procesar_comando(const string &comando);


// ------------------------- PCB -------------------------
struct Proceso {
    int id_proceso;
    int tiempo_requerido;
    int tiempo_restante;
    int tiempo_llegada;
    int tiempo_inicio;
    int tiempo_finalizacion;
    string estado; // "LISTO", "EJECUTANDO", "TERMINADO", "BLOQUEADO"
    int tiempo_espera_acumulado;
    int response_time; // tiempo hasta primer start
    int quantum_consumido;
    int quantums_served;

    Proceso(int id, int tiempo, int llegada) :
        id_proceso(id),
        tiempo_requerido(tiempo),
        tiempo_restante(tiempo),
        tiempo_llegada(llegada),
        tiempo_inicio(-1),
        tiempo_finalizacion(-1),
        estado("LISTO"),
        tiempo_espera_acumulado(0),
        response_time(-1),
        quantum_consumido(0),
        quantums_served(0) {}
};


// ------------------------- FrameManager + Tablas de páginas -------------------------
struct Frame {
    int pid = -1;
    int pagina = -1;
    int last_used = 0; // para LRU
};


struct AccesoRegistro {
    int tick;
    int pid;
    int pagina;
    bool hit;
    int tiempo_acceso_simulado; // ticks consumidos por el acceso
};


// ------------------------ Gestor de Marcos (Working Set) ---------------------------- //
class GestorMarcos {
private:
    vector<Frame> marcos;
    deque<int> cola_fifo;
    int max_marcos;
    int reloj_tick;
    int accesos_totales;
    int fallos_totales;
    vector<AccesoRegistro> trazas;

    // NUEVOS CAMPOS
    bool usar_lru;
    bool usar_ws;
    int ventana_ws; // tamaño de la ventana Working Set

public:
    map<int, map<int,int>> tabla_paginas;

    GestorMarcos(int n_marcos = 3, bool lru = false, bool ws = false, int ventana = 5)
        : max_marcos(n_marcos), reloj_tick(0), accesos_totales(0),
          fallos_totales(0), usar_lru(lru), usar_ws(ws), ventana_ws(ventana) {
        marcos.resize(max_marcos);
        for (int i = 0; i < max_marcos; ++i) cola_fifo.push_back(i);
    }

    int acceder_pagina(int pid, int pagina, bool lru_flag, bool &hit) {
        reloj_tick++;
        accesos_totales++;

        usar_lru = lru_flag; // actualiza modo activo dinámicamente

        // HIT
        if (tabla_paginas.count(pid) && tabla_paginas[pid].count(pagina)) {
            int marco = tabla_paginas[pid][pagina];
            if (marcos[marco].pid == pid && marcos[marco].pagina == pagina) {
                hit = true;
                marcos[marco].last_used = reloj_tick;
                trazas.push_back({reloj_tick, pid, pagina, true, 1});
                return 1;
            }
        }

        // MISS
        hit = false;
        fallos_totales++;
        int tiempo_miss = 10;

        int marco_libre = -1;
        for (int i = 0; i < max_marcos; ++i) {
            if (marcos[i].pid == -1) { marco_libre = i; break; }
        }

        int elegido = -1;
        if (marco_libre != -1) {
            elegido = marco_libre;
        } else if (usar_ws) {
            // WORKING SET: reemplaza página fuera del conjunto activo
            int candidato = -1;
            int oldest_time = INT_MAX;
            for (int i = 0; i < max_marcos; ++i) {
                int edad = reloj_tick - marcos[i].last_used;
                if (edad > ventana_ws) { // fuera del conjunto de trabajo
                    candidato = i;
                    break;
                }
                if (marcos[i].last_used < oldest_time) {
                    oldest_time = marcos[i].last_used;
                    elegido = i; // fallback: la más vieja si todas activas
                }
            }
            if (candidato != -1) elegido = candidato;
        } else if (usar_lru) {
            // LRU clásico
            int min_used = INT_MAX;
            for (int i = 0; i < max_marcos; ++i) {
                if (marcos[i].last_used < min_used) {
                    min_used = marcos[i].last_used;
                    elegido = i;
                }
            }
        } else {
            // FIFO
            elegido = cola_fifo.front();
            cola_fifo.pop_front();
            cola_fifo.push_back(elegido);
        }

        // limpiar entrada anterior
        int pid_victima = marcos[elegido].pid;
        int pag_victima = marcos[elegido].pagina;
        if (pid_victima != -1)
            tabla_paginas[pid_victima].erase(pag_victima);

        // asignar
        marcos[elegido].pid = pid;
        marcos[elegido].pagina = pagina;
        marcos[elegido].last_used = reloj_tick;
        tabla_paginas[pid][pagina] = elegido;

        trazas.push_back({reloj_tick, pid, pagina, false, tiempo_miss});
        return tiempo_miss;
    }

    void set_politica(bool lru, bool ws, int ventana = 5) {
        usar_lru = lru;
        usar_ws = ws;
        ventana_ws = ventana;
    }

    // Resto de metodos igual...
    void mostrar_estado() const {
        cout << "\n[Memoria] Estado de marcos (index: PID->Pagina, last_used)\n";
        for (int i = 0; i < max_marcos; ++i) {
            cout << " Marco[" << setw(2) << i << "]: ";
            if (marcos[i].pid == -1) cout << "(libre)\n";
            else cout << "PID " << marcos[i].pid << " -> Pag " << marcos[i].pagina
                      << " (last=" << marcos[i].last_used << ")\n";
        }
        cout << "Accesos totales: " << accesos_totales
             << " | Fallos totales: " << fallos_totales
             << " | Tasa fallos: "
             << (accesos_totales>0 ? (double)fallos_totales/accesos_totales : 0.0)
             << "\n";
    }

    void mostrar_trazas(int ultimos = 20) const {
        cout << "\n[Memoria] Ultimas trazas de acceso (tick pid pag HIT tiempo):\n";
        int inicio = max(0, (int)trazas.size() - ultimos);
        for (int i = inicio; i < (int)trazas.size(); ++i) {
            const auto &r = trazas[i];
            cout << " " << r.tick << " | PID " << r.pid << " | Pag " << r.pagina
                 << " | " << (r.hit ? "HIT " : "MISS")
                 << " | t=" << r.tiempo_acceso_simulado << "\n";
        }
    }

        // Libera todos los marcos usados por un proceso terminado
    void liberar_proceso(int pid) {
        for (int i = 0; i < max_marcos; ++i) {
            if (marcos[i].pid == pid) {
                marcos[i] = Frame(); // marco libre
            }
        }
        tabla_paginas.erase(pid);
    }

    void visualizar_memoria_ascii() const {
        cout << "\n[Visualizacion ASCII - Marcos de Memoria]\n";
        if (marcos.empty()) {
            cout << "(Sin marcos)\n";
            return;
        }

        const string verde = "\033[1;32m"; // verde para hit
        const string rojo = "\033[1;31m";  // rojo para miss
        const string gris = "\033[1;90m";  // gris para libre
        const string reset = "\033[0m";

        // determinar los ultimos accesos para marcar colores
        map<int, bool> ultimo_hit_por_marco;
        if (!trazas.empty()) {
            auto r = trazas.back();
            // busca el marco donde cayo el acceso
            for (int i = 0; i < max_marcos; ++i) {
                if (marcos[i].pid == r.pid && marcos[i].pagina == r.pagina) {
                    ultimo_hit_por_marco[i] = r.hit;
                }
            }
        }

        cout << "+--------------------------------------------------+\n";
        for (int i = 0; i < max_marcos; ++i) {
            string color = gris;
            string contenido = "(libre)";
            if (marcos[i].pid != -1) {
                contenido = "PID " + to_string(marcos[i].pid) +
                            " P" + to_string(marcos[i].pagina);
                if (ultimo_hit_por_marco.count(i))
                    color = (ultimo_hit_por_marco.at(i) ? verde : rojo);
            }
            cout << "| Marco[" << setw(2) << i << "]: "
                 << color << setw(15) << left << contenido << reset
                 << " (last=" << setw(3) << marcos[i].last_used << ") |\n";
        }
        cout << "+--------------------------------------------------+\n";
        cout << "HIT = verde | MISS = rojo | Libre = gris\n";
    }

};


// ------------------------- Planificador Round Robin -------------------------
class PlanificadorRR {
private:
    queue<shared_ptr<Proceso>> cola_listos;
    map<int, shared_ptr<Proceso>> procesos;
    int tiempo_quantum;
    int tiempo_actual;
    int sig_id_proceso;
    shared_ptr<Proceso> proceso_en_cpu; // proceso actualmente en CPU (o nullptr)
public:
    PlanificadorRR(int quantum=2) : tiempo_quantum(quantum), tiempo_actual(0), sig_id_proceso(1), proceso_en_cpu(nullptr) {}

    int crear_proceso(int tiempo_requerido, int llegada_offset = 0) {
        int llegada = tiempo_actual + llegada_offset;
        auto p = make_shared<Proceso>(sig_id_proceso++, tiempo_requerido, llegada);
        // Si llegada == tiempo_actual lo ponemos en cola de listos, si no, queda registrado y se insertará cuando llegue
        procesos[p->id_proceso] = p;
        if (llegada <= tiempo_actual) {
            cola_listos.push(p);
            cout << "[+] Proceso creado (RR) | id=" << p->id_proceso << " tiempo=" << tiempo_requerido << " (LISTO)\n";
        } else {
            p->estado = "LISTO"; // seguirá en estado LISTO pero no en cola hasta que llegue
            cout << "[+] Proceso creado con llegada futura | id=" << p->id_proceso << " tiempo=" << tiempo_requerido
                 << " llegada=" << p->tiempo_llegada << "\n";
        }
        return p->id_proceso;
    }

    // Exponer enqueue para semaforo signal
    void enqueue_ready(shared_ptr<Proceso> p) {
        if (p->estado != "TERMINADO") {
            p->estado = "LISTO";
            cola_listos.push(p);
        }
    }

    // actualiza colas con procesos cuya llegada ha ocurrido
    void incorporar_llegadas() {
        for (auto &kv : procesos) {
            auto p = kv.second;
            if (p->tiempo_llegada <= tiempo_actual && p->estado == "LISTO") {
                // si no está en cola debemos añadir; para evitar duplicados no comprobamos presencia en cola (simple)
                // Se puede mejorar con un flag 'en_cola'
                bool esta = false;
                // simple: si proceso_en_cpu es el mismo, no push
                if (proceso_en_cpu && proceso_en_cpu->id_proceso == p->id_proceso) esta = true;
                if (!esta) {
                    // Evitar añadir si ya finalizado
                    if (p->estado != "TERMINADO" && p->tiempo_restante > 0) {
                        cola_listos.push(p);
                    }
                }
            }
        }
    }

    // Ejecutar un tick (1 unidad de tiempo). Gestiona seleccion, preempcion y metricas.
    void tick(GestorMarcos &gestor, bool mem_lru_flag) {
        // incorporar llegadas antes de seleccionar
        incorporar_llegadas();

        // Si no hay proceso en CPU, seleccionar uno
        if (!proceso_en_cpu) {
            // eliminar procesos terminados del frente
            while (!cola_listos.empty() && cola_listos.front()->estado == "TERMINADO") cola_listos.pop();
            if (!cola_listos.empty()) {
                proceso_en_cpu = cola_listos.front();
                cola_listos.pop();
                if (proceso_en_cpu->tiempo_inicio == -1) proceso_en_cpu->tiempo_inicio = tiempo_actual;
                proceso_en_cpu->estado = "EJECUTANDO";
                proceso_en_cpu->quantum_consumido = 0;
                if (proceso_en_cpu->response_time == -1) {
                    proceso_en_cpu->response_time = tiempo_actual - proceso_en_cpu->tiempo_llegada;
                }
                proceso_en_cpu->quantums_served++;
                cout << "[TICK " << tiempo_actual << "] Context switch -> PID " << proceso_en_cpu->id_proceso << "\n";
            } else {
                cout << "[TICK " << tiempo_actual << "] CPU IDLE\n";
                tiempo_actual++;
                return;
            }
        }

        // Ejecutar 1 unidad del proceso en CPU
        tiempo_actual++;
        proceso_en_cpu->tiempo_restante -= 1;
        proceso_en_cpu->quantum_consumido += 1;

        // incrementar espera de los procesos listos
        for (auto &kv : procesos) {
            auto p = kv.second;
            if (p->estado == "LISTO") p->tiempo_espera_acumulado++;
        }

        cout << "[TICK " << tiempo_actual << "] Ejecutando PID " << proceso_en_cpu->id_proceso
             << " (restante=" << proceso_en_cpu->tiempo_restante << ", quantum_usado=" << proceso_en_cpu->quantum_consumido << ")\n";

        // comprobacion: si proceso termina
        if (proceso_en_cpu->tiempo_restante <= 0) {
            proceso_en_cpu->estado = "TERMINADO";
            proceso_en_cpu->tiempo_finalizacion = tiempo_actual;
            cout << "[EVENT] PID " << proceso_en_cpu->id_proceso << " TERMINADO en tick " << tiempo_actual << "\n";
            // liberar marcos asociados
            gestor.liberar_proceso(proceso_en_cpu->id_proceso);
            proceso_en_cpu = nullptr;
            return;
        }

        // si quantum consumido alcanza el quantum => preemption
        if (proceso_en_cpu->quantum_consumido >= tiempo_quantum) {
            proceso_en_cpu->estado = "LISTO";
            cola_listos.push(proceso_en_cpu);
            cout << "[EVENT] Preempcion -> PID " << proceso_en_cpu->id_proceso << " vuelve a cola LISTOS\n";
            proceso_en_cpu = nullptr;
            return;
        }
    }

    // Ejecutar N ticks secuenciales (usa tick())
    void ejecutar_ticks(int n, GestorMarcos &gestor, bool mem_lru_flag) {
        for (int i = 0; i < n; ++i) tick(gestor, mem_lru_flag);
    }

    void listar_procesos() {
        cout << "\nID\tEstado\tRest\tInicio\tFin\tEspera\tResp\tQuantServed\n";
        for (auto &kv : procesos) {
            auto p = kv.second;
            cout << p->id_proceso << "\t" << p->estado << "\t" << p->tiempo_restante << "\t"
                 << p->tiempo_inicio << "\t" << p->tiempo_finalizacion << "\t"
                 << p->tiempo_espera_acumulado << "\t" << p->response_time << "\t" << p->quantums_served << "\n";
        }
    }

    void estadisticas_generales() {
        double espera_total = 0;
        double retorno_total = 0;
        int finalizados = 0;
        int n = procesos.size();
        int cpu_ticks_ocupados = 0;
        int makespan = tiempo_actual;
        for (auto &kv : procesos) {
            auto p = kv.second;
            if (p->tiempo_finalizacion != -1) {
                int turnaround = p->tiempo_finalizacion - p->tiempo_llegada;
                int waiting = turnaround - p->tiempo_requerido;
                espera_total += waiting;
                retorno_total += turnaround;
                finalizados++;
            }
            cpu_ticks_ocupados += (p->tiempo_requerido - p->tiempo_restante);
        }
        cout << fixed << setprecision(3);
        if (finalizados == 0) cout << "Aun no hay procesos finalizados.\n";
        else {
            cout << "Promedio espera: " << (espera_total / finalizados)
                 << " | Promedio turnaround: " << (retorno_total / finalizados) << "\n";
        }
        double utilizacion = (makespan>0 ? (double)cpu_ticks_ocupados / makespan : 0.0);
        cout << "Tiempo actual (makespan sim): " << tiempo_actual << " | CPU utilisation (sim): " << utilizacion << "\n";
    }

    shared_ptr<Proceso> obtener_proceso(int id) {
        return procesos.count(id) ? procesos[id] : nullptr;
    }

    void terminar_proceso(int id) {
        if (!procesos.count(id)) {
            cout << "[!] No existe proceso " << id << "\n";
            return;
        }
        auto p = procesos[id];
        p->estado = "TERMINADO";
        p->tiempo_finalizacion = tiempo_actual;
        cout << "[X] Proceso " << id << " terminado manualmente.\n";
    }
};


// ------------------------- Planificador SJF -------------------------
class PlanificadorSJF {
private:
    vector<shared_ptr<Proceso>> cola_listos;
    map<int, shared_ptr<Proceso>> procesos;
    int tiempo_actual;
    int sig_id_proceso;
    shared_ptr<Proceso> proceso_en_cpu;

public:
    PlanificadorSJF() : tiempo_actual(0), sig_id_proceso(1), proceso_en_cpu(nullptr) {}

    int crear_proceso(int tiempo_requerido, int llegada_offset = 0) {
        int llegada = tiempo_actual + llegada_offset;
        auto p = make_shared<Proceso>(sig_id_proceso++, tiempo_requerido, llegada);
        procesos[p->id_proceso] = p;
        cola_listos.push_back(p);
        cout << "[+] Proceso creado (SJF) | id=" << p->id_proceso << " tiempo=" << tiempo_requerido << "\n";
        return p->id_proceso;
    }

    void tick(GestorMarcos &gestor, bool mem_lru_flag) {
        // eliminar terminados de la lista
        cola_listos.erase(remove_if(cola_listos.begin(), cola_listos.end(),
            [](auto &p){ return p->estado == "TERMINADO"; }), cola_listos.end());

        if (!proceso_en_cpu) {
            if (cola_listos.empty()) {
                cout << "[TICK " << tiempo_actual << "] CPU IDLE\n";
                tiempo_actual++;
                return;
            }

            // seleccionar el proceso con menor tiempo restante
            sort(cola_listos.begin(), cola_listos.end(),
                 [](auto &a, auto &b){ return a->tiempo_restante < b->tiempo_restante; });

            proceso_en_cpu = cola_listos.front();
            cola_listos.erase(cola_listos.begin());

            if (proceso_en_cpu->tiempo_inicio == -1)
                proceso_en_cpu->tiempo_inicio = tiempo_actual;

            proceso_en_cpu->estado = "EJECUTANDO";
            cout << "[TICK " << tiempo_actual << "] SJF selecciona PID " << proceso_en_cpu->id_proceso << "\n";
        }

        // ejecutar 1 tick
        tiempo_actual++;
        proceso_en_cpu->tiempo_restante -= 1;

        for (auto &kv : procesos) {
            auto p = kv.second;
            if (p->estado == "LISTO") p->tiempo_espera_acumulado++;
        }

        cout << "[TICK " << tiempo_actual << "] Ejecutando PID " << proceso_en_cpu->id_proceso
             << " (restante=" << proceso_en_cpu->tiempo_restante << ")\n";

        if (proceso_en_cpu->tiempo_restante <= 0) {
            proceso_en_cpu->estado = "TERMINADO";
            proceso_en_cpu->tiempo_finalizacion = tiempo_actual;
            cout << "[EVENT] PID " << proceso_en_cpu->id_proceso << " TERMINADO en tick " << tiempo_actual << "\n";
            gestor.liberar_proceso(proceso_en_cpu->id_proceso);
            proceso_en_cpu = nullptr;
        }
    }

    void ejecutar_ticks(int n, GestorMarcos &gestor, bool mem_lru_flag) {
        for (int i = 0; i < n; ++i) tick(gestor, mem_lru_flag);
    }

    shared_ptr<Proceso> obtener_proceso(int id) {
        auto it = procesos.find(id);
        if (it != procesos.end())
            return it->second;
        return nullptr;
    }

    void listar_procesos() {
        cout << "\nID\tEstado\tRest\tInicio\tFin\tEspera\n";
        for (auto &kv : procesos) {
            auto p = kv.second;
            cout << p->id_proceso << "\t" << p->estado << "\t" << p->tiempo_restante
                 << "\t" << p->tiempo_inicio << "\t" << p->tiempo_finalizacion
                 << "\t" << p->tiempo_espera_acumulado << "\n";
        }
    }
};


// ------------------------- Semáforo simulado -------------------------
class SemaforoSimulado {
private:
    int valor;
    queue<shared_ptr<Proceso>> cola_bloqueados;
public:
    SemaforoSimulado(int v = 1): valor(v) {}
    // Wait: si valor>0 decrementa, si no, bloquea al proceso (cambia su estado)
    void wait(shared_ptr<Proceso> p) {
        if (valor > 0) {
            valor--;
        } else {
            p->estado = "BLOQUEADO";
            cola_bloqueados.push(p);
        }
    }
    // Signal: si hay bloqueados, despierta al primero; si no, incrementa valor
    shared_ptr<Proceso> signal() {
        if (!cola_bloqueados.empty()) {
            auto p = cola_bloqueados.front();
            cola_bloqueados.pop();
            p->estado = "LISTO";
            return p;
        } else {
            valor++;
            return nullptr;
        }
    }
    int get_valor() const { return valor; }
    size_t bloqueados() const { return cola_bloqueados.size(); }
};


// ------------------------- Productor-Consumidor simulado -------------------------
class ProductorConsumidorSimulado {
private:
    deque<int> buffer;
    size_t capacidad;
    SemaforoSimulado sem_vacio; // cuenta de vacios (capacidad)
    SemaforoSimulado sem_item;  // cuenta de items disponibles
public:
    ProductorConsumidorSimulado(size_t cap=5)
        : capacidad(cap), sem_vacio((int)cap), sem_item(0) {}

    // Produce de forma simulada: si buffer lleno => bloquea (retornamos false)
    bool producir_simulado(int item) {
        // si hay hueco inmediato
        if (buffer.size() < capacidad) {
            buffer.push_back(item);
            // signal item
            sem_item.signal();
            // consume un espacio de vacio (decrement) ya fue consumido por push
            // en el modelo simulado no usamos valor interno de sem_vacio aqui
            cout << "[SYNC] Producido " << item << " (buffer=" << buffer.size() << ")\n";
            return true;
        } else {
            cout << "[SYNC] Buffer lleno: produccion bloqueada (simulado) \n";
            return false;
        }
    }

    // Consume de forma simulada: si buffer vacio => bloquea (retornamos false)
    bool consumir_simulado() {
        if (!buffer.empty()) {
            int item = buffer.front();
            buffer.pop_front();
            sem_vacio.signal();
            cout << "[SYNC] Consumido " << item << " (restan=" << buffer.size() << ")\n";
            return true;
        } else {
            cout << "[SYNC] Buffer vacio: consumo bloqueado (simulado)\n";
            return false;
        }
    }

    void estado_buffer() const {
        cout << "[SYNC] Elementos en buffer: " << buffer.size() << " / " << capacidad << "\n";
    }
};


// ------------------------- Cena de los Filosofos (simulada) ------------------------- //
class FilosofoSimulado {
private:
    int id;
    shared_ptr<Proceso> proceso;
    SemaforoSimulado* tenedor_izq;
    SemaforoSimulado* tenedor_der;

public:
    FilosofoSimulado(int _id, shared_ptr<Proceso> p, 
                     SemaforoSimulado* izq, SemaforoSimulado* der)
        : id(_id), proceso(p), tenedor_izq(izq), tenedor_der(der) {}

    void ciclo() {
        cout << "[Filosofo " << id << "] Pensando...\n";
        this_thread::sleep_for(chrono::milliseconds(300));

        cout << "[Filosofo " << id << "] Intenta tomar tenedores\n";
        tenedor_izq->wait(proceso);
        tenedor_der->wait(proceso);

        if (proceso->estado != "BLOQUEADO") {
            proceso->estado = "EJECUTANDO";
            cout << "[Filosofo " << id << "] Comiendo 🍝\n";
            this_thread::sleep_for(chrono::milliseconds(300));

            tenedor_izq->signal();
            tenedor_der->signal();
            proceso->estado = "LISTO";
            cout << "[Filosofo " << id << "] Termina de comer y suelta tenedores\n";
        } else {
            cout << "[Filosofo " << id << "] Bloqueado esperando tenedores...\n";
        }
    }

    string estado() const { return proceso->estado; }
    int getId() const { return proceso->id_proceso; }
};


// ------------------------- Simulacion de la cena -------------------------
void simular_cena_filosofos(int N = 5, int rondas = 3) {
    cout << "\n=== Simulacion: Cena de los Filosofos ===\n";

    vector<SemaforoSimulado> tenedores(N, SemaforoSimulado(1));
    vector<shared_ptr<Proceso>> procesos;
    vector<FilosofoSimulado> filosofos;

    // Crear filosofos y procesos
    for (int i = 0; i < N; ++i) {
        auto p = make_shared<Proceso>(i+1, 1, 0); // id_proceso, tiempo, llegada
        procesos.push_back(p);
        filosofos.emplace_back(i, p, &tenedores[i], &tenedores[(i + 1) % N]);
    }

    // Simular varias rondas
    for (int r = 0; r < rondas; ++r) {
        cout << "\n--- RONDA " << r + 1 << " ---\n";
        for (auto &f : filosofos) f.ciclo();

        cout << "\nEstado tras ronda " << r + 1 << ":\n";
        for (auto &f : filosofos) {
            cout << "  Filosofo " << f.getId() << " -> " << f.estado() << "\n";
        }
    }

    cout << "\n=== Fin de la simulacion de los filosofos ===\n";
}


// ------------------------- Solicitud de E/S con prioridad -------------------------
struct SolicitudIO {
    int prioridad;
    shared_ptr<Proceso> proceso;
    int duracion;
    int tiempo_inicio;
    int tiempo_restante;

    bool operator<(const SolicitudIO& other) const {
        // prioridad más alta -> primero (menor numero = mayor prioridad)
        return prioridad > other.prioridad;
    }
};


// ------------------------- Dispositivos Simulados (E/S con prioridad) -------------------------
class DispositivoSimulado {
private:
    string nombre;
    bool ocupado;
    priority_queue<SolicitudIO> cola;
    optional<SolicitudIO> en_ejecucion;
    PlanificadorRR* planificador; // para reinsertar procesos al finalizar

public:
    DispositivoSimulado()
        : nombre(""), ocupado(false), planificador(nullptr) {}

    DispositivoSimulado(string n, PlanificadorRR* p = nullptr)
        : nombre(std::move(n)), ocupado(false), planificador(p) {}

    void setPlanificador(PlanificadorRR* p) { planificador = p; }

    void solicitar(shared_ptr<Proceso> proceso, int duracion, int prioridad) {
        SolicitudIO req{prioridad, proceso, duracion, -1, duracion};
        cola.push(req);
        proceso->estado = "BLOQUEADO";
        cout << "[IO] PID " << proceso->id_proceso << " solicita " << nombre
             << " (dur=" << duracion << ", prio=" << prioridad << ")\n";
    }

    void tick() {
        if (!ocupado) {
            if (!cola.empty()) {
                en_ejecucion = cola.top();
                cola.pop();
                ocupado = true;
                en_ejecucion->tiempo_inicio = 0;
                cout << "[IO] " << nombre << " atendiendo PID "
                     << en_ejecucion->proceso->id_proceso << "\n";
            }
            return;
        }

        if (en_ejecucion) {
            en_ejecucion->tiempo_restante--;
            if (en_ejecucion->tiempo_restante <= 0) {
                cout << "[IO] PID " << en_ejecucion->proceso->id_proceso
                     << " termino en " << nombre << "\n";
                en_ejecucion->proceso->estado = "LISTO";
                if (planificador)
                    planificador->enqueue_ready(en_ejecucion->proceso);
                ocupado = false;
                en_ejecucion.reset();
            }
        }
    }

    void estado() const {
        cout << "[IO] " << nombre << " -> "
             << (ocupado ? "OCUPADO" : "LIBRE")
             << " | Cola: " << cola.size() << " solicitudes\n";
    }
};


// --------------- Gestor Dispositivos (Simulados) ------------------ //
class GestorDispositivos {
private:
    map<string, DispositivoSimulado> dispositivos;

public:
    GestorDispositivos(PlanificadorRR* planificador) {
        dispositivos["disco"] = DispositivoSimulado("Disco", planificador);
        dispositivos["red"]   = DispositivoSimulado("Red", planificador);
        dispositivos["usb"]   = DispositivoSimulado("USB", planificador);
    }

    void solicitar(const string& nombre, shared_ptr<Proceso> p, int duracion, int prioridad) {
        if (!dispositivos.count(nombre)) {
            cout << "[!] Dispositivo '" << nombre << "' no existe\n";
            return;
        }
        dispositivos[nombre].solicitar(p, duracion, prioridad);
    }

    void tick() {
        for (auto& kv : dispositivos) kv.second.tick();
    }

    void estado() const {
        cout << "\n=== Estado de dispositivos ===\n";
        for (auto& kv : dispositivos) kv.second.estado();
    }
};


// ------------------------- Impresora Simulada -------------------------
class ImpresoraSimulada {
private:
    deque<pair<int, string>> cola_impresion; // (pid, contenido)
    size_t capacidad;
    SemaforoSimulado sem_vacio;  // capacidad disponible
    SemaforoSimulado sem_ocupado; // trabajos en cola
    mutex mtx; // control de concurrencia simulada
public:
    ImpresoraSimulada(size_t cap = 3)
        : capacidad(cap), sem_vacio((int)cap), sem_ocupado(0) {}

    // Proceso solicita imprimir algo
    bool producir_impresion(shared_ptr<Proceso> p, const string &contenido) {
        lock_guard<mutex> lock(mtx);

        if (cola_impresion.size() < capacidad) {
            cola_impresion.push_back({p->id_proceso, contenido});
            cout << "[IMPRESORA] Proceso " << p->id_proceso 
                 << " envia trabajo: '" << contenido << "'\n";
            sem_ocupado.signal();
            return true;
        } else {
            cout << "[IMPRESORA] Cola llena. Proceso " << p->id_proceso 
                 << " bloqueado (esperando turno)...\n";
            p->estado = "BLOQUEADO";
            return false;
        }
    }

    // Impresora procesa un trabajo (simula consumo)
    bool procesar_impresion() {
        lock_guard<mutex> lock(mtx);

        if (!cola_impresion.empty()) {
            auto trabajo = cola_impresion.front();
            cola_impresion.pop_front();
            sem_vacio.signal();

            cout << "[IMPRESORA] Imprimiendo trabajo de PID " << trabajo.first
                 << " -> \"" << trabajo.second << "\"\n";
            this_thread::sleep_for(chrono::milliseconds(300));
            cout << "[IMPRESORA] Trabajo de PID " << trabajo.first << " completado\n";
            return true;
        } else {
            cout << "[IMPRESORA] No hay trabajos pendientes.\n";
            return false;
        }
    }

    void estado() const {
        cout << "\n[IMPRESORA] Estado actual:\n";
        cout << " Trabajos en cola: " << cola_impresion.size() 
             << " / " << capacidad << "\n";
        for (auto &t : cola_impresion) {
            cout << "  PID " << t.first << " -> \"" << t.second << "\"\n";
        }
    }
};


// ------------------------- Simulacion de planificacion de disco -------------------------
class SimuladorDisco {
private:
    vector<int> solicitudes;
    int posicion_inicial;
    int movimiento_total;

    void graficar_recorrido(const vector<int>& recorrido, const string& nombre_algoritmo) {
        cout << "\n[GRAFICO] Recorrido del cabezal (" << nombre_algoritmo << ")\n";
        cout << "Cilindros: ";
        for (int c : recorrido) cout << c << " ";
        cout << "\n\nEje aproximado (posicion del cabezal por orden de acceso):\n";

        int min_c = *min_element(recorrido.begin(), recorrido.end());
        int max_c = *max_element(recorrido.begin(), recorrido.end());
        int rango = max_c - min_c + 1;

        for (size_t i = 0; i < recorrido.size(); ++i) {
            int offset = (int)((double)(recorrido[i] - min_c) / rango * 60); // ancho aproximado
            cout << setw(2) << i+1 << " | " << string(offset, '-') << "*(" << recorrido[i] << ")\n";
        }
    }

public:
    SimuladorDisco(vector<int> reqs, int pos_inicial)
        : solicitudes(reqs), posicion_inicial(pos_inicial), movimiento_total(0) {}

    void ejecutar_FCFS() {
        cout << "\n[DISK] Algoritmo FCFS\n";
        int pos = posicion_inicial;
        movimiento_total = 0;
        vector<int> recorrido = {pos};

        for (int r : solicitudes) {
            int movimiento = abs(r - pos);
            cout << "  Cabezal: " << pos << " -> " << r
                 << " (mov=" << movimiento << ")\n";
            movimiento_total += movimiento;
            pos = r;
            recorrido.push_back(pos);
        }
        cout << "Movimiento total FCFS: " << movimiento_total << "\n";
        graficar_recorrido(recorrido, "FCFS");
    }

    void ejecutar_SSTF() {
        cout << "\n[DISK] Algoritmo SSTF\n";
        vector<int> pendientes = solicitudes;
        int pos = posicion_inicial;
        movimiento_total = 0;
        vector<int> recorrido = {pos};

        while (!pendientes.empty()) {
            auto it = min_element(pendientes.begin(), pendientes.end(),
                                  [pos](int a, int b) {
                                      return abs(a - pos) < abs(b - pos);
                                  });
            int r = *it;
            int movimiento = abs(r - pos);
            cout << "  Cabezal: " << pos << " -> " << r
                 << " (mov=" << movimiento << ")\n";
            movimiento_total += movimiento;
            pos = r;
            recorrido.push_back(pos);
            pendientes.erase(it);
        }
        cout << "Movimiento total SSTF: " << movimiento_total << "\n";
        graficar_recorrido(recorrido, "SSTF");
    }

    void ejecutar_SCAN(bool hacia_derecha = true, int max_cilindro = 199) {
        cout << "\n[DISK] Algoritmo SCAN (" << (hacia_derecha ? "→ derecha" : "← izquierda") << ")\n";
        vector<int> menores, mayores;
        int pos = posicion_inicial;
        movimiento_total = 0;

        for (int r : solicitudes) {
            if (r < pos) menores.push_back(r);
            else mayores.push_back(r);
        }
        sort(menores.begin(), menores.end());
        sort(mayores.begin(), mayores.end());

        vector<int> recorrido = {pos};

        if (hacia_derecha) {
            for (int r : mayores) recorrido.push_back(r);
            recorrido.push_back(max_cilindro);
            for (int i = (int)menores.size() - 1; i >= 0; --i) recorrido.push_back(menores[i]);
        } else {
            for (int i = (int)menores.size() - 1; i >= 0; --i) recorrido.push_back(menores[i]);
            recorrido.push_back(0);
            for (int r : mayores) recorrido.push_back(r);
        }

        for (size_t i = 1; i < recorrido.size(); ++i) {
            int mov = abs(recorrido[i] - recorrido[i - 1]);
            cout << "  Cabezal: " << recorrido[i - 1] << " -> " << recorrido[i]
                 << " (mov=" << mov << ")\n";
            movimiento_total += mov;
        }

        cout << "Movimiento total SCAN: " << movimiento_total << "\n";
        graficar_recorrido(recorrido, "SCAN");
    }
    void visualizar_linea_disco(int max_cilindro = 199, int ancho = 80) {
        cout << "\n[Visualizacion Disco - Linea de Cilindros]\n";

        int pos = posicion_inicial;
        vector<char> linea(ancho, '-');

        // Mapea los cilindros solicitados en la linea
        for (int r : solicitudes) {
            int idx = (r * ancho) / max_cilindro;
            if (idx >= 0 && idx < ancho) linea[idx] = '|';
        }

        // Marca el cabezal
        int pos_idx = (pos * ancho) / max_cilindro;
        if (pos_idx >= 0 && pos_idx < ancho) linea[pos_idx] = 'O';

        // Render
        for (char c : linea) cout << c;
        cout << "\nPosicion cabezal: " << pos
            << " | Solicitudes: " << solicitudes.size() << "\n";
    }

};


// ------------------------- CLI y main -------------------------
int main() {
    PlanificadorRR planificador_rr(2);         // quantum = 2
    PlanificadorSJF planificador_sjf;
    GestorMarcos gestor(3);                 // 3 marcos por defecto
    bool modo_lru = false;
    ProductorConsumidorSimulado sync_sim(5);
    ImpresoraSimulada impresora(3);

    int modo_planificador = 1; // 1=RR, 2=SJF
    GestorDispositivos gestor_io(&planificador_rr);


    cout << "=== SIMULADOR DE KERNEL ===\n";
    cout << "Seleccionar planificador inicial:\n"
         << "  1 -> Round Robin (RR)\n"
         << "  2 -> Shortest Job First (SJF)\n"
         << "Opcion: ";
    cin >> modo_planificador;
    cin.ignore();

    cout << "\n[!] Planificador actual: " 
         << (modo_planificador == 1 ? "Round Robin (RR)" : "Shortest Job First (SJF)") << "\n\n";

    cout << "=== SIMULADOR DE KERNEL ===\n";
    cout << "Comandos:\n"
         << "  new <tiempo> [llegada_offset]    -> crea proceso\n"
         << "  run <ticks>                      -> ejecuta planificador N ticks\n"
         << "  tick                             -> avanza 1 tick\n"
         << "  ps                               -> lista procesos\n"
         << "  stats                            -> metricas generales\n"
         << "  kill <id>                        -> termina proceso manualmente\n"
         << "  modo <rr|sjf>                    -> cambia planificador\n"
         << "  mem <pid> <pag>                  -> acceder pagina (usa GestorMarcos)\n"
         << "  memmode <fifo|lru|ws> [marcos]   -> cambia politica y opcional marcos\n"
         << "  filosofos                        -> simular la cena de los filosofos\n"
         << "  impresora <pid> <texto>          -> proceso solicita imprimir\n"
         << "  printproc                        -> impresora procesa un trabajo\n"
         << "  printstat                        -> mostrar estado actual de la cola\n"
         << "  memstat                          -> mostrar estado memoria\n"
         << "  memtrace                         -> mostrar trazas memoria\n"
         << "  memstats <pid>                   -> mostrar stats de un proceso (accesos/fallos)\n"
         << "  sem_signal <name>                -> (simulado) signal en semaforo predef\n"
         << "  produce <x>                      -> producir item en buffer (simulado)\n"
         << "  consume                          -> consumir item del buffer (simulado)\n"
         << "  bufstat                          -> estado buffer sincronizacion\n"
         << "  memview                          -> vista ASCII de marcos de memoria (color HIT/MISS)\n"
         << "  disk <fcfs|sstf|scan>            -> ejecuta simulacion de disco y visualizacion ASCII\n"
         << "  exit\n";

    string linea;
    while (true) {
        cout << "\n> ";
        if (!getline(cin, linea)) break;
        stringstream ss(linea);
        string cmd;
        ss >> cmd;

        // Creacion de procesos 
        if (cmd == "new") {
            int t; int off = 0;
            ss >> t;
            if (ss.fail()) { cout << "Uso: new <tiempo> [llegada_offset]\n"; continue; }
            if (!(ss >> off)) off = 0;
            if (modo_planificador == 1)
                planificador_rr.crear_proceso(t, off);
            else
                planificador_sjf.crear_proceso(t, off);
        } 
        // Ejecucion de ticks
        else if (cmd == "run") {
            int n; ss >> n;
            if (ss.fail()) { cout << "run <ticks>\n"; continue; }
            if (modo_planificador == 1)
                planificador_rr.ejecutar_ticks(n, gestor, modo_lru);
            else
                planificador_sjf.ejecutar_ticks(n, gestor, modo_lru);
        }

        // Tick individual
        else if (cmd == "tick") {
            if (modo_planificador == 1)
                planificador_rr.tick(gestor, modo_lru);
            else
                planificador_sjf.tick(gestor, modo_lru);
        }
        // Solicitud de E/S
        else if (cmd == "io") {
            string dev; int pid, dur, prio;
            ss >> dev >> pid >> dur >> prio;
            if (ss.fail()) { cout << "Uso: io <dispositivo> <pid> <duracion> <prioridad>\n"; continue; }

            auto p = planificador_rr.obtener_proceso(pid);
            if (!p) { cout << "[!] No existe proceso " << pid << "\n"; continue; }

            gestor_io.solicitar(dev, p, dur, prio);
        }

        // Avanzar tick de E/S
        else if (cmd == "iotick") {
            gestor_io.tick();
        }

        // Mostrar estado de dispositivos
        else if (cmd == "iostat") {
            gestor_io.estado();
        }
        // Listar procesos
        else if (cmd == "ps") {
            if (modo_planificador == 1)
                planificador_rr.listar_procesos();
            else
                planificador_sjf.listar_procesos();
        }

        // Estadisticas
        else if (cmd == "stats") {
            if (modo_planificador == 1)
                planificador_rr.estadisticas_generales();
            else
                cout << "[SJF] (No implementadas estadisticas globales aun)\n";
        }

        // Terminar proceso
        else if (cmd == "kill") {
            int id; ss >> id;
            if (ss.fail()) { cout << "kill <id>\n"; continue; }
            if (modo_planificador == 1) {
                planificador_rr.terminar_proceso(id);
                gestor.liberar_proceso(id);
            } else {
                cout << "[SJF] Terminacion manual no implementada.\n";
            }
        }

        // Cambiar planificador
        else if (cmd == "modo") {
            string modo; ss >> modo;
            if (modo == "rr") {
                modo_planificador = 1;
                cout << "[!] Cambiado a Round Robin (RR)\n";
            } else if (modo == "sjf") {
                modo_planificador = 2;
                cout << "[!] Cambiado a Shortest Job First (SJF)\n";
            } else {
                cout << "Uso: modo <rr|sjf>\n";
            }
        }

        // Memoria
        else if (cmd == "mem") {
            int pid, pag;
            ss >> pid >> pag;
            if (ss.fail()) { cout << "mem <pid> <pag>\n"; continue; }

            shared_ptr<Proceso> p;
            if (modo_planificador == 1) p = planificador_rr.obtener_proceso(pid);
            else cout << "[!] Acceso memoria no disponible en modo SJF\n";

            if (!p) { cout << "[!] No existe proceso " << pid << "\n"; continue; }

            bool hit = false;
            int tiempo_acceso = gestor.acceder_pagina(pid, pag, modo_lru, hit);
            cout << (hit ? "[MEM] HIT " : "[MEM] MISS ")
                 << " PID " << pid << " Pag " << pag
                 << " (tiempo_sim=" << tiempo_acceso << " ticks)\n";
        }

        // Configuracion memoria
        else if (cmd == "memmode") {
            string modo; int marcos = 3; int ventana = 5;
            ss >> modo;
            if (ss.fail()) { cout << "memmode <fifo|lru|ws> [marcos] [ventana]\n"; continue; }

            if (ss >> marcos) { /* opcional numero de marcos */ }
            if (ss >> ventana) { /* opcional ventana WS */ }

            bool lru = false, ws = false;
            if (modo == "lru") lru = true;
            else if (modo == "ws") ws = true;

            gestor = GestorMarcos(marcos, lru, ws, ventana);
            cout << "[!] Reiniciado gestor de marcos con " << marcos << " marcos\n";
            if (ws) cout << "[!] Politica memoria: Working Set (ventana=" << ventana << ")\n";
            else cout << "[!] Politica memoria: " << (lru ? "LRU" : "FIFO") << "\n";
        }

        // Cena filosofos
        else if (cmd == "filosofos") {
            int n = 5, rondas = 3;
            ss >> n >> rondas;
            if (ss.fail()) {
                cout << "Uso: filosofos [num_filosofos] [rondas]\n";
                continue;
            }
            simular_cena_filosofos(n, rondas);
        }

        // Impresora
        else if (cmd == "print") {
            int pid; 
            string contenido;
            ss >> pid;
            getline(ss, contenido);
            if (ss.fail() || contenido.empty()) {
                cout << "Uso: print <pid> <texto>\n";
                continue;
            }
            // Eliminar espacios iniciales en el contenido
            contenido.erase(0, contenido.find_first_not_of(" \t"));

            shared_ptr<Proceso> p = planificador_rr.obtener_proceso(pid);
            if (!p)
                p = planificador_sjf.obtener_proceso(pid);

            if (!p) {
                cout << "[!] No existe proceso con PID " << pid << "\n";
                continue;
            }

            impresora.producir_impresion(p, contenido);
        }

        else if (cmd == "printproc") {
            impresora.procesar_impresion();
        }

        else if (cmd == "printstat") {
            impresora.estado();
        }
        else if (cmd == "disk") {
            string algoritmo;
            ss >> algoritmo;
            if (ss.fail()) {
                cout << "Uso: disk <fcfs|sstf|scan>\n";
                continue;
            }

            vector<int> reqs = {55, 58, 60, 70, 18, 90, 150, 38, 184};
            int pos_inicial = 50;
            SimuladorDisco disco(reqs, pos_inicial);

            if (algoritmo == "fcfs") disco.ejecutar_FCFS();
            else if (algoritmo == "sstf") disco.ejecutar_SSTF();
            else if (algoritmo == "scan") disco.ejecutar_SCAN(true);
            else {
                cout << "[!] Algoritmo no reconocido\n";
                continue;
            }

            disco.visualizar_linea_disco();
        }


        // Estadisticas memoria
        else if (cmd == "memstat") gestor.mostrar_estado();
        else if (cmd == "memtrace") gestor.mostrar_trazas();
        else if (cmd == "memview") {
            gestor.visualizar_memoria_ascii();
        }
        else if (cmd == "produce") {
            int x; ss >> x;
            if (ss.fail()) { cout << "produce <valor>\n"; continue; }
            sync_sim.producir_simulado(x);
        }
        else if (cmd == "consume") sync_sim.consumir_simulado();
        else if (cmd == "bufstat") sync_sim.estado_buffer();
        else if (cmd == "exit") {
            cout << "Saliendo...\n";
            break;
        }

        else if (cmd == "disk") {
            string tipo; ss >> tipo;
            if (ss.fail()) { cout << "Uso: disk <fcfs|sstf|scan> <lista_de_solicitudes> [dir=up|down]\n"; continue; }

            int pos_inicial = 50; // posicion inicial del cabezal (puedes cambiarlo)
            vector<int> reqs;
            string token;

            while (ss >> token) {
                if (token.rfind("dir=", 0) == 0) break; // termina si aparece "dir="
                reqs.push_back(stoi(token));
            }

            bool hacia_derecha = true;
            if (token == "dir=down") hacia_derecha = false;

            if (reqs.empty()) {
                cout << "Debes ingresar al menos una solicitud de cilindro.\n";
                continue;
            }

            SimuladorDisco sim(reqs, pos_inicial);

            if (tipo == "fcfs") sim.ejecutar_FCFS();
            else if (tipo == "sstf") sim.ejecutar_SSTF();
            else if (tipo == "scan") sim.ejecutar_SCAN(hacia_derecha);
            else cout << "Algoritmo no reconocido. Usa fcfs, sstf o scan.\n";
        }

        else if (cmd.empty()) continue;
        else cout << "Comando no reconocido: " << cmd << "\n";
    }

    return 0;
}
