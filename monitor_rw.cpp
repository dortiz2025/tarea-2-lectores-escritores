#include "monitor_rw.hpp"

// STUB TEMPORAL (Semana 1) - Reemplazar por implementación completa de R3, R4, R5 y R6 (Integrante 2)

MonitorRW::MonitorRW(Politica p, bool registrar_traza)
    : politica(p), registrar_traza(registrar_traza) {}

void MonitorRW::anotar(Tipo tipo, Accion accion, long turno) {
    if (registrar_traza) {
        traza.push_back(AnotacionTraza{id_hebra, tipo, accion, turno});
    }
}

void MonitorRW::entrar_lectura() {
    std::unique_lock<std::mutex> lock(m);
    anotar(Tipo::Lectora, Accion::Pide, -1);
    cv_lectoras.wait(lock, [this]() { return !escritora_activa; });
    escritora_activa = true; // candado dummy temporal
    anotar(Tipo::Lectora, Accion::Entra, -1);
}

void MonitorRW::salir_lectura() {
    std::unique_lock<std::mutex> lock(m);
    escritora_activa = false;
    anotar(Tipo::Lectora, Accion::Sale, -1);
    cv_lectoras.notify_all();
    cv_escritoras.notify_all();
}

void MonitorRW::entrar_escritura() {
    std::unique_lock<std::mutex> lock(m);
    anotar(Tipo::Escritora, Accion::Pide, -1);
    cv_escritoras.wait(lock, [this]() { return !escritora_activa; });
    escritora_activa = true; // candado dummy temporal
    anotar(Tipo::Escritora, Accion::Entra, -1);
}

void MonitorRW::salir_escritura() {
    std::unique_lock<std::mutex> lock(m);
    escritora_activa = false;
    anotar(Tipo::Escritora, Accion::Sale, -1);
    cv_lectoras.notify_all();
    cv_escritoras.notify_all();
}

std::vector<AnotacionTraza> MonitorRW::obtener_traza() {
    std::unique_lock<std::mutex> lock(m);
    return traza;
}