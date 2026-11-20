#ifndef MONITOR_RW_HPP
#define MONITOR_RW_HPP

#include "control_rw.hpp"

#include <condition_variable>
#include <mutex>
#include <vector>

enum class Politica { Lectores, Escritores, Fifo };

enum class Tipo { Lectora, Escritora };
enum class Accion { Pide, Entra, Sale };

struct AnotacionTraza {
    int hebra;      // identificador lógico de la hebra
    Tipo tipo;
    Accion accion;
    long turno;     // turno FIFO; -1 en las otras políticas
};

class MonitorRW : public ControlRW {
public:
    explicit MonitorRW(Politica p, bool registrar_traza = true);

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

    std::vector<AnotacionTraza> obtener_traza(); // ver R6

private:
    Politica politica;
    bool registrar_traza;
    std::mutex m;                              // mutex del monitor
    std::vector<AnotacionTraza> traza;         // ver R6

    // Variables de condición y estado de admisión (R3, R4, R5)
    std::condition_variable cv_lectoras;
    std::condition_variable cv_escritoras;

    int lectoras_activas = 0;
    bool escritora_activa = false;
    int lectoras_esperando = 0;
    int escritoras_esperando = 0;

    // Control de turnos consecutivos para la política FIFO (R5)
    long proximo_turno = 0;
    long turno_actual = 0;

    // Helper interno (se invoca siempre con el mutex m adquirido)
    void anotar(Tipo tipo, Accion accion, long turno);
};

#endif // MONITOR_RW_HPP