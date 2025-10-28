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

using namespace std;

struct Proceso {
    int id_proceso;
    int tiempo_requerido;
    int tiempo_restante;
    int tiempo_llegada;
    int tiempo_inicio;
    int tiempo_finalizacion;
    string estado;

    Proceso(int id, int tiempo, int llegada): 
        id_proceso(id),
        tiempo_requerido(tiempo),
        tiempo_restante(tiempo),
        tiempo_llegada(llegada),
        tiempo_inicio(-1),
        tiempo_finalizacion(-1),
        estado("LISTO") {}
};


class PlanificadorRR {
private:
    // Cola de procesos listos para ejecutar
    queue<shared_ptr<Proceso>> cola_listos;
    // Mapa con todos los procesos
    map<int, shared_ptr<Proceso>> procesos;
    // Tiempo asignado a cada proceso
    int tiempo_ejecucion;
    // Tiempo global del sistema
    int tiempo_actual;
    // Control del siguiente ID a asignar
    int sig_id_proceso;

public:
    PlanificadorRR(int tiempo_ejecucion) : tiempo_ejecucion(tiempo_ejecucion), tiempo_actual(0), sig_id_proceso(1) {}

    int crear_proceso(int tiempo_requerido) {
        // Crear un nuevo proceso y agregarlo a la cola de listos
        auto p = make_shared<Proceso>(sig_id_proceso++, tiempo_requerido, tiempo_actual);
        cola_listos.push(p);
        procesos[p->id_proceso] = p;
        cout << "[+] Proceso creado | id_proceso=" << p ->id_proceso
                  << " con tiempo_requerido=" << tiempo_requerido << "\n";
        return p->id_proceso;
    }

    // Simular la ejecucion del planificador de procesos por N pasos
    void ejecutar(int pasos) {
        for (int i = 0; i < pasos; i++) {
            if (cola_listos.empty()) {
                tiempo_actual++;
                continue;
            }
            auto p = cola_listos.front();
            cola_listos.pop();
            if (p->estado == "TERMINADO")
                continue;

            p->estado = "EJECUTANDO";
            if (p->tiempo_inicio == -1)
                p->tiempo_inicio = tiempo_actual;

            int trabajo = min(tiempo_ejecucion, p->tiempo_restante);
            tiempo_actual += trabajo;
            p->tiempo_restante -= trabajo;

            if (p->tiempo_restante <= 0) {
                p->estado = "TERMINADO";
                p->tiempo_finalizacion = tiempo_actual;
                cout << "[RR] id_proceso " << p->id_proceso << " finalizado en: " << tiempo_actual << "\n";
            } else {
                p->estado = "LISTO";
                cola_listos.push(p);
            }
        }
    }

    // Listar todos los procesos
    void listar_procesos() {
        cout << "id_proceso\tEstado\tRestante\tInicio\tFin\n";
        for (auto &entrada : procesos) {
            auto p = entrada.second;
            cout << p->id_proceso << "\t" << p->estado << "\t" << p->tiempo_restante
                      << "\t\t" << p->tiempo_inicio << "\t" << p->tiempo_finalizacion << "\n";
        }
    }

    // Mostrar metricas de rendimiento
    void estatisticas() {
        double espera_total = 0, ejecucion_total = 0;
        int finalizados = 0;

        for (auto &entrada : procesos) {
            auto p = entrada.second;
            if (p->tiempo_finalizacion == -1)
                continue;

            int tiempo_retorno = p->tiempo_finalizacion - p->tiempo_llegada;
            int tiempo_espera = tiempo_retorno - p->tiempo_requerido;

            espera_total += tiempo_espera;
            ejecucion_total += tiempo_retorno;
            finalizados++;
        }
        if (finalizados == 0) {
            cout << "Aun no hay procesos finalizados.\n";
            return;
        }
        cout << "Promedio tiempo_espera Time: " << espera_total / finalizados
                  << " | Promedio tiempo_retorno: " << ejecucion_total / finalizados << "\n";
    }

    void terminar(int id_proceso) {
        if (procesos.count(id_proceso)) {
            procesos[id_proceso]->estado = "TERMINADO";
            procesos[id_proceso]->tiempo_finalizacion = tiempo_actual;
            cout << "[X] Proceso " << id_proceso << " terminado manualmente.\n";
        } else {
            cout << "[!] No existe un proceso con ID " << id_proceso << endl;
        }
    }

    // Obtener un proceso en especifico por ID
    shared_ptr<Proceso> obtener_proceso(int id_proceso) {
        return procesos.count(id_proceso) ? procesos[id_proceso] : nullptr;
    }
};

// Memoria Virtual (FIFO)
class MemoriaVirtual {
private:
    int max_frames;
    deque<pair<int,int>> fifo; // (id_proceso,page)
    map<pair<int,int>, int> lru_cache; // (id_proceso,page) -> timestamp
    int tick;
    int faults;
    bool useLRU;

public:
    MemoriaVirtual(int frames=3, bool lru=false)
        : max_frames(frames), tick(0), faults(0), useLRU(lru) {}

    void access_page(int id_proceso, int page) {
        tick++;
        pair<int,int> key = {id_proceso,page};
        if (useLRU) {
            // LRU policy
            if (lru_cache.count(key)) {
                lru_cache[key] = tick; // update recent use
                cout << "[LRU] HIT P("<<id_proceso<<","<<page<<")\n";
                return;
            }
            faults++;
            cout << "[LRU] MISS P("<<id_proceso<<","<<page<<")\n";
            if ((int)lru_cache.size() >= max_frames) {
                // find least recently used
                auto victim = min_element(lru_cache.begin(), lru_cache.end(),
                    [](auto &a, auto &b){ return a.second < b.second; });
                cout << "   Reemplaza página P("<<victim->first.first<<","<<victim->first.second<<")\n";
                lru_cache.erase(victim);
            }
            lru_cache[key] = tick;
        } else {
            // FIFO policy
            for (auto &p : fifo)
                if (p == key) {
                    cout << "[FIFO] HIT P("<<id_proceso<<","<<page<<")\n";
                    return;
                }
            faults++;
            cout << "[FIFO] MISS P("<<id_proceso<<","<<page<<")\n";
            if ((int)fifo.size() >= max_frames) {
                cout << "   Reemplaza página P("<<fifo.front().first<<","<<fifo.front().second<<")\n";
                fifo.pop_front();
            }
            fifo.push_back(key);
        }
        show_visual();
    }

    void show_status() {
        cout << "\nFrames actuales:\n";
        if (useLRU) {
            for (auto &p : lru_cache)
                cout << " P("<<p.first.first<<","<<p.first.second<<") ";
        } else {
            for (auto &p : fifo)
                cout << " P("<<p.first<<","<<p.second<<") ";
        }
        cout << "\nFallos totales: " << faults
             << " | Tasa de fallos: "
             << (tick > 0 ? (float)faults/tick : 0) << "\n";
    }
    
    void show_visual() {
        cout << "\n Estado actual de los marcos de memoria:\n";
        cout << "---------------------------------\n";
        if (useLRU) {
            if (lru_cache.empty()) cout << "| (vacio)                    |\n";
            else {
                for (auto &p : lru_cache) {
                    cout << "| PID " << p.first.first << " -> Pagina " << p.first.second << " |\n";
                }
            }
        } else {
            if (fifo.empty()) cout << "| (vacio)                    |\n";
            else {
                for (auto &p : fifo) {
                    cout << "│ PID " << p.first << " -> Pagina " << p.second << " |\n";
                }
            }
        }
        cout << "---------------------------------\n";
        cout << "Fallos totales: " << faults
             << " | Tasa de fallos: " << (tick > 0 ? (float)faults / tick : 0)
             << "\n";
    }

};

class ProductorConsumidor {
private:
    queue<int> buffer;
    const int MAX_SIZE = 5;
    mutex mtx;
    condition_variable not_full, not_empty;

public:
    void producir(int item) {
        unique_lock<mutex> lock(mtx);
        not_full.wait(lock, [&]{ return buffer.size() < MAX_SIZE; });
        buffer.push(item);
        cout << "Producido: " << item << " (en buffer="<<buffer.size()<<")\n";
        not_empty.notify_one();
    }

    void consumir() {
        unique_lock<mutex> lock(mtx);
        not_empty.wait(lock, [&]{ return !buffer.empty(); });
        int item = buffer.front();
        buffer.pop();
        cout << "Consumido: " << item << " (restan="<<buffer.size()<<")\n";
        not_full.notify_one();
    }

    void stat() {
        cout << "Elementos actuales en buffer: " << buffer.size() << "\n";
    }
};



int main() {
    PlanificadorRR planificador(2);         
    MemoriaVirtual memoria(3, false);       
    ProductorConsumidor sync;               

    cout << "=== SIMULADOR DE KERNEL - ENTREGA 2 ===\n";
    cout << "Comandos disponibles:\n"
         << "  new <tiempo>         crea un nuevo proceso\n"
         << "  run <ticks>          ejecuta el planificador por N ticks\n"
         << "  ps                   lista procesos actuales\n"
         << "  stats                muestra metricas promedio\n"
         << "  kill <id>            finaliza un proceso manualmente\n"
         << "  mem <id> <pag>       accede a una pagina (FIFO/LRU)\n"
         << "  memmode <fifo|lru>   cambia la politica de memoria\n"
         << "  produce <x>          produce un item (sincronizacion)\n"
         << "  consume              consume un item\n"
         << "  bufstat              muestra estado del buffer compartido\n"
         << "  exit                 salir del simulador\n";

    string cmd;
    while (true) {
        cout << "\n> ";
        getline(cin, cmd);
        stringstream ss(cmd);
        string op;
        ss >> op;

        // ------------------- Gestion de procesos -------------------
        if (op == "new") {
            int tiempo;
            ss >> tiempo;
            planificador.crear_proceso(tiempo);
        } 
        else if (op == "run") {
            int ticks;
            ss >> ticks;
            planificador.ejecutar(ticks);
        } 
        else if (op == "ps") planificador.listar_procesos();
        else if (op == "stats") planificador.estatisticas();
        else if (op == "kill") { int id; ss >> id; planificador.terminar(id); }

        // ------------------- Memoria virtual -------------------
        else if (op == "mem") {
            int id, page;
            ss >> id >> page;
            memoria.access_page(id, page);
        } 
        else if (op == "memmode") {
            string mode;
            ss >> mode;
            if (mode == "lru") memoria = MemoriaVirtual(3, true);
            else memoria = MemoriaVirtual(3, false);
            cout << "[!] Política de memoria cambiada a " << mode << "\n";
        } 
        else if (op == "memstat") memoria.show_status();

        // ------------------- Sincronizacion -------------------
        else if (op == "produce") { 
            int item; 
            ss >> item; 
            if (ss.fail()) {
                cout << "Uso: produce <valor>\n";
            } else {
                thread t(&ProductorConsumidor::producir, &sync, item);
                t.detach();
            }
        }
        else if (op == "consume") { 
            thread t(&ProductorConsumidor::consumir, &sync);
            t.detach(); 
        }
        else if (op == "bufstat") sync.stat();

        // ------------------- Salida -------------------
        else if (op == "exit") {
            cout << "Saliendo del simulador...\n";
            break;
        } 
        else if (op.empty()) continue;
        else cout << "Comando no reconocido.\n";
    }

    return 0;
}
