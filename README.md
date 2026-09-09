# minerby

A from-scratch **CPU miner written in C++20** — built to understand Proof-of-Work
mining end to end rather than to turn a profit. It speaks two pool protocols,
ships two hashing engines, and runs unattended for weeks.

![CI](https://github.com/Liuberth33/minerby/actions/workflows/ci.yml/badge.svg)
&nbsp;·&nbsp; C++20 · CMake · MSVC / GCC · Windows + Linux
&nbsp;·&nbsp; **[Español ↓](#-español)**

> ⚠️ **Legitimate use only.** Run `minerby` on hardware you own or administer, and
> be mindful of the CPU, power and heat it costs. It is not built to run on other
> people's machines or to hide its activity.

---

## What it does

```
minerby ──▶ mining pool ──▶ your wallet ──▶ exchange ──▶ fiat
   │                            │
 hashes                 pool pays out at its threshold
```

`minerby` covers the first hop: compute hashes, find shares, submit them so coins
land in a wallet you control. Everything after that is manual.

| Layer | Implementation |
|-------|----------------|
| **Pool protocol** | Bitcoin-style **Stratum V1** *and* the **Monero / xmrig JSON** protocol, behind one `PoolClient` interface |
| **PoW engine** | Hand-written streaming **SHA-256d**, and **RandomX** (`rx/0`, Monero mainnet) via the official library, behind one `IHasher` interface |
| **Work** | Protocol-agnostic `MiningJob` (blob + nonce offset + 256-bit target); Stratum V1 path assembles the coinbase, folds the merkle branch and lays out the 80-byte header |
| **Mining** | Fixed thread pool scanning the nonce space, generation-based work replacement, pause/resume for RandomX re-keying, optional low scheduling priority |
| **Ops** | Rolling-EMA hashrate, `/metrics` (Prometheus) HTTP endpoint, persistent lifetime stats across restarts, connection watchdog, clean shutdown, auto-restart scripts |

Verified against a live Monero pool: login, job parsing, RandomX re-keying with
the real network seed, share detection and `submit` all confirmed — the pool's
server-side hash recomputation matched, shares were rejected only on difficulty.

---

## Build

Needs a C++20 compiler (MSVC or GCC), CMake ≥ 3.24, Ninja, and the submodules.

```bash
git clone --recurse-submodules https://github.com/Liuberth33/minerby.git
cd minerby
```

**Windows (MSVC):**

```powershell
./scripts/build.ps1          # locates the VS Build Tools and builds with Ninja
```

**Any platform:**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

`FetchContent` pulls `nlohmann/json`, `spdlog`, `CLI11` and `doctest`; RandomX is
built from `third_party/RandomX`. Build without RandomX (SHA-256d only) with
`-DMINERBY_WITH_RANDOMX=OFF`.

---

## Usage

### Benchmark a kernel (no network)

```bash
./build/minerby bench --engine sha256d --threads 4 --seconds 15
./build/minerby bench --engine randomx --threads 4 --seconds 30           # light (256 MB)
./build/minerby bench --engine randomx --fast --threads 8 --seconds 30    # fast (~2.3 GB dataset)
```

### Estimate profitability

```bash
./build/minerby estimate --hashrate 2500 --net-difficulty 400000000000 --xmr-price 150
```

### Mine

```bash
cp config.monero.example.json config.json      # edit with your address + pool
./build/minerby run --config config.json
```

`config.json` is git-ignored — never commit credentials or a wallet address.

```jsonc
{
  "protocol":     "monero",        // "monero" (RandomX) or "bitcoin" (Stratum V1)
  "engine":       "randomx",       // "randomx" or "sha256d"
  "pool_host":    "pool.example.com",
  "pool_port":    3333,            // must be a non-TLS port
  "user":         "YOUR_ADDRESS",  // the pool pays here
  "pass":         "minerby",
  "threads":      0,               // 0 = auto
  "cpu_priority": "low",           // "low" yields to the desktop; "normal" = max speed
  "metrics_port": 9101,            // 0 = off; else serves /metrics (Prometheus)
  "stats_file":   "",              // "" = minerby-stats.json next to the config
  "randomx": {
    "mode":         "light",       // "light" (256 MB) or "fast" (~2.3 GB, much faster)
    "init_threads": 0,
    "large_pages":  false,
    "secure":       false
  },
  "net_difficulty": 0,             // > 0 prints a live earnings estimate
  "block_reward":   0.6,
  "coin_price_usd": 0
}
```

### Run it 24/7

```bash
./scripts/mine-forever.ps1            # relaunches minerby if it exits
./scripts/install-autostart.ps1       # per-user scheduled task, starts at logon (no admin)
```

`minerby-stats.json` accumulates shares, hashes, sessions and total uptime across
restarts. A watchdog reconnects if the pool goes quiet for 150 s. `Ctrl+C`,
closing the console, or logging off all shut down cleanly and flush stats.
`run --for 3600` mines for an hour and stops.

---

## Layout

```
src/
  net/         blocking TCP client with line-oriented reads (Winsock / POSIX)
  pool/        PoolClient · StratumV1Client (sha256d) · MoneroClient (RandomX)
  stratum/     mining.notify / subscribe parsing + coinbase/merkle/header assembly
  pow/         IHasher · SHA-256 · Sha256dHasher · target/difficulty math ·
               RandomXContext (shared cache/dataset) · RandomXHasher
  miner/       generic MiningJob · worker thread pool (nonce, pause, reseed, priority)
  telemetry/   counters, EMA hashrate, /metrics server, persistent stats
  config/      JSON config + CLI overrides
  util/        hex helpers, per-platform thread priority
  app/         bench / run / estimate subcommands
third_party/
  RandomX/     submodule, pinned to v1.2.3 (rx/0, Monero mainnet)
tests/         SHA-256 & RandomX known-answer vectors, hex, target/difficulty,
               Monero target expansion, Stratum parsing, stats store
```

Tests: 27 cases / 192 assertions. CI builds and tests on Windows and Linux.

---

## Status

| Phase | Scope | State |
|-------|-------|-------|
| 0 | Scaffold, CMake, module layout, CI | ✅ |
| 1 | SHA-256d + full Stratum V1 + coinbase/merkle/header assembly | ✅ |
| 2 | RandomX engine + Monero protocol + generic pipeline + `estimate` | ✅ |
| 3 | Unattended 24/7: low priority, persistent stats, watchdog, clean shutdown, auto-restart | ✅ |
| 3+ | `--engine xmrig` supervisor, live price feed, native Windows service, web dashboard, multi-pool failover | ⏳ |

**Parked pending hardware.** CPU mining Monero on a thin laptop earns cents per
month — less than the electricity. The project is complete and correct as an
engineering exercise; it will be revisited on a machine (or dedicated rig) where
the numbers make sense.

### Antivirus

Any miner trips "coinminer" heuristics (Avast flags `Win64:CoinMiner` and
DNS-blocks pool domains). If a security suite blocks the build or the binary, add
the project folder to its exceptions and/or allow the pool domain — it is a
false positive on an unsigned mining binary, not an infection.

---

## 🇪🇸 Español

Minero de **CPU escrito desde cero en C++20**, hecho para entender la minería
Proof-of-Work de principio a fin, no para ganar dinero. Habla dos protocolos de
pool, trae dos motores de hashing y aguanta semanas funcionando solo.

> ⚠️ **Uso legítimo únicamente.** Ejecútalo en hardware que poseas o administres,
> consciente de la CPU, la energía y el calor que cuesta. No está pensado para
> equipos ajenos ni para ocultar su actividad.

### Qué hace

`minerby` cubre el primer tramo: calcular hashes, encontrar shares y enviarlas
para que las monedas lleguen a una wallet tuya. Lo de después (vender en un
exchange) es manual.

| Capa | Implementación |
|------|----------------|
| **Protocolo de pool** | **Stratum V1** (estilo Bitcoin) *y* el protocolo **Monero / xmrig JSON**, tras una interfaz `PoolClient` |
| **Motor PoW** | **SHA-256d** propio (streaming) y **RandomX** (`rx/0`, Monero mainnet) vía la librería oficial, tras una interfaz `IHasher` |
| **Trabajo** | `MiningJob` genérico (blob + offset del nonce + target de 256 bits); en Stratum V1 se ensambla el coinbase, se pliega la rama merkle y se arma la cabecera de 80 bytes |
| **Minado** | Pool de hilos fijo, reemplazo de trabajo por generación, pausa/reanudación para re-derivar RandomX, prioridad de CPU baja opcional |
| **Operación** | Hashrate (EMA), endpoint `/metrics` (Prometheus), estadísticas persistentes entre reinicios, watchdog de conexión, apagado limpio, scripts de auto-reinicio |

Verificado contra un pool real de Monero: login, parseo de jobs, re-derivación de
RandomX con el seed real de la red, detección y envío de shares — el pool
recalculó el hash por su cuenta y coincidió; solo rechazó por dificultad.

### Compilar

Requiere compilador C++20 (MSVC o GCC), CMake ≥ 3.24, Ninja y los submódulos.

```bash
git clone --recurse-submodules https://github.com/Liuberth33/minerby.git
cd minerby
```

**Windows (MSVC):** `./scripts/build.ps1`

**Cualquier plataforma:**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Sin RandomX (solo SHA-256d): `-DMINERBY_WITH_RANDOMX=OFF`.

### Uso

```bash
# Benchmark de un motor (sin red)
./build/minerby bench --engine randomx --threads 4 --seconds 30

# Estimación de rentabilidad
./build/minerby estimate --hashrate 2500 --net-difficulty 400000000000 --xmr-price 150

# Minar
cp config.monero.example.json config.json     # edítalo con tu dirección y tu pool
./build/minerby run --config config.json

# 24/7
./scripts/mine-forever.ps1                     # relanza el minero si se cae
./scripts/install-autostart.ps1               # tarea al iniciar sesión (sin admin)
```

`config.json` está en `.gitignore` — nunca subas credenciales ni una dirección de
wallet. Referencia de configuración y estructura del código: ver la parte en
inglés arriba.

### Estado

Fases 0-3 completas: scaffold + CI · SHA-256d + Stratum V1 · RandomX + Monero ·
operación 24/7 desatendida. Pendiente (3+): modo `--engine xmrig`, feed de precio
en vivo, servicio de Windows nativo, dashboard web, failover multi-pool.

**Aparcado a la espera de hardware.** Minar Monero por CPU en una laptop fina
deja céntimos al mes, menos que la electricidad. El proyecto está completo y
correcto como ejercicio de ingeniería; se retomará en una máquina (o rig
dedicado) donde los números tengan sentido.

### Antivirus

Todo minero dispara heurísticas de "coinminer" (Avast marca `Win64:CoinMiner` y
bloquea por DNS los dominios de pools). Si un antivirus bloquea la compilación o
el binario, añade la carpeta del proyecto a sus excepciones y/o permite el
dominio del pool — es un falso positivo por ser un binario de minería sin firmar,
no una infección.

---

## License

MIT — see [LICENSE](LICENSE). RandomX is MIT, © tevador / The Monero Project.
