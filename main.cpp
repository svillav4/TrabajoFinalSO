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

class GestorMarcos {
private:
    vector<Frame> marcos;
    deque<int> cola_fifo; // índices de marcos para FIFO
    int max_marcos;
    int reloj_tick;
    int accesos_totales;
    int fallos_totales;
    vector<AccesoRegistro> trazas;
public:
    // tabla de páginas: pid -> (pagina -> marco index)
    map<int, map<int,int>> tabla_paginas;

    GestorMarcos(int n_marcos = 3) : max_marcos(n_marcos), reloj_tick(0), accesos_totales(0), fallos_totales(0) {
        marcos.resize(max_marcos);
        for (int i = 0; i < max_marcos; ++i) {
            marcos[i] = Frame();
            cola_fifo.push_back(i);
        }
    }

    // Acceso a página: devuelve tiempo en ticks que costó el acceso y setea hit true/false
    int acceder_pagina(int pid, int pagina, bool usar_lru, bool &hit) {
        reloj_tick++;
        accesos_totales++;
        // buscar en tabla paginas del pid
        if (tabla_paginas.count(pid) && tabla_paginas[pid].count(pagina)) {
            int marco = tabla_paginas[pid][pagina];
            if (marco >= 0 && marcos[marco].pid == pid && marcos[marco].pagina == pagina) {
                // HIT
                hit = true;
                marcos[marco].last_used = reloj_tick;
                int t = 1; // tiempo de acceso hit (simulado)
                trazas.push_back({reloj_tick, pid, pagina, true, t});
                return t;
            }
        }
        // MISS: hay que traer página
        hit = false;
        fallos_totales++;
        int tiempo_miss = 10; // coste de miss (simulado)
        // buscar marco libre
        int marco_libre = -1;
        for (int i = 0; i < max_marcos; ++i) {
            if (marcos[i].pid == -1) { marco_libre = i; break; }
        }
        int elegido = -1;
        if (marco_libre != -1) {
            elegido = marco_libre;
        } else {
            // elegir víctima segun politica: FIFO o LRU
            if (!usar_lru) {
                // FIFO: el frente de cola_fifo contiene índice más viejo ocupado
                while (!cola_fifo.empty()) {
                    int c = cola_fifo.front();
                    // aceptamos cualquier marco para reemplazo
                    cola_fifo.pop_front();
                    elegido = c;
                    cola_fifo.push_back(elegido); // mover al final (nuevo propietario)
                    break;
                }
            } else {
                // LRU: buscar marco con menor last_used
                int min_used = INT_MAX;
                for (int i = 0; i < max_marcos; ++i) {
                    if (marcos[i].last_used < min_used) {
                        min_used = marcos[i].last_used;
                        elegido = i;
                    }
                }
            }
            // limpiar entrada de tabla del proceso que perdió el marco
            if (elegido != -1) {
                int pid_victima = marcos[elegido].pid;
                int pag_victima = marcos[elegido].pagina;
                if (pid_victima != -1) {
                    if (tabla_paginas.count(pid_victima)) {
                        tabla_paginas[pid_victima].erase(pag_victima);
                    }
                }
            }
        }
        // asignar
        if (elegido == -1) {
            // fallback (no debería ocurrir)
            elegido = 0;
        }
        marcos[elegido].pid = pid;
        marcos[elegido].pagina = pagina;
        marcos[elegido].last_used = reloj_tick;
        tabla_paginas[pid][pagina] = elegido;

        trazas.push_back({reloj_tick, pid, pagina, false, tiempo_miss});
        return tiempo_miss;
    }

    void mostrar_estado() const {
        cout << "\n[Memoria] Estado de marcos (index: PID->Pagina, last_used)\n";
        for (int i = 0; i < max_marcos; ++i) {
            cout << " Marco[" << setw(2) << i << "]: ";
            if (marcos[i].pid == -1) cout << "(libre)\n";
            else cout << "PID " << marcos[i].pid << " -> Pag " << marcos[i].pagina
                      << " (last=" << marcos[i].last_used << ")\n";
        }
        cout << "Accesos totales: " << accesos_totales << " | Fallos totales: " << fallos_totales
             << " | Tasa fallos: " << (accesos_totales>0 ? (double)fallos_totales/accesos_totales : 0.0) << "\n";
    }

    void mostrar_trazas(int ultimos = 20) const {
        cout << "\n[Memoria] Ultimas trazas de acceso (tick pid pag HIT tiempo):\n";
        int inicio = max(0, (int)trazas.size() - ultimos);
        for (int i = inicio; i < (int)trazas.size(); ++i) {
            const auto &r = trazas[i];
            cout << " " << r.tick << " | PID " << r.pid << " | Pag " << r.pagina
                 << " | " << (r.hit ? "HIT " : "MISS") << " | t=" << r.tiempo_acceso_simulado << "\n";
        }
    }

    // estadísticas por proceso
    pair<int,int> estadisticas_proceso(int pid) const {
        // devuelve (accesos, fallos) estimados contando trazas
        int a = 0, f = 0;
        for (auto &r : trazas) {
            if (r.pid == pid) {
                a++;
                if (!r.hit) f++;
            }
        }
        return {a,f};
    }

    void liberar_proceso(int pid) {
        // cuando un proceso termina se deben liberar los marcos que ocupaba
        for (int i = 0; i < max_marcos; ++i) {
            if (marcos[i].pid == pid) {
                marcos[i] = Frame();
            }
        }
        tabla_paginas.erase(pid);
    }
};

// ------------------------- Productor-Consumidor simulado -------------------------
class ProductorConsumidorSimulado {
private:
    deque<int> buffer;
    size_t capacidad;
    SemaforoSimulado sem_vacio; // cuenta de vacíos (capacidad)
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
            // en el modelo simulado no usamos valor interno de sem_vacio aquí
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

// ------------------------- Planificador Round Robin con tick -------------------------
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
            cout << "[+] Proceso creado | id=" << p->id_proceso << " tiempo=" << tiempo_requerido << " (LISTO)\n";
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

    // Ejecutar un tick (1 unidad de tiempo). Gestiona selección, preempción y métricas.
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

// ------------------------- CLI y main -------------------------
int main() {
    PlanificadorRR planificador(2);         // quantum = 2
    GestorMarcos gestor(3);                 // 3 marcos por defecto
    bool modo_lru = false;
    ProductorConsumidorSimulado sync_sim(5);

    cout << "=== SIMULADOR DE KERNEL ===\n";
    cout << "Comandos:\n"
         << "  new <tiempo> [llegada_offset]   -> crea proceso\n"
         << "  run <ticks>                      -> ejecuta planificador N ticks\n"
         << "  tick                             -> avanza 1 tick\n"
         << "  ps                               -> lista procesos\n"
         << "  stats                            -> metricas generales\n"
         << "  kill <id>                        -> termina proceso manualmente\n"
         << "  mem <pid> <pag>                  -> acceder pagina (usa GestorMarcos)\n"
         << "  memmode <fifo|lru> [marcos]      -> cambia politica y opcional marcos\n"
         << "  memstat                          -> mostrar estado memoria\n"
         << "  memtrace                          -> mostrar trazas memoria\n"
         << "  memstats <pid>                   -> mostrar stats de un proceso (accesos/fallos)\n"
         << "  sem_signal <name>                -> (simulado) signal en semaforo predef\n"
         << "  produce <x>                      -> producir item en buffer (simulado)\n"
         << "  consume                          -> consumir item del buffer (simulado)\n"
         << "  bufstat                          -> estado buffer sincronizacion\n"
         << "  exit\n";

    string linea;
    while (true) {
        cout << "\n> ";
        if (!getline(cin, linea)) break;
        stringstream ss(linea);
        string cmd;
        ss >> cmd;
        if (cmd == "new") {
            int t; int off = 0;
            ss >> t;
            if (ss.fail()) { cout << "Uso: new <tiempo> [llegada_offset]\n"; continue; }
            if (!(ss >> off)) off = 0;
            planificador.crear_proceso(t, off);
        } else if (cmd == "run") {
            int n; ss >> n;
            if (ss.fail()) { cout << "run <ticks>\n"; continue; }
            planificador.ejecutar_ticks(n, gestor, modo_lru);
        } else if (cmd == "tick") {
            planificador.tick(gestor, modo_lru);
        } else if (cmd == "ps") {
            planificador.listar_procesos();
        } else if (cmd == "stats") {
            planificador.estadisticas_generales();
        } else if (cmd == "kill") {
            int id; ss >> id;
            if (ss.fail()) { cout << "kill <id>\n"; continue; }
            planificador.terminar_proceso(id);
            gestor.liberar_proceso(id);
        } else if (cmd == "mem") {
            int pid, pag;
            ss >> pid >> pag;
            if (ss.fail()) { cout << "mem <pid> <pag>\n"; continue; }
            // validar existencia proceso
            auto p = planificador.obtener_proceso(pid);
            if (!p) { cout << "[!] No existe proceso " << pid << "\n"; continue; }
            bool hit=false;
            int tiempo_acceso = gestor.acceder_pagina(pid, pag, modo_lru, hit);
            cout << (hit ? "[MEM] HIT " : "[MEM] MISS ") << " PID " << pid << " Pag " << pag
                 << " (tiempo_sim=" << tiempo_acceso << " ticks)\n";
        } else if (cmd == "memmode") {
            string modo; int marcos;
            ss >> modo;
            if (ss.fail()) { cout << "memmode <fifo|lru> [marcos]\n"; continue; }
            if (ss >> marcos) {
                gestor = GestorMarcos(marcos);
                cout << "[!] Reiniciado gestor de marcos con " << marcos << " marcos\n";
            }
            if (modo == "lru") modo_lru = true;
            else modo_lru = false;
            cout << "[!] Politica memoria: " << (modo_lru ? "LRU" : "FIFO") << "\n";
        } else if (cmd == "memstat") {
            gestor.mostrar_estado();
        } else if (cmd == "memtrace") {
            gestor.mostrar_trazas();
        } else if (cmd == "memstats") {
            int pid; ss >> pid;
            if (ss.fail()) { cout << "memstats <pid>\n"; continue; }
            auto p = planificador.obtener_proceso(pid);
            if (!p) { cout << "[!] No existe proceso " << pid << "\n"; continue; }
            auto stats = gestor.estadisticas_proceso(pid);
            cout << "[MEM-STATS] PID " << pid << " -> Accesos: " << stats.first << " | Fallos: " << stats.second
                 << " | Tasa: " << (stats.first>0 ? (double)stats.second/stats.first : 0.0) << "\n";
        } else if (cmd == "produce") {
            int x; ss >> x;
            if (ss.fail()) { cout << "produce <valor>\n"; continue; }
            sync_sim.producir_simulado(x);
        } else if (cmd == "consume") {
            sync_sim.consumir_simulado();
        } else if (cmd == "bufstat") {
            sync_sim.estado_buffer();
        } else if (cmd == "exit") {
            cout << "Saliendo...\n";
            break;
        } else if (cmd.empty()) {
            continue;
        } else {
            cout << "Comando no reconocido: " << cmd << "\n";
        }
    }

    return 0;
}
