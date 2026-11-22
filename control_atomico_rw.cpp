#include "control_atomico_rw.hpp"

#include <thread>

// STUB TEMPORAL (Semana 1) - Reemplazar por implementación completa de R8, R9 y R10 (Integrante 4)

ControlAtomicoRW::ControlAtomicoRW(int num_hebras)
    : contadores_por_hebra(static_cast<std::size_t>(num_hebras)) {}

void ControlAtomicoRW::entrar_lectura() {
    std::uint64_t esperado = 0;
    while (!estado.compare_exchange_weak(esperado, BIT_ESC_ACTIVA)) {
        esperado = 0;
        std::this_thread::yield();
    }
}

void ControlAtomicoRW::salir_lectura() {
    estado.store(0);
}

void ControlAtomicoRW::entrar_escritura() {
    std::uint64_t esperado = 0;
    while (!estado.compare_exchange_weak(esperado, BIT_ESC_ACTIVA)) {
        esperado = 0;
        std::this_thread::yield();
    }
}

void ControlAtomicoRW::salir_escritura() {
    estado.store(0);
}

std::optional<MetricasAtomicas> ControlAtomicoRW::obtener_metricas_atomicas() const {
    MetricasAtomicas total{};
    for (const auto& c : contadores_por_hebra) {
        total.recurso_ocupado += c.recurso_ocupado;
        total.escritoras_esperando += c.escritoras_esperando;
        total.cas_fallido += c.cas_fallido;
    }
    return total;
}