#ifndef MONITOR_RW_HPP
#define MONITOR_RW_HPP

#include "control_rw.hpp"
#include <condition_variable>
#include <mutex>
#include <vector>

enum class Politica{Lectores, Escritores, Fifo};
enum class Tipo{Lectora, Escritora};
enum class Accion{Pide, Entra, Sale};

struct AnotacionTraza{
    int hebra;
    Tipo tipo;
    Accion accion;
    long turno;
};

class MonitorRW : public ControlRW {
    public: 

    explicit MonitorRW(Politica p, bool registrar_traza = true);

    void entrar_lectura() override;
    void salir_lectura() override;
    void entrar_escritura() override;
    void salir_escritura() override;

    std::vector<AnotacionTraza> obtener_traza();
    private:

    Politica politica;
    bool registrar_traza;
    std::mutex m;
    std::vector<AnotacionTraza> traza;
    std::condition_variable cv_lectores;
    std::condition_variable cv_escritores;

    int lectores_activos;
    bool escritora_activa;
    int lectores_esperando;
    int escritores_esperando;    

    long turno_actual;
    long turno_siguiente;

    bool puede_entrar_lectura(long turno);
    bool puede_entrar_escritura(long turno);
    void anotar(Tipo tipo, Accion accion, long turno);
};
#endif