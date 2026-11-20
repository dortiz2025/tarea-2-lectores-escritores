#ifndef CONTROL_RW_HPP
#define CONTROL_RW_HPP

#include <cstdint>
#include <optional>

// Identificador lógico de la hebra (0 .. L + E - 1), definido en rw.cpp
extern thread_local int id_hebra;

// Contadores agregados de intentos fallidos para la versión atómica (R10)
struct MetricasAtomicas {
    std::uint64_t recurso_ocupado = 0;
    std::uint64_t escritoras_esperando = 0;
    std::uint64_t cas_fallido = 0;
};

class ControlRW {
public:
    virtual ~ControlRW() = default;
    virtual void entrar_lectura() = 0;
    virtual void salir_lectura() = 0;
    virtual void entrar_escritura() = 0;
    virtual void salir_escritura() = 0;

    // Por defecto no disponible; sobrescrito únicamente por ControlAtomicoRW (R10)
    virtual std::optional<MetricasAtomicas> obtener_metricas_atomicas() const {
        return std::nullopt;
    }
};

#endif // CONTROL_RW_HPP