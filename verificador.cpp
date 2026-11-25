#include "verificador.hpp"

#include <limits>
#include <unordered_map>

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza,
                                      Politica politica) {
    (void)politica;

    struct Solicitud {
        Tipo tipo;
        long turno;
        std::size_t indice_pide;
        std::size_t indice_entra;
    };

    std::unordered_map<int, Solicitud> pendientes;
    std::unordered_map<int, Solicitud> activas;

    int lectoras_pendientes = 0;
    int escritoras_pendientes = 0;
    int lectoras_activas = 0;
    int escritoras_activas = 0;

    const auto violacion = [](std::size_t indice, const std::string& regla) {
        return ResultadoVerificacion{false, indice, regla, 0};
    };

    for (std::size_t i = 0; i < traza.size(); ++i) {
        const AnotacionTraza& anotacion = traza[i];

        if (anotacion.tipo != Tipo::Lectora &&
            anotacion.tipo != Tipo::Escritora) {
            return violacion(i, "tipo de hebra desconocido");
        }

        switch (anotacion.accion) {
        case Accion::Pide: {
            if (pendientes.find(anotacion.hebra) != pendientes.end()) {
                return violacion(i, "la hebra solicita mientras ya tiene una solicitud pendiente");
            }
            if (activas.find(anotacion.hebra) != activas.end()) {
                return violacion(i, "la hebra solicita mientras ya esta dentro del recurso");
            }

            const Solicitud solicitud{
                anotacion.tipo, anotacion.turno, i,
                std::numeric_limits<std::size_t>::max()
            };
            pendientes.emplace(anotacion.hebra, solicitud);

            if (anotacion.tipo == Tipo::Lectora) {
                ++lectoras_pendientes;
            } else {
                ++escritoras_pendientes;
            }
            break;
        }

        case Accion::Entra: {
            const auto solicitud = pendientes.find(anotacion.hebra);
            if (solicitud == pendientes.end()) {
                return violacion(i, "la hebra entra sin una solicitud pendiente");
            }
            if (solicitud->second.tipo != anotacion.tipo) {
                return violacion(i, "el tipo de Entra no coincide con el tipo de Pide");
            }
            if (activas.find(anotacion.hebra) != activas.end()) {
                return violacion(i, "la hebra entra mientras ya esta dentro del recurso");
            }

            Solicitud activa = solicitud->second;
            pendientes.erase(solicitud);
            activa.indice_entra = i;
            activas.emplace(anotacion.hebra, activa);

            if (anotacion.tipo == Tipo::Lectora) {
                --lectoras_pendientes;
                ++lectoras_activas;
            } else {
                --escritoras_pendientes;
                ++escritoras_activas;
            }
            break;
        }

        case Accion::Sale: {
            const auto activa = activas.find(anotacion.hebra);
            if (activa == activas.end()) {
                return violacion(i, "la hebra sale sin haber entrado");
            }
            if (activa->second.tipo != anotacion.tipo) {
                return violacion(i, "el tipo de Sale no coincide con el tipo de Entra");
            }

            activas.erase(activa);
            if (anotacion.tipo == Tipo::Lectora) {
                --lectoras_activas;
            } else {
                --escritoras_activas;
            }
            break;
        }

        default:
            return violacion(i, "accion desconocida");
        }
    }

    if (lectoras_pendientes != 0 || escritoras_pendientes != 0 ||
        lectoras_activas != 0 || escritoras_activas != 0) {
        std::size_t indice_incompleto = std::numeric_limits<std::size_t>::max();
        std::string regla_incompleta;

        for (const auto& entrada : pendientes) {
            if (entrada.second.indice_pide < indice_incompleto) {
                indice_incompleto = entrada.second.indice_pide;
                regla_incompleta = "solicitud Pide sin una anotacion Entra";
            }
        }
        for (const auto& entrada : activas) {
            if (entrada.second.indice_entra < indice_incompleto) {
                indice_incompleto = entrada.second.indice_entra;
                regla_incompleta = "anotacion Entra sin una anotacion Sale";
            }
        }

        return violacion(indice_incompleto, regla_incompleta);
    }

    return ResultadoVerificacion{true, 0, "", 0};
}
