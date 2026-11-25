#include "verificador.hpp"

#include <limits>
#include <unordered_map>

ResultadoVerificacion verificar_traza(const std::vector<AnotacionTraza>& traza,
                                      Politica politica) {
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
    std::uint64_t evidencia = 0;
    std::uint64_t siguiente_turno_solicitud = 0;
    std::uint64_t siguiente_turno_entrada = 0;

    const auto crear_error = [&evidencia](std::size_t indice, const std::string& regla) {
        return ResultadoVerificacion{false, indice, regla, evidencia};
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

            // En FIFO cada solicitud recibe el siguiente turno disponible.
            if (politica == Politica::Fifo &&
                (paso.turno < 0 ||
                 static_cast<std::uint64_t>(paso.turno) != siguiente_turno_solicitud)) {
                return crear_error(indice, "el turno de Pide no es consecutivo en FIFO");
            }

            // Esta evidencia cuenta lectoras que pidieron mientras una
            // escritora esperaba bajo la prioridad de escritores.
            if (politica == Politica::Escritores &&
                paso.tipo == Tipo::Lectora &&
                cantidad_escritoras_pendientes > 0) {
                ++evidencia;
            }

            // Con prioridad de lectores, una lectora entra de inmediato si
            // no hay una escritora activa: su Entra debe ser el paso siguiente.
            if (politica == Politica::Lectores &&
                paso.tipo == Tipo::Lectora &&
                cantidad_escritoras_dentro == 0) {
                const bool siguiente_es_su_entrada =
                    indice + 1 < traza.size() &&
                    traza[indice + 1].hebra == paso.hebra &&
                    traza[indice + 1].tipo == Tipo::Lectora &&
                    traza[indice + 1].accion == Accion::Entra;

                if (!siguiente_es_su_entrada) {
                    return crear_error(
                        indice,
                        "una lectora pidio sin escritora activa y la anotacion siguiente no fue su Entra"
                    );
                }
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
            if (politica == Politica::Fifo) {
                ++siguiente_turno_solicitud;
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

            // Una escritora necesita el recurso para ella sola.
            if (paso.tipo == Tipo::Escritora &&
                (cantidad_lectoras_dentro > 0 || cantidad_escritoras_dentro > 0)) {
                return crear_error(
                    indice,
                    "una escritora entra mientras hay otra hebra activa"
                );
            }

            // Una escritora no puede adelantarse a lectoras que ya esperan.
            if (politica == Politica::Lectores &&
                paso.tipo == Tipo::Escritora &&
                cantidad_lectoras_pendientes > 0) {
                return crear_error(
                    indice,
                    "una escritora entra mientras hay lectoras pendientes"
                );
            }

            // Con prioridad de escritores, las lectoras esperan mientras
            // haya al menos una escritora pendiente.
            if (politica == Politica::Escritores &&
                paso.tipo == Tipo::Lectora &&
                cantidad_escritoras_pendientes > 0) {
                return crear_error(
                    indice,
                    "una lectora entra mientras hay escritoras pendientes"
                );
            }

            // En FIFO, el turno de entrada debe coincidir con el turno que
            // recibio la hebra al pedir acceso y con el siguiente esperado.
            if (politica == Politica::Fifo) {
                if (solicitud_encontrada->second.turno != paso.turno) {
                    return crear_error(indice, "el turno de Entra no coincide con su Pide");
                }
                if (paso.turno < 0 ||
                    static_cast<std::uint64_t>(paso.turno) != siguiente_turno_entrada) {
                    return crear_error(indice, "las entradas no respetan el orden FIFO");
                }
            }

            // Una lectora puede compartir el recurso con otras lectoras,
            // pero no puede coincidir con una escritora.
            if (paso.tipo == Tipo::Lectora && cantidad_escritoras_dentro > 0) {
                return crear_error(
                    indice,
                    "una lectora entra mientras hay una escritora activa"
                );
            }

            // Evidencia de que la prioridad de lectores fue puesta a prueba:
            // la lectora entra aunque ya haya escritoras esperando.
            if (politica == Politica::Lectores &&
                paso.tipo == Tipo::Lectora &&
                cantidad_escritoras_pendientes > 0) {
                ++evidencia;
            }

            // En FIFO, una lectora entra concurrentemente con otra si ya
            // habia al menos una lectora dentro del recurso.
            if (politica == Politica::Fifo &&
                paso.tipo == Tipo::Lectora &&
                cantidad_lectoras_dentro > 0) {
                ++evidencia;
            }

            if (politica == Politica::Fifo) {
                ++siguiente_turno_entrada;
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

    return ResultadoVerificacion{true, 0, "", evidencia};
}
