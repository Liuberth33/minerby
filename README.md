# minerby

Minero de CPU en C++ para aprender cómo funciona la minería Proof-of-Work de
principio a fin: cliente de pool (**Stratum V1** y **protocolo Monero/xmrig**),
kernels de hashing (**sha256d propio** y **RandomX**), pool de hilos y telemetría.
Construido por fases.

> ⚠️ **Uso legítimo únicamente.** Ejecuta `minerby` solo en hardware que tú
> posees o administras, y sé consciente del consumo de CPU/energía que implica.
> No está pensado para instalarse en equipos ajenos ni para ocultar su
> actividad. La minería sostenida en un portátil genera calor y desgaste: usa
> `threads` para limitar el número de núcleos.

---

## El flujo completo

```
minerby (este software)  ──▶  Pool de minería  ──▶  Tu wallet (dirección propia)
        │                                                   │
   calcula hashes                          el pool paga al alcanzar el mínimo
                                                            │
                                                            ▼
                          Depósito en Binance  ──▶  Vender por USDT/USD  ──▶  Retirar
```

`minerby` cubre solo el primer tramo: **entregar shares a un pool para que las
monedas lleguen a una wallet**. La venta en Binance (u otra plataforma) la haces
tú manualmente. Muchos pools de Monero permiten configurar la dirección de pago
directamente como tu dirección de depósito del exchange.

### Nota de rentabilidad (sé realista)

En una CPU/portátil normal la minería rinde muy poco — a menudo entre céntimos y
unos pocos dólares al día en bruto, y la electricidad puede costar más que lo que
generas. Bitcoin es solo-ASIC; Ethereum ya no se mina (pasó a Proof-of-Stake).
La opción viable para CPU es **Monero (RandomX)**. Usa `minerby estimate` para
ver una cifra aproximada antes de dejarlo corriendo.

---

## Roadmap por fases

| Fase | Contenido | Estado |
|------|-----------|--------|
| **0** | Scaffold: CMake, módulos `net` / `pool` / `pow` / `miner` / `telemetry` / `config`, tests, CI | ✅ |
| **1** | `Sha256dHasher` propio + Stratum V1 (subscribe/authorize/notify/set_difficulty/submit) + ensamblado coinbase/merkle/header. Falta prueba de envío de shares contra un pool real. | ✅ (código) |
| **2** | `RandomXHasher` (submódulo `third_party/RandomX`, pin `v1.2.3` = `rx/0` de Monero mainnet) + protocolo Monero/xmrig (login/job/submit) + pipeline `MiningJob` genérico + `minerby estimate`. | ✅ |
| **3** | Modo `--engine xmrig` (supervisar XMRig), feed de precio en vivo, servicio de Windows, dashboard web, failover multi-pool, huge pages. | ⏳ |

---

## Compilar (Windows / MSVC)

Necesitas **Visual Studio Build Tools** con el workload de C++ (MSVC + CMake +
Ninja) y los submódulos:

```powershell
git submodule update --init --recursive
./scripts/build.ps1
```

O manualmente, dentro de un entorno con `vcvars64.bat` cargado:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

La primera configuración descarga `nlohmann/json`, `spdlog`, `CLI11` y `doctest`
con `FetchContent`, y compila RandomX desde `third_party/RandomX`. Para una build
sin RandomX (solo sha256d): `-DMINERBY_WITH_RANDOMX=OFF`.

### Antivirus

Todo software de minería dispara heurísticas de "coinminer" (Avast marca
`Win64:CoinMiner` / PUP). Si el antivirus bloquea la compilación o los binarios,
**añade la carpeta del proyecto a las excepciones del antivirus** (en Avast:
Menú → Configuración → General → Excepciones → añadir `C:\ruta\a\minerby`). Es
un falso positivo por ser un miner sin firmar, no una infección.

---

## Uso

### Benchmark de un kernel

```powershell
./build/minerby bench --engine sha256d --threads 4 --seconds 15
./build/minerby bench --engine randomx --threads 4 --seconds 30          # RandomX light
./build/minerby bench --engine randomx --fast --threads 8 --seconds 30   # RandomX fast (~2.3 GB)
```

### Estimación de rentabilidad

```powershell
./build/minerby estimate --hashrate 2500 --net-difficulty 400000000000 --xmr-price 150
```

Toma la dificultad de red de un explorador de bloques o de la página de stats del
pool. La cifra ignora comisiones, varianza y deriva de dificultad.

### Minar Monero (RandomX)

```powershell
copy config.monero.example.json config.json     # edítalo con tu dirección XMR y tu pool
./build/minerby run --config config.json
```

```jsonc
{
  "protocol": "monero",
  "engine": "randomx",
  "pool_host": "pool.ejemplo.com",
  "pool_port": 3333,
  "user": "TU_DIRECCION_XMR",     // el pool paga aquí; puede ser tu depósito de Binance
  "pass": "minerby",
  "threads": 0,                    // 0 = auto
  "metrics_port": 9101,            // 0 = desactivado; expone /metrics (Prometheus)
  "randomx": {
    "mode": "light",              // "light" = 256 MB · "fast" = ~2.3 GB (mucho más rápido)
    "init_threads": 0,            // hilos para construir el dataset en modo fast
    "large_pages": false,         // requiere privilegio del SO; +2-3x cuando está disponible
    "secure": false               // JIT con W^X (sistemas endurecidos)
  },
  "net_difficulty": 0,            // >0 activa la estimación en vivo en el log
  "block_reward": 0.6,
  "coin_price_usd": 0
}
```

**Modo light vs fast:** `fast` reserva el dataset de RandomX (~2 GB) y es varias
veces más rápido, pero necesita ~2.3 GB de RAM libre. `light` usa solo la caché
de 256 MB. El seed de RandomX (`seed_hash`) cambia cada ~3 días en mainnet;
`minerby` re-deriva la caché automáticamente y pausa los hilos mientras tanto.

### Minar contra un pool SHA-256 (Stratum V1)

```powershell
copy config.bitcoin.example.json config.json
./build/minerby run --config config.json
```

`config.json` está en `.gitignore` — nunca subas tus credenciales ni tu wallet.

---

## Estructura

```
src/
  net/         TCP cliente (Winsock/POSIX) con lectura por líneas
  pool/        PoolClient · StratumV1Client (sha256d) · MoneroClient (RandomX)
  stratum/     parseo de mining.notify/subscribe + ensamblado header/merkle
  pow/         IHasher · SHA-256 · Sha256dHasher · target/dificultad ·
               RandomXContext (caché/dataset compartidos) · RandomXHasher
  miner/       MiningJob genérico · pool de hilos (nonce, pausa, reseed)
  telemetry/   contadores, hashrate (EMA), servidor HTTP /metrics
  config/      carga de configuración JSON + overrides CLI
  app/         subcomandos bench / run / estimate
third_party/
  RandomX/     submódulo, pin v1.2.3 (rx/0 de Monero mainnet)
tests/         KATs de SHA-256 y RandomX, hex, target, Monero target, parseo
```

## Licencia

MIT — ver [LICENSE](LICENSE). RandomX es MIT, © tevador / The Monero Project.
