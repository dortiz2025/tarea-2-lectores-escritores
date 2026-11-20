#ifndef CONTROL_ATOMICO_RW_HPP
#define CONTROL_ATOMICO_RW_HPP

#include "control_rw.hpp"

#include <atomic>
#include <cstdint>
#include <vector>

class ControlAtomicoRW : public ControlRW {
public:
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
                  "std::atomic<std::uint64_t> debe ser lock-free");

    // Formato de la palabra empaquetada de 64 bits (R9):
    //   Bits [0..30]  (31 bits): lectoras_activas     (máx. 2^31 - 1 >> 1000)
    //   Bit  [31]     ( 1 bit ): escritora_activa     (0 o 1)
    //   Bits [32..62] (31 bits): escritoras_esperando (máx. 2^31 - 1 >> 1000)
    //   Bit  [63]     ( 1 bit ): reservado (0)
    // Con L + E <= 1000 (R1), ningún campo puede desbordarse.
    static constexpr std::uint64_t MASK_LECTORAS    = 0x000000007FFFFFFFULL;
    static constexpr std::uint64_t BIT_ESC_ACTIVA   = (1ULL << 31);
    static constexpr std::uint64_t SHIFT_ESC_ESP    = 32;
    static constexpr std::uint64_t MASK_ESC_ESP     = (0x7FFFFFFFULL << SHIFT_ESC_ESP);
    static constexpr std::uint64_t UNO_LECTORA      = 1ULL;
    static constexpr std::uint64_t UNO_ESC_ESP      = (1ULL << SHIFT_ESC_ESP);

    explicit ControlAtomicoRW(int num_hebras);

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

    // Suma los contadores locales por hebra; invocar después de los join() (R10)
    std::optional<MetricasAtomicas> obtener_metricas_atomicas() const override;

private:
    // Alineado a línea de caché para evitar false sharing entre hebras en espera activa
    struct alignas(64) ContadoresHebra {
        std::uint64_t recurso_ocupado = 0;
        std::uint64_t escritoras_esperando = 0;
        std::uint64_t cas_fallido = 0;
    };

    std::atomic<std::uint64_t> estado{0};
    std::vector<ContadoresHebra> contadores_por_hebra;
};

#endif // CONTROL_ATOMICO_RW_HPP