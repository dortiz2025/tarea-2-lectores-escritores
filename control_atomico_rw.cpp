#include "control_atomico_rw.hpp"

//constructor de la clase heredado de cintrol_rw
ControlAtomicoRW::ControlAtomicoRW(int num_hebras)
    : contadores_por_hebra(){
}

void ControlAtomicoRW::entrar_lectura() {
    // Implementación de la entrada de lectura
    while (true) {
        uint64_t estado_actual = estado.load();
        bool escritor_activo = (estado_actual & BIT_ESC_ACTIVA) != 0; 

        uint64_t escritores_esperando = (estado_actual & MASK_ESC_ESP) >> SHIFT_ESC_ESP;

        if (escritor_activo) {
            contadores_por_hebra[id_hebra].recurso_ocupado++;
            continue;
        }

        if (escritores_esperando > 0) {
            contadores_por_hebra[id_hebra].escritoras_esperando++;
            continue;
        }

        uint64_t nuevo = estado_actual + UNO_LECTORA;

        if (estado.compare_exchange_weak(estado_actual, nuevo)) {
            return;
        }

        
        contadores_por_hebra[id_hebra].cas_fallido++;
    }
}

void ControlAtomicoRW::salir_lectura() {
    // Implementación de la salida de lectura
    while (true) {
        uint64_t estado_actual = estado.load();
        uint64_t nuevo = estado_actual - UNO_LECTORA;

        if (estado.compare_exchange_weak(estado_actual, nuevo)) {
            return;
        }

        contadores_por_hebra[id_hebra].cas_fallido++;
    }
}

void ControlAtomicoRW::entrar_escritura() {
    // Implementación de la entrada de escritura
    while (true) {
        uint64_t estado_actual = estado.load();
        //registramos un escritor esperando
        uint64_t lector_nuevo = estado_actual + UNO_LECTORA;

        if (estado.compare_exchange_weak(estado_actual, lector_nuevo)) {
            break;
        }

        contadores_por_hebra[id_hebra].cas_fallido++;

    }
    
    while (true){
        //esperamos a  poder acceder a la escritura
        uint64_t estado_actual = estado.load();
        uint64_t lectores_activos = estado_actual & MASK_LECTORAS;
        bool escritor_activo = (estado_actual & BIT_ESC_ACTIVA) != 0;

        if(lectores_activos > 0 || escritor_activo){
            contadores_por_hebra[id_hebra].recurso_ocupado++;
            continue;
        }

        uint64_t nuevo = estado_actual - UNO_ESC_ESP + BIT_ESC_ACTIVA;

        if (estado.compare_exchange_weak(estado_actual, nuevo)) {
            return;
        }

        contadores_por_hebra[id_hebra].cas_fallido++;

    }    
}

void ControlAtomicoRW::salir_escritura() {
    // Implementación de la salida de escritura
    while (true) {
        uint64_t estado_actual = estado.load();
        uint64_t nuevo = estado_actual - BIT_ESC_ACTIVA;

        if (estado.compare_exchange_weak(estado_actual, nuevo)) {
            return;
        }

        contadores_por_hebra[id_hebra].cas_fallido++;
    }
}


std::optional<MetricasAtomicas> ControlAtomicoRW::obtener_metricas_atomicas() const {
    MetricasAtomicas metricas{};
    for (const auto& contadores : contadores_por_hebra) {
        metricas.recurso_ocupado += contadores.recurso_ocupado;
        metricas.escritoras_esperando += contadores.escritoras_esperando;
        metricas.cas_fallido += contadores.cas_fallido;
    }
    return metricas;
}