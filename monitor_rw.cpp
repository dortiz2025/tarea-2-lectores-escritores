#include "monitor_rw.hpp"

extern thread_local int id_hebra;

MonitorRW::MonitorRW(Politica politica, bool registrar_traza)
    :politica(politica),
     registrar_traza(registrar_traza),
     lectores_activos(0),
     escritora_activa(false),
     lectores_esperando(0),
     escritores_esperando(0),
     turno_actual(0),
     turno_siguiente(0) {}

bool MonitorRW::puede_entrar_lectura(long turno){
    if(politica == Politica::Lectores){
        return !escritora_activa;
    }else if (politica == Politica::Escritores){
        return (!escritora_activa && escritores_esperando == 0);
    }else if (politica == Politica::Fifo){
        return (!escritora_activa && turno == turno_actual);
    }
    return false;
    
}

bool MonitorRW::puede_entrar_escritura(long turno){
    if(politica == Politica::Lectores){
        return (lectores_activos == 0 && !escritora_activa && lectores_esperando == 0);
    }else if (politica == Politica::Escritores){
        return (lectores_activos == 0 && !escritora_activa);
    }else if (politica == Politica::Fifo){
        return (lectores_activos == 0 && !escritora_activa && turno == turno_actual);
    }
    return false;
}

void MonitorRW::entrar_lectura(){
    long turno = 0;
    long turno_traza = -1;
    
    std::unique_lock<std::mutex> lock(m);

    if(politica == Politica::Fifo){
        turno = turno_siguiente++;
        turno_traza = turno;
    }

    anotar(Tipo::Lectora, Accion::Pide, turno_traza);

    lectores_esperando++;

    cv_lectores.wait(lock, [&](){
        return puede_entrar_lectura(turno);
    });
    
    lectores_esperando--;
    lectores_activos++;

    if(politica == Politica::Fifo){
        turno_actual++;
    }

    anotar(Tipo::Lectora, Accion::Entra, turno_traza);

    cv_escritores.notify_all();
    cv_lectores.notify_all();
}

void MonitorRW::salir_lectura(){
    std::unique_lock<std::mutex> lock(m);

    lectores_activos--;

    anotar(Tipo::Lectora, Accion::Sale, -1);
    
    if(lectores_activos == 0){
        cv_escritores.notify_all();
    }
    cv_lectores.notify_all();
}

void MonitorRW::entrar_escritura(){
    long turno = 0;
    long turno_traza = -1; 

    std::unique_lock<std::mutex> lock(m);
    
    if(politica == Politica::Fifo){
        turno = turno_siguiente++;
        turno_traza = turno;
    }
    
    anotar(Tipo::Escritora, Accion::Pide, turno_traza);

    escritores_esperando++;
    
    cv_escritores.wait(lock, [&](){
        return puede_entrar_escritura(turno);
    });
    
    escritores_esperando--;
    escritora_activa = true;

    if(politica == Politica::Fifo){
        turno_actual++;
    }

    anotar(Tipo::Escritora, Accion::Entra, turno_traza);
}

void MonitorRW::salir_escritura(){
    std::unique_lock<std::mutex> lock(m);
    
    escritora_activa = false;
    
    anotar(Tipo::Escritora, Accion::Sale, -1);
    
    cv_escritores.notify_all();
    cv_lectores.notify_all();
    
}

void MonitorRW::anotar(Tipo tipo, Accion accion, long turno){
    if(registrar_traza){
        traza.push_back(AnotacionTraza{id_hebra, tipo, accion, turno});
    }
}

std::vector<AnotacionTraza> MonitorRW::obtener_traza(){
    std::unique_lock<std::mutex> lock(m);
    return traza;
}
