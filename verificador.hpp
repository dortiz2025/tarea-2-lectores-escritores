#ifndef VERIFICADOR_HPP
#define VERIFICADOR_HPP

#include "monitor_rw.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ResultadoVerificacion {
    bool politica_ok = true;
    std::size_t indice_violacion = 0;  // índice de la anotación donde ocurre la falla
    std::string regla_incumplida;      // descripción de la regla violada (si !politica_ok)
    std::uint64_t evidencia = 0;       // indicador de evidencia según política (R6)
};

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza,
                                      Politica politica);

#endif // VERIFICADOR_HPP