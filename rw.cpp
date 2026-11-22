#include "control_atomico_rw.hpp"
#include "control_rw.hpp"
#include "monitor_rw.hpp"
#include "verificador.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Definición de la variable thread_local declarada en control_rw.hpp (R6)
thread_local int id_hebra = -1;

namespace {

struct Registro {
    long version = 0;
    long doble_version = 0;
};

struct EsperaHebra {
    double suma_us = 0.0;
    double max_us = 0.0;
};

bool parsear_long(const std::string& texto, long& salida) {
    try {
        std::size_t pos = 0;
        long val = std::stol(texto, &pos, 10);
        if (pos != texto.size()) {
            return false;
        }
        salida = val;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

double tiempo_cpu_segundos() {
    struct timespec ts {};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
        return 0.0;
    }
    return static_cast<double>(ts.tv_sec) +
           static_cast<double>(ts.tv_nsec) * 1e-9;
}

void imprimir_uso(const char* prog) {
    std::cerr << "Uso: " << prog
              << " <modo> <politica> <L> <E> <K> <dur_lec_us> <dur_esc_us> [notraza]\n"
              << "  modo: cond | atomic\n"
              << "  politica: lectores | escritores | fifo (atomic solo admite escritores)\n"
              << "  L >= 1, E >= 1, L + E <= 1000, K >= 1, duraciones >= 0\n";
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 8 && argc != 9) {
        imprimir_uso(argv[0]);
        return 1;
    }

    const std::string modo = argv[1];
    const std::string politica_str = argv[2];

    bool notraza = false;
    if (argc == 9) {
        if (std::string(argv[8]) != "notraza") {
            std::cerr << "Error: argumento opcional invalido '" << argv[8]
                      << "' (esperado: notraza).\n";
            return 1;
        }
        notraza = true;
    }

    if (modo != "cond" && modo != "atomic") {
        std::cerr << "Error: modo invalido '" << modo << "'.\n";
        return 1;
    }

    Politica politica{};
    if (politica_str == "lectores") {
        politica = Politica::Lectores;
    } else if (politica_str == "escritores") {
        politica = Politica::Escritores;
    } else if (politica_str == "fifo") {
        politica = Politica::Fifo;
    } else {
        std::cerr << "Error: politica invalida '" << politica_str << "'.\n";
        return 1;
    }

    if (modo == "atomic" && politica != Politica::Escritores) {
        std::cerr << "Error: el modo atomic solo admite la politica escritores.\n";
        return 1;
    }

    long L = 0, E = 0, K = 0, dur_lec_us = 0, dur_esc_us = 0;
    if (!parsear_long(argv[3], L) || !parsear_long(argv[4], E) ||
        !parsear_long(argv[5], K) || !parsear_long(argv[6], dur_lec_us) ||
        !parsear_long(argv[7], dur_esc_us)) {
        std::cerr << "Error: los parametros numericos deben ser enteros validos.\n";
        return 1;
    }

    if (L < 1 || E < 1 || (L + E) > 1000 || K < 1 || dur_lec_us < 0 || dur_esc_us < 0) {
        std::cerr << "Error: parametros fuera de rango.\n";
        imprimir_uso(argv[0]);
        return 1;
    }

    // Verificar que 2*E*K y (L+E)*K no desborden long (R1)
    constexpr long MAX_LONG = std::numeric_limits<long>::max();
    if (K > MAX_LONG / (2 * E) || K > MAX_LONG / (L + E)) {
        std::cerr << "Error: 2*E*K o (L+E)*K desborda el tipo long.\n";
        return 1;
    }

    const int total_hebras = static_cast<int>(L + E);

    // Instanciar la implementación seleccionada
    MonitorRW* monitor_ptr = nullptr;
    std::unique_ptr<ControlRW> control;
    if (modo == "cond") {
        auto mon = std::make_unique<MonitorRW>(politica, !notraza);
        monitor_ptr = mon.get();
        control = std::move(mon);
    } else {
        control = std::make_unique<ControlAtomicoRW>(total_hebras);
    }

    // Estado compartido y contadores de diagnóstico atómicos separados (R2)
    Registro registro{};
    std::atomic<int> diag_lectoras_activas{0};
    std::atomic<int> diag_escritoras_activas{0};
    std::atomic<int> diag_max_lectoras{0};
    std::atomic<long> diag_inconsistencias{0};
    std::atomic<long> diag_colisiones_escritora{0};

    std::vector<EsperaHebra> esperas_lectoras(static_cast<std::size_t>(L));
    std::vector<EsperaHebra> esperas_escritoras(static_cast<std::size_t>(E));

    std::vector<std::thread> hebras;
    hebras.reserve(static_cast<std::size_t>(total_hebras));

    // Inicio de medición: desde antes de iniciar las hebras hasta todos los join() (E2)
    const auto pared_inicio = std::chrono::steady_clock::now();
    const double cpu_inicio = tiempo_cpu_segundos();

    try {
        // Hebras lectoras: ids [0 .. L - 1]
        for (long i = 0; i < L; ++i) {
            hebras.emplace_back([&, i]() {
                id_hebra = static_cast<int>(i);
                auto& est = esperas_lectoras[static_cast<std::size_t>(i)];

                for (long op = 0; op < K; ++op) {
                    const auto t0 = std::chrono::steady_clock::now();
                    control->entrar_lectura();
                    const auto t1 = std::chrono::steady_clock::now();

                    const double espera_us =
                        std::chrono::duration<double, std::micro>(t1 - t0).count();
                    est.suma_us += espera_us;
                    if (espera_us > est.max_us) {
                        est.max_us = espera_us;
                    }

                    // Diagnóstico al entrar (R2)
                    const int activas =
                        diag_lectoras_activas.fetch_add(1, std::memory_order_acq_rel) + 1;
                    int prev_max = diag_max_lectoras.load(std::memory_order_relaxed);
                    while (activas > prev_max &&
                           !diag_max_lectoras.compare_exchange_weak(
                               prev_max, activas, std::memory_order_relaxed)) {
                    }
                    if (diag_escritoras_activas.load(std::memory_order_acquire) > 0) {
                        diag_colisiones_escritora.fetch_add(1, std::memory_order_relaxed);
                    }

                    // Acceso al registro compartido
                    const long v = registro.version;
                    const long dv = registro.doble_version;
                    if (dv != 2 * v) {
                        diag_inconsistencias.fetch_add(1, std::memory_order_relaxed);
                    }

                    if (dur_lec_us > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(dur_lec_us));
                    }

                    // Diagnóstico antes de salir
                    if (diag_escritoras_activas.load(std::memory_order_acquire) > 0) {
                        diag_colisiones_escritora.fetch_add(1, std::memory_order_relaxed);
                    }
                    diag_lectoras_activas.fetch_sub(1, std::memory_order_acq_rel);

                    control->salir_lectura();
                }
            });
        }

        // Hebras escritoras: ids [L .. L + E - 1]
        for (long j = 0; j < E; ++j) {
            hebras.emplace_back([&, j]() {
                id_hebra = static_cast<int>(L + j);
                auto& est = esperas_escritoras[static_cast<std::size_t>(j)];

                for (long op = 0; op < K; ++op) {
                    const auto t0 = std::chrono::steady_clock::now();
                    control->entrar_escritura();
                    const auto t1 = std::chrono::steady_clock::now();

                    const double espera_us =
                        std::chrono::duration<double, std::micro>(t1 - t0).count();
                    est.suma_us += espera_us;
                    if (espera_us > est.max_us) {
                        est.max_us = espera_us;
                    }

                    // Diagnóstico al entrar (R2)
                    const int esc_activas =
                        diag_escritoras_activas.fetch_add(1, std::memory_order_acq_rel) + 1;
                    if (esc_activas > 1 ||
                        diag_lectoras_activas.load(std::memory_order_acquire) > 0) {
                        diag_colisiones_escritora.fetch_add(1, std::memory_order_relaxed);
                    }

                    // Modificación exclusiva del registro compartido
                    registro.version++;
                    registro.doble_version = 2 * registro.version;

                    if (dur_esc_us > 0) {
                        std::this_thread::sleep_for(std::chrono::microseconds(dur_esc_us));
                    }

                    // Diagnóstico antes de salir
                    if (diag_escritoras_activas.load(std::memory_order_acquire) > 1 ||
                        diag_lectoras_activas.load(std::memory_order_acquire) > 0) {
                        diag_colisiones_escritora.fetch_add(1, std::memory_order_relaxed);
                    }
                    diag_escritoras_activas.fetch_sub(1, std::memory_order_acq_rel);

                    control->salir_escritura();
                }
            });
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error al crear hebras: " << ex.what() << "\n";
        for (auto& t : hebras) {
            if (t.joinable()) {
                t.join();
            }
        }
        return 1;
    }

    for (auto& t : hebras) {
        if (t.joinable()) {
            t.join();
        }
    }

    const auto pared_fin = std::chrono::steady_clock::now();
    const double cpu_fin = tiempo_cpu_segundos();

    const double tiempo_pared_ms =
        std::chrono::duration<double, std::milli>(pared_fin - pared_inicio).count();
    const double tiempo_cpu_ms = (cpu_fin - cpu_inicio) * 1000.0;

    // Agregar métricas de espera
    double suma_espera_lec = 0.0, max_espera_lec = 0.0;
    for (const auto& e : esperas_lectoras) {
        suma_espera_lec += e.suma_us;
        if (e.max_us > max_espera_lec) {
            max_espera_lec = e.max_us;
        }
    }
    const double prom_espera_lec = suma_espera_lec / static_cast<double>(L * K);

    double suma_espera_esc = 0.0, max_espera_esc = 0.0;
    for (const auto& e : esperas_escritoras) {
        suma_espera_esc += e.suma_us;
        if (e.max_us > max_espera_esc) {
            max_espera_esc = e.max_us;
        }
    }
    const double prom_espera_esc = suma_espera_esc / static_cast<double>(E * K);

    // Comprobación de invariantes (R2)
    const long version_esperada = E * K;
    const long doble_esperada = 2 * E * K;
    const long inconsistencias = diag_inconsistencias.load();
    const long colisiones = diag_colisiones_escritora.load();

    const bool correcto = (registro.version == version_esperada) &&
                          (registro.doble_version == doble_esperada) &&
                          (inconsistencias == 0) &&
                          (colisiones == 0);

    // Resumen final (Sección 5)
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "modo=" << modo
              << " politica=" << politica_str
              << " L=" << L << " E=" << E << " K=" << K
              << " operaciones=" << (L + E) * K << "\n";
    std::cout << "version_final=" << registro.version
              << " (esperada=" << version_esperada << ")"
              << " doble_version_final=" << registro.doble_version
              << " (esperada=" << doble_esperada << ")\n";
    std::cout << "inconsistencias=" << inconsistencias
              << " colisiones_escritora=" << colisiones
              << " max_lectoras_simultaneas=" << diag_max_lectoras.load() << "\n";
    std::cout << "tiempo_pared_ms=" << tiempo_pared_ms
              << " tiempo_cpu_ms=" << tiempo_cpu_ms << "\n";
    std::cout << "espera_lectoras_us: prom=" << prom_espera_lec
              << " max=" << max_espera_lec << "\n";
    std::cout << "espera_escritoras_us: prom=" << prom_espera_esc
              << " max=" << max_espera_esc << "\n";
    std::cout << "correcto=" << (correcto ? "SI" : "NO") << "\n";

    if (modo == "cond" && !notraza && monitor_ptr != nullptr) {
        const auto traza = monitor_ptr->obtener_traza();
        const auto res = verificar_traza(traza, politica);
        std::cout << "politica_ok=" << (res.politica_ok ? "SI" : "NO")
                  << " evidencia=" << res.evidencia << "\n";
        if (!res.politica_ok) {
            std::cout << "violacion_traza: indice=" << res.indice_violacion
                      << " regla=" << res.regla_incumplida << "\n";
        }
    }

    if (const auto metricas = control->obtener_metricas_atomicas(); metricas.has_value()) {
        std::cout << "fallos_atomic: recurso_ocupado=" << metricas->recurso_ocupado
                  << " escritoras_esperando=" << metricas->escritoras_esperando
                  << " cas_fallido=" << metricas->cas_fallido << "\n";
    }

    return correcto ? 0 : 1;
}