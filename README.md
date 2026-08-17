# Albatastic PRO Watchdog

# 🌍🇬🇧 [English version below](#-english-version)

> ⚖️ **Licencia: CC BY-NC 4.0 — Uso no comercial únicamente**  
> Este diseño es de libre uso y modificación, **pero queda prohibido cualquier uso comercial** sin autorización expresa del autor.

---

## Descripción

Sistema de watchdog inteligente para nodos Meshtastic/MeshCore basado en **ATtiny13A**. Diseñado para integrarse en la PCB **Albatastic PRO v1.2**, monitoriza la actividad del módulo de radio y ejecuta un reset automático si detecta que el nodo se ha colgado.

A diferencia de un watchdog por tiempo fijo, este sistema **detecta actividad real** en el bus del módulo de radio. Si el nodo sigue funcionando, nunca se resetea innecesariamente.

También está disponible como **módulo independiente** que puede añadirse a nodos ya existentes, sin necesidad de usar la PCB Albatastic PRO.

<div align="center">
  <img src="images/WatchdogPCB.jpeg" width="60%" />
</div>

---

## 🆕 Versión "Fusion" — Watchdog para 2 nodos

A partir de esta revisión, el firmware incluye una variante **Fusion** capaz de vigilar **dos nodos independientes** (por ejemplo, un nodo Meshtastic y un nodo MeshCore) con un único ATtiny13A, compartiendo el relé de corte de corriente pero con reset suave individual por nodo.

- Cada nodo tiene su propio pin de reset suave (RST) y su propio pin de entrada watchdog.
- El **reset duro (corte de corriente) es compartido** — si cualquiera de los dos nodos se cuelga sin recuperarse, se cortan ambos a la vez, ya que comparten alimentación vía el mismo relé.
- El modo test se activa **puenteando entre sí los dos pines de reset suave** al arrancar (ver pinout más abajo), en lugar de usar jumpers dedicados — esto libera pines para monitorizar el segundo nodo.

> La versión Fusion es totalmente compatible con nodos que usen SX1262, LR1121 (EBYTE E80) o cualquier chip de radio que exponga una señal de actividad tipo DIO1.

---

## ¿Cómo funciona?

El ATtiny13A monitoriza el pin de **actividad del módulo de radio** (DIO1 en SX1262/LR1121, o señal equivalente) de cada nodo. Este pin cambia de estado con cada operación de TX/RX. Si deja de cambiar durante demasiado tiempo, el nodo correspondiente se considera bloqueado y el sistema actúa en **dos niveles**:

| Nivel | Condición | Acción |
|-------|-----------|--------|
| **Soft** | Sin actividad en el nodo durante X horas (config.) | Pulso en pin RST de ese MCU |
| **Hard** | Sin recuperación tras reset suave (+10 min) | Corte de corriente vía relé (afecta a ambos nodos si es versión Fusion) |
| **Preventivo** | Tiempo máximo configurable (activable/desactivable) | Reset duro aunque haya actividad |

> En la **versión estándar**, el reset preventivo se activa o desactiva con el jumper **OPCION2** (ver pinout más abajo); su intervalo (`HORAS_PREVENTIVO`) se ajusta en firmware. En la **versión Fusion**, al no disponer de jumpers de configuración de tiempos, se activa/desactiva con `PREVENTIVO_ACTIVO` directamente en firmware.

> 💡 **Nota sobre el tráfico de la malla**: Debido al funcionamiento de redes tipo Meshtastic/MeshCore, es raro que un nodo no reciba o retransmita algún mensaje en varias horas si hay tráfico en la zona. Si el nodo va a estar en una ubicación con poco o ningún tráfico, se recomienda aumentar el tiempo de detección (`HORAS_SOFT_PRO`) para evitar resets innecesarios. Si todo funciona correctamente y hay tráfico en la red, el watchdog no debería saltar.

---

## Pinout ATtiny13A

### Versión estándar (1 nodo + jumpers de configuración)

```
                RST  1 ─┐   ┌─ 8  VCC (VBAT directo, LiPo 3.0–4.2V)
        OPCION2 PB3  2 ─┤   ├─ 7  PB2 ← DIO1 módulo radio (watchdog)
        OPCION1 PB4  3 ─┤   ├─ 6  PB1 → RST MCU (reset SUAVE, via 100Ω)
                GND  4 ─┘   └─ 5  PB0 → Gate SI2312 (reset DURO, via 100Ω)
```

| Pin | Función |
|-----|---------|
| PB0 | Reset DURO → MOSFET SI2312 → Relé HFD4/3 → corte de corriente |
| PB1 | Reset SUAVE → pin RST del MCU |
| PB2 | Entrada watchdog ← DIO1 del módulo de radio |
| PB3 | Jumper OPCION2 → **activa/desactiva el reset preventivo** |
| PB4 | Jumper OPCION1 → modo test |

> **OPCION2**: abierto = reset preventivo **desactivado**. Soldado = reset preventivo **activado**, disparándose cada `HORAS_PREVENTIVO` (12h por defecto, configurable en firmware).
>
> **OPCION1**: soldado = **modo test** (timeouts cortos, 40s/64s). La lectura de ambos jumpers usa pull-up interno forzado, robusta ante pines flotantes.

### Versión Fusion (2 nodos)

```
                RST  1 ─┐   ┌─ 8  VCC (VBAT directo, LiPo 3.0–4.2V)
   WATCHDOG N2  PB3  2 ─┤   ├─ 7  PB2 ← WATCHDOG N1 (DIO1 nodo 1)
   RST NODO2    PB4  3 ─┤   ├─ 6  PB1 → RST NODO1 (reset SUAVE, via 100Ω)
                GND  4 ─┘   └─ 5  PB0 → Gate SI2312 (reset DURO compartido, via 100Ω)
```

| Pin | Función |
|-----|---------|
| PB0 | Reset DURO → MOSFET SI2312 → Relé HFD4/3 → corte de corriente (**ambos nodos**) |
| PB1 | Reset SUAVE → RST del nodo 1 |
| PB2 | Entrada watchdog ← señal de actividad radio del nodo 1 |
| PB3 | Entrada watchdog ← señal de actividad radio del nodo 2 |
| PB4 | Reset SUAVE → RST del nodo 2 |

> **Modo test (Fusion)**: se activa uniendo eléctricamente **PB1 y PB4** al arrancar el ATtiny (por ejemplo con un jumper entre ambos pines). El firmware lo detecta en el arranque mediante lectura con pull-up interno, de forma robusta ante pines flotantes.

---

## Hardware necesario

- **ATtiny13A** (DIP-8)
- **MOSFET SI2312** — N-Channel, controla la bobina del relé
- **Relé HFD4/3** — reed relay de baja corriente, corta la alimentación del/de los nodo(s)
- **Diodo 1N4148W** — en paralelo al relé, protección contra pico de tensión al desactivar
- **R** 10kΩ — serie en la(s) señal(es) de watchdog, protección ante transitorios (opcional, no imprescindible)
- **R** 100Ω — serie en gate SI2312
- **R** 100Ω — serie en cada línea RST hacia el/los MCU

> ⚠️ El ATtiny13A se alimenta directamente desde LiPo (3.0–4.2V). La señal de DIO1 del módulo de radio opera a 3.3V, compatible con el ATtiny en todo el rango de batería.

> ⚠️ **Importante — fuse BOD**: se recomienda flashear con **BOD (Brown-Out Detection) a 1.8V**. Al alimentar el ATtiny directamente de batería y compartir la misma alimentación con el relé de corte, la reconexión de corriente tras un reset duro puede provocar una caída momentánea de tensión (inrush de los condensadores del/los nodo/s). Sin BOD activo esto puede dejar al ATtiny en un estado inconsistente tras repetidos ciclos.

> Si no se desea usar el sistema watchdog, se puede **puentear mediante bypass soldado** en la PCB Albatastic PRO.

---

## Conexión (versión Fusion)

```
ATtiny PB0 → R 100Ω → Gate SI2312 → Relé HFD4/3 → Corte corriente (reset DURO, ambos nodos)
ATtiny PB1 → R 100Ω → RST Nodo 1                                    (reset SUAVE nodo 1)
ATtiny PB2 ← Watchdog Nodo 1 (DIO1 / señal actividad radio)         (watchdog nodo 1)
ATtiny PB3 ← Watchdog Nodo 2 (DIO1 / señal actividad radio)         (watchdog nodo 2)
ATtiny PB4 → R 100Ω → RST Nodo 2                                    (reset SUAVE nodo 2)
```

---

## Configuración

Todos los tiempos se ajustan por firmware (no hay jumpers de tiempo en la versión Fusion; el único jumper es el de modo test, PB1↔PB4):

```cpp
// --- Reset preventivo ---
#define PREVENTIVO_ACTIVO     1      // 1 = activo, 0 = desactivado
#define HORAS_PREVENTIVO      12     // Horas entre resets preventivos

// --- Tiempos watchdog (modo producción) ---
#define HORAS_SOFT_PRO        4      // Horas sin actividad → reset suave
#define MINUTOS_EXTRA_HARD    10     // Minutos extra tras soft → reset duro

// --- Tiempos modo test (PB1+PB4 unidos al arrancar) ---
#define SEGUNDOS_SOFT_TEST    40     // Segundos sin actividad → reset suave
#define SEGUNDOS_HARD_TEST    64     // Segundos sin actividad → reset duro
```

| Modo | Soft | Hard | Preventivo |
|------|------|------|------------|
| Producción (por defecto) | 4h | 4h10m | 12h (configurable, desactivable) |
| Test (PB1+PB4 unidos) | 40s | 64s | igual que producción |

> El reset suave se dispara **una sola vez por episodio de inactividad** por nodo; si el nodo se recupera, el contador se reinicia. Si no se recupera, a los pocos minutos se ejecuta el reset duro compartido.

---

## Características técnicas

- Consumo en sleep: **~0.11µA**
- Ciclo WDT interno: 8 segundos, usado como base de tiempo (independiente del reloj de sistema)
- Detección de actividad por cambio de estado en pines watchdog mediante **PCINT**, capturando pulsos incluso de corta duración durante el sleep
- Contadores en `uint16_t` (hasta ~145h por contador) para minimizar uso de flash
- Manejo de pines por registros directos (`DDRB`/`PORTB`/`PINB`) en vez de funciones Arduino, para caber en el 1KB de flash del ATtiny13A
- Detección de modo test robusta frente a pines flotantes (pull-up interno forzado)
- Compatible con SX1262, LR1121 (EBYTE E80) y cualquier radio con pin de actividad tipo DIO1
- Compilado con **Arduino IDE + MicroCore** (BOD 1.8V, sin bootloader, sin millis/micros)

---

## Firmware

El firmware está escrito en C para ATtiny13A con avr-gcc (compatible con Arduino IDE + MicroCore).

Existen dos variantes en el repositorio:

- `firmware/estandar/` — 1 nodo + jumpers OPCION1/OPCION2 para timeout preventivo
- `firmware/fusion/` — 2 nodos, relé de corte compartido, reset suave independiente por nodo

Todos los tiempos y opciones se configuran al inicio del archivo correspondiente.

---

## Integración con Albatastic PRO

Este watchdog está diseñado específicamente para la PCB **Albatastic PRO v1.2**.  
Más información sobre la PCB: [PCB-Albatastic-PRO](https://github.com/EmilioAL-Git/PCB-Albatastic-PRO)

---

## Autor

**Diseñado por**: [@Sremylio](https://telegram.me/sremylio) para MESHTASTIC ALBACETE

## 📜 Licencia

**Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0)**

Puedes usar, modificar y compartir este proyecto siempre que reconozcas al autor original y **no lo uses con fines comerciales**.

Más información: https://creativecommons.org/licenses/by-nc/4.0/

---
---

# 🇬🇧 ENGLISH VERSION

# Albatastic PRO Watchdog

> ⚖️ **License: CC BY-NC 4.0 — Non-commercial use only**  
> This design is free to use and modify, **but any commercial use is prohibited** without express authorization from the author.

---

## Description

Smart watchdog system for Meshtastic/MeshCore nodes based on the **ATtiny13A**. Designed to integrate with the **Albatastic PRO v1.2** PCB, it monitors radio module activity and triggers an automatic reset if the node is detected as frozen.

Unlike a fixed-timer watchdog, this system **detects real activity** on the radio module bus. As long as the node is working, it will never reset unnecessarily.

It is also available as a **standalone module** that can be added to existing nodes, without requiring the Albatastic PRO PCB.

<div align="center">
  <img src="images/WatchdogPCB.jpeg" width="60%" />
</div>

---

## 🆕 "Fusion" version — Dual-node watchdog

Starting from this revision, the firmware includes a **Fusion** variant able to watch **two independent nodes** (e.g. a Meshtastic node and a MeshCore node) with a single ATtiny13A, sharing the power-cut relay but with an independent soft reset per node.

- Each node has its own soft-reset (RST) pin and its own watchdog input pin.
- **Hard reset (power cut) is shared** — if either node hangs without recovering, both are power-cycled together, since they share the same supply through the same relay.
- Test mode is activated by **bridging the two soft-reset pins together** at boot (see pinout below), instead of using dedicated jumpers — this frees up pins to monitor the second node.

> The Fusion version is fully compatible with nodes using SX1262, LR1121 (EBYTE E80), or any radio chip that exposes a DIO1-like activity signal.

---

## How it works

The ATtiny13A monitors the **radio module activity pin** (DIO1 on SX1262/LR1121, or equivalent signal) of each node. This pin changes state with every TX/RX operation. If it stops changing for too long, the corresponding node is considered frozen and the system acts in **two levels**:

| Level | Condition | Action |
|-------|-----------|--------|
| **Soft** | No activity on that node for X hours (configurable) | RST pulse on that MCU's reset pin |
| **Hard** | No recovery after soft reset (+10 min) | Power cut via relay (affects both nodes on the Fusion version) |
| **Preventive** | Configurable max timeout (can be enabled/disabled) | Hard reset even if activity is detected |

> On the **standard version**, the preventive reset is enabled/disabled with the **OPTION2** jumper (see pinout below); its interval (`HORAS_PREVENTIVO`) is set in firmware. On the **Fusion version**, since there are no timing jumpers, it is enabled/disabled directly in firmware with `PREVENTIVO_ACTIVO`.

> 💡 **Note on mesh traffic**: Due to how Meshtastic/MeshCore-type networks work, it is unlikely that a node will go several hours without receiving or forwarding at least one message if there is traffic in the area. If the node is going to be deployed in a low or no-traffic location, it is recommended to increase the detection time (`HORAS_SOFT_PRO`) to avoid unnecessary resets. If everything is working correctly and there is network traffic, the watchdog should never trigger.

---

## ATtiny13A Pinout

### Standard version (1 node + configuration jumpers)

```
              RST  1 ─┐   ┌─ 8  VCC (direct VBAT, LiPo 3.0–4.2V)
        OPTION2 PB3  2 ─┤   ├─ 7  PB2 ← DIO1 radio module (watchdog)
        OPTION1 PB4  3 ─┤   ├─ 6  PB1 → RST MCU (soft reset, via 100Ω)
              GND  4 ─┘   └─ 5  PB0 → Gate SI2312 (hard reset, via 100Ω)
```

| Pin | Function |
|-----|---------|
| PB0 | Hard reset → MOSFET SI2312 → Relay HFD4/3 → power cut |
| PB1 | Soft reset → MCU RST pin |
| PB2 | Watchdog input ← radio module DIO1 |
| PB3 | Jumper OPTION2 → **enables/disables the preventive reset** |
| PB4 | Jumper OPTION1 → test mode |

> **OPTION2**: open = preventive reset **disabled**. Bridged = preventive reset **enabled**, firing every `HORAS_PREVENTIVO` (12h by default, configurable in firmware).
>
> **OPTION1**: bridged = **test mode** (short timeouts, 40s/64s). Both jumpers are read using a forced internal pull-up, robust against floating pins.

### Fusion version (2 nodes)

```
              RST  1 ─┐   ┌─ 8  VCC (direct VBAT, LiPo 3.0–4.2V)
   WATCHDOG N2  PB3  2 ─┤   ├─ 7  PB2 ← WATCHDOG N1 (node 1 DIO1)
   RST NODE2    PB4  3 ─┤   ├─ 6  PB1 → RST NODE1 (soft reset, via 100Ω)
              GND  4 ─┘   └─ 5  PB0 → Gate SI2312 (shared hard reset, via 100Ω)
```

| Pin | Function |
|-----|---------|
| PB0 | Hard reset → MOSFET SI2312 → Relay HFD4/3 → power cut (**both nodes**) |
| PB1 | Soft reset → node 1 RST |
| PB2 | Watchdog input ← node 1 radio activity signal |
| PB3 | Watchdog input ← node 2 radio activity signal |
| PB4 | Soft reset → node 2 RST |

> **Test mode (Fusion)**: activated by electrically bridging **PB1 and PB4** at ATtiny boot (e.g. with a jumper between both pins). The firmware detects this at startup using an internal pull-up read, robust against floating pins.

---

## Required hardware

- **ATtiny13A** (DIP-8)
- **MOSFET SI2312** — N-Channel, drives the relay coil
- **Relay HFD4/3** — low-current reed relay, cuts power to the node(s)
- **Diode 1N4148W** — in parallel with the relay, flyback protection when relay turns off
- **R** 10kΩ — series on the watchdog signal(s), transient protection (optional, not strictly required)
- **R** 100Ω — series on SI2312 gate
- **R** 100Ω — series on each RST line to the MCU(s)

> ⚠️ The ATtiny13A is powered directly from LiPo (3.0–4.2V). The radio module's DIO1 signal operates at 3.3V, compatible with the ATtiny across the full battery range.

> ⚠️ **Important — BOD fuse**: it is recommended to flash with **BOD (Brown-Out Detection) at 1.8V**. Since the ATtiny is powered directly from the battery and shares the same supply with the power-cut relay, reconnecting power after a hard reset can cause a momentary voltage drop (inrush from the node's/nodes' capacitors). Without BOD enabled, this can leave the ATtiny in an inconsistent state after repeated cycles.

> If the watchdog system is not needed, it can be **bypassed with a solder bridge** on the Albatastic PRO PCB.

---

## Wiring (Fusion version)

```
ATtiny PB0 → R 100Ω → Gate SI2312 → Relay HFD4/3 → Power cut (hard reset, both nodes)
ATtiny PB1 → R 100Ω → Node 1 RST                                (soft reset node 1)
ATtiny PB2 ← Node 1 watchdog (DIO1 / radio activity signal)     (watchdog node 1)
ATtiny PB3 ← Node 2 watchdog (DIO1 / radio activity signal)     (watchdog node 2)
ATtiny PB4 → R 100Ω → Node 2 RST                                (soft reset node 2)
```

---

## Configuration

All timings are set in firmware (no timing jumpers on the Fusion version; the only jumper is test mode, PB1↔PB4):

```cpp
// --- Preventive reset ---
#define PREVENTIVO_ACTIVO     1      // 1 = enabled, 0 = disabled
#define HORAS_PREVENTIVO      12     // Hours between preventive resets

// --- Watchdog timings (production mode) ---
#define HORAS_SOFT_PRO        4      // Hours without activity → soft reset
#define MINUTOS_EXTRA_HARD    10     // Extra minutes after soft → hard reset

// --- Test mode timings (PB1+PB4 bridged at boot) ---
#define SEGUNDOS_SOFT_TEST    40     // Seconds without activity → soft reset
#define SEGUNDOS_HARD_TEST    64     // Seconds without activity → hard reset
```

| Mode | Soft | Hard | Preventive |
|------|------|------|------------|
| Production (default) | 4h | 4h10m | 12h (configurable, can be disabled) |
| Test (PB1+PB4 bridged) | 40s | 64s | same as production |

> The soft reset triggers **only once per inactivity episode** per node; if the node recovers, the counter resets. If it doesn't recover, the shared hard reset fires a few minutes later.

---

## Technical specs

- Sleep current: **~0.11µA**
- Internal WDT cycle: 8 seconds, used as time base (independent of system clock)
- Activity detection via state change on watchdog pins using **PCINT**, capturing even short pulses during sleep
- Counters use `uint16_t` (up to ~145h per counter) to minimize flash usage
- Pin handling via direct registers (`DDRB`/`PORTB`/`PINB`) instead of Arduino functions, to fit within the ATtiny13A's 1KB flash
- Test mode detection robust against floating pins (forced internal pull-up)
- Compatible with SX1262, LR1121 (EBYTE E80) and any radio with a DIO1-like activity pin
- Built with **Arduino IDE + MicroCore** (BOD 1.8V, no bootloader, no millis/micros)

---

## Firmware

Written in C for ATtiny13A with avr-gcc (compatible with Arduino IDE + MicroCore).

Two variants are available in the repository:

- `firmware/standard/` — 1 node + OPTION1/OPTION2 jumpers for preventive timeout
- `firmware/fusion/` — 2 nodes, shared power-cut relay, independent soft reset per node

All timings and options are configured at the top of the corresponding file.

---

## Integration with Albatastic PRO

This watchdog is specifically designed for the **Albatastic PRO v1.2** PCB.  
More information about the PCB: [PCB-Albatastic-PRO](https://github.com/EmilioAL-Git/PCB-Albatastic-PRO)

---

## Author

**Designed by**: [@Sremylio](https://telegram.me/sremylio) for MESHTASTIC ALBACETE

## 📜 License

**Creative Commons Attribution–NonCommercial 4.0 International (CC BY-NC 4.0)**

You may use, modify and share this project as long as you credit the original author and **do not use it for commercial purposes**.

More information: https://creativecommons.org/licenses/by-nc/4.0/
