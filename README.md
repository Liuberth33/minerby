# minerby

Minero de CPU en C++ para aprender cómo funciona la minería Proof-of-Work de
principio a fin: cliente **Stratum V1**, kernel de **hashing propio**, pool de
hilos y telemetría. Construido por fases.

> ⚠️ **Uso legítimo únicamente.** Ejecuta `minerby` solo en hardware que tú
> posees o administras, y sé consciente del consumo de CPU/energía que implica.
> No está pensado para instalarse en equipos ajenos ni para ocultar su
> actividad. La minería sostenida en un portátil genera calor y desgaste: usa
> `--threads` para limitar el número de núcleos.

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
tú manualmente. Muchos pools permiten configurar la dirección de pago
directamente como tu dirección de depósito del exchange.

### Nota de rentabilidad (sé realista)

En una CPU/portátil normal la minería rinde muy poco — a menudo entre céntimos y
unos pocos dólares al día en bruto, y la electricidad puede costar más que lo que
generas. Bitcoin es solo-ASIC; Ethereum ya no se mina (pasó a Proof-of-Stake).
La opción viable para CPU es **Monero (RandomX)**, objetivo de la Fase 2.

---

## Roadmap por fases

| Fase | Contenido | Estado |
|------|-----------|--------|
| **0** | Scaffold: CMake, módulos `net` / `stratum` / `pow` / `miner` / `telemetry` / `config`, tests, CI | ✅ |
| **1** | `Sha256dHasher` propio + Stratum V1 completo contra un pool real (moneda SHA-256 / testnet). Conecta, autoriza, recibe jobs, hashea, envía shares. | 🚧 en curso |
| **2** | `RandomXHasher` (submódulo `third_party/RandomX`) + dialecto Stratum de Monero. Estimador de rentabilidad XMR/día → USD. Modo `--engine xmrig` que supervisa XMRig. | ⏳ |
| **3** | Servicio de Windows, métricas Prometheus, dashboard web, failover multi-pool, huge pages. | ⏳ |

---

## Compilar (Windows / MSVC)

Necesitas **Visual Studio Build Tools** con el workload de C++ (MSVC + CMake +
Ninja). Desde un *Developer PowerShell* o usando el script incluido:

```powershell
./scripts/build.ps1
```

O manualmente, dentro de un entorno con `vcvars64.bat` cargado:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

La primera configuración descarga las dependencias con `FetchContent`:
`nlohmann/json`, `spdlog`, `CLI11` y `doctest`.

---

## Uso

### Benchmark del kernel de hashing

```powershell
./build/minerby bench --threads 4 --seconds 15
```

Mide el hashrate sostenido de sha256d en tu CPU. No se conecta a ningún pool.

### Minar contra un pool

```powershell
copy config.example.json config.json   # edítalo con tus datos
./build/minerby run --config config.json
```

`config.json` está en `.gitignore` — nunca subas tus credenciales ni tu wallet.

```jsonc
{
  "pool_host":   "pool.example.com",
  "pool_port":   3333,
  "user":        "TU_WALLET.worker1",
  "pass":        "x",
  "threads":     0,          // 0 = auto (núcleos disponibles)
  "metrics_port": 9101,      // 0 = desactivado; expone /metrics (Prometheus)
  "engine":      "sha256d"
}
```

---

## Estructura

```
src/
  net/         TCP cliente (Winsock) + lectura de líneas JSON-RPC
  stratum/     cliente Stratum V1 + ensamblado de header/merkle (build_work)
  pow/         interfaz IHasher · SHA-256 · Sha256dHasher · target/dificultad
  miner/       pool de hilos: iteración de nonce, detección de share
  telemetry/   contadores, hashrate (EMA), servidor HTTP /metrics
  config/      carga de configuración JSON + overrides CLI
  app/         main y apagado limpio
tests/         KATs de SHA-256, hex, target y parseo de Stratum
```

## Licencia

MIT — ver [LICENSE](LICENSE).
