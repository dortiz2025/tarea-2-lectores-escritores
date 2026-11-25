#include "verificador.hpp"

#include <limits>
#include <unordered_map>

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza,
                                      Politica politica) {
    // En este paso solo reconstruimos el estado; las reglas de cada politica
    // se comprobaran despues.
    (void)politica;

    struct InformacionSolicitud {
        Tipo tipo;
        long turno; // Se conserva para comprobar FIFO en el siguiente paso.
        std::size_t indice_pide;
        std::size_t indice_entra;
    };

    // La clave de cada mapa es el identificador de hebra.
    std::unordered_map<int, InformacionSolicitud> solicitudes_pendientes;
    std::unordered_map<int, InformacionSolicitud> hebras_dentro;

    // Estos contadores permiten consultar el estado sin recorrer los mapas.
    int cantidad_lectoras_pendientes = 0;
    int cantidad_escritoras_pendientes = 0;
    int cantidad_lectoras_dentro = 0;
    int cantidad_escritoras_dentro = 0;

    const auto crear_error = [](std::size_t indice, const std::string& regla) {
        return ResultadoVerificacion{false, indice, regla, 0};
    };

    // Recorremos la traza en orden, como si actualizaramos el estado paso a paso.
    for (std::size_t indice = 0; indice < traza.size(); ++indice) {
        const AnotacionTraza& paso = traza[indice];

        if (paso.tipo != Tipo::Lectora && paso.tipo != Tipo::Escritora) {
            return crear_error(indice, "tipo de hebra desconocido");
        }

        switch (paso.accion) {
        case Accion::Pide: {
            // Una hebra no puede pedir otra vez si ya esta esperando o dentro.
            if (solicitudes_pendientes.find(paso.hebra) != solicitudes_pendientes.end()) {
                return crear_error(indice, "la hebra solicita mientras ya tiene una solicitud pendiente");
            }
            if (hebras_dentro.find(paso.hebra) != hebras_dentro.end()) {
                return crear_error(indice, "la hebra solicita mientras ya esta dentro del recurso");
            }

            const InformacionSolicitud informacion{
                paso.tipo, paso.turno, indice,
                std::numeric_limits<std::size_t>::max()
            };
            solicitudes_pendientes.emplace(paso.hebra, informacion);

            if (paso.tipo == Tipo::Lectora) {
                ++cantidad_lectoras_pendientes;
            } else {
                ++cantidad_escritoras_pendientes;
            }
            break;
        }

        case Accion::Entra: {
            // Para entrar, la hebra primero debe haber pedido acceso.
            const auto solicitud_encontrada = solicitudes_pendientes.find(paso.hebra);
            if (solicitud_encontrada == solicitudes_pendientes.end()) {
                return crear_error(indice, "la hebra entra sin una solicitud pendiente");
            }
            if (solicitud_encontrada->second.tipo != paso.tipo) {
                return crear_error(indice, "el tipo de Entra no coincide con el tipo de Pide");
            }
            if (hebras_dentro.find(paso.hebra) != hebras_dentro.end()) {
                return crear_error(indice, "la hebra entra mientras ya esta dentro del recurso");
            }

            // La solicitud deja de estar pendiente y pasa a estar dentro.
            InformacionSolicitud informacion_activa = solicitud_encontrada->second;
            solicitudes_pendientes.erase(solicitud_encontrada);
            informacion_activa.indice_entra = indice;
            hebras_dentro.emplace(paso.hebra, informacion_activa);

            if (paso.tipo == Tipo::Lectora) {
                --cantidad_lectoras_pendientes;
                ++cantidad_lectoras_dentro;
            } else {
                --cantidad_escritoras_pendientes;
                ++cantidad_escritoras_dentro;
            }
            break;
        }

        case Accion::Sale: {
            // Solo puede salir una hebra que aparece actualmente dentro.
            const auto hebra_encontrada = hebras_dentro.find(paso.hebra);
            if (hebra_encontrada == hebras_dentro.end()) {
                return crear_error(indice, "la hebra sale sin haber entrado");
            }
            if (hebra_encontrada->second.tipo != paso.tipo) {
                return crear_error(indice, "el tipo de Sale no coincide con el tipo de Entra");
            }

            hebras_dentro.erase(hebra_encontrada);
            if (paso.tipo == Tipo::Lectora) {
                --cantidad_lectoras_dentro;
            } else {
                --cantidad_escritoras_dentro;
            }
            break;
        }

        default:
            return crear_error(indice, "accion desconocida");
        }
    }

    // Una traza completa debe terminar sin hebras esperando ni dentro.
    if (cantidad_lectoras_pendientes != 0 || cantidad_escritoras_pendientes != 0 ||
        cantidad_lectoras_dentro != 0 || cantidad_escritoras_dentro != 0) {
        std::size_t primer_indice_incompleto = std::numeric_limits<std::size_t>::max();
        std::string mensaje_incompleto;

        for (const auto& solicitud : solicitudes_pendientes) {
            if (solicitud.second.indice_pide < primer_indice_incompleto) {
                primer_indice_incompleto = solicitud.second.indice_pide;
                mensaje_incompleto = "solicitud Pide sin una anotacion Entra";
            }
        }
        for (const auto& hebra : hebras_dentro) {
            if (hebra.second.indice_entra < primer_indice_incompleto) {
                primer_indice_incompleto = hebra.second.indice_entra;
                mensaje_incompleto = "anotacion Entra sin una anotacion Sale";
            }
        }

        return crear_error(primer_indice_incompleto, mensaje_incompleto);
    }

    return ResultadoVerificacion{true, 0, "", 0};
}
