#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// -------------------------------------------------------
// PINOUT ATTINY13A
// PB0 → pcbEN → R100Ω → Gate SI2312 → reset DURO
// PB1 → R100Ω → RST ProMicro → reset SUAVE
// PB2 ← DIO1 SX1262/LR1121 → watchdog actividad LoRa
// PB4 ← Jumper OPCION1 → modo test
// -------------------------------------------------------

// ═══════════════════════════════════════════════════════
// CONFIGURACIÓN — ajusta aquí todos los parámetros
// ═══════════════════════════════════════════════════════

// --- Reset preventivo ---
#define PREVENTIVO_ACTIVO   false     // true = activo, false = desactivado
#define HORAS_PREVENTIVO    12        // Horas entre resets preventivos (si activo)

// --- Tiempos de watchdog actividad ---
#define HORAS_SOFT_PRO      4         // Horas sin DIO1 → reset suave
#define MINUTOS_EXTRA_HARD  10        // Minutos extra tras soft → reset duro

// --- Tiempos modo test (jumper OPCION1 soldado) ---
#define SEGUNDOS_SOFT_TEST  40        // Segundos sin DIO1 → reset suave
#define SEGUNDOS_HARD_TEST  64        // Segundos sin DIO1 → reset duro

// ═══════════════════════════════════════════════════════
// NO TOCAR A PARTIR DE AQUÍ
// ═══════════════════════════════════════════════════════

#define WDT_SEG             8UL

#define H_A_CICLOS(h)       ((h) * 3600UL / WDT_SEG)
#define M_A_CICLOS(m)       ((m) * 60UL  / WDT_SEG)
#define S_A_CICLOS(s)       ((s) / WDT_SEG)

#define CICLOS_SOFT_PRO     H_A_CICLOS(HORAS_SOFT_PRO)
#define CICLOS_HARD_PRO     (CICLOS_SOFT_PRO + M_A_CICLOS(MINUTOS_EXTRA_HARD))
#define CICLOS_SOFT_TEST    S_A_CICLOS(SEGUNDOS_SOFT_TEST)
#define CICLOS_HARD_TEST    S_A_CICLOS(SEGUNDOS_HARD_TEST)
#define CICLOS_PREVENTIVO   H_A_CICLOS(HORAS_PREVENTIVO)

#define RESETPIN    PB0
#define RSTPIN      PB1
#define WATCHPIN    PB2
#define OPCION1     PB4

unsigned long ciclos_acumulados    = 0;
unsigned long ciclos_sin_actividad = 0;
unsigned long umbral_soft          = 0;
unsigned long umbral_hard          = 0;

volatile uint8_t actividad_detectada = 0;

ISR(PCINT0_vect) {
  actividad_detectada = 1;
}

ISR(WDT_vect) {}

void setup_wdt() {
  cli();
  wdt_reset();
  MCUSR &= ~_BV(WDRF);
  WDTCR |= _BV(WDCE) | _BV(WDE);
  WDTCR = _BV(WDTIE) | _BV(WDP3) | _BV(WDP0);
  sei();
}

void sleep_wdt() {
  GIFR  |= _BV(PCIF);
  GIMSK |= _BV(PCIE);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();
  sleep_disable();
  GIMSK &= ~_BV(PCIE);
}

void reset_suave() {
  wdt_disable();
  pinMode(RSTPIN, OUTPUT);
  digitalWrite(RSTPIN, LOW);
  delay(200);
  digitalWrite(RSTPIN, HIGH);
  delay(50);
  pinMode(RSTPIN, INPUT);
  setup_wdt();
}

void reset_duro() {
  wdt_disable();
  reset_suave();
  delay(1000);
  digitalWrite(RESETPIN, HIGH);
  delay(500);
  digitalWrite(RESETPIN, LOW);
  setup_wdt();
}

void setup() {
  pinMode(RESETPIN, OUTPUT);
  digitalWrite(RESETPIN, LOW);

  pinMode(RSTPIN, INPUT);

  pinMode(WATCHPIN, INPUT);
  PCMSK |= _BV(PCINT2);

  pinMode(OPCION1, INPUT);
  digitalWrite(OPCION1, HIGH);
  delay(10);

  if (digitalRead(OPCION1) == LOW) {
    umbral_soft = CICLOS_SOFT_TEST;
    umbral_hard = CICLOS_HARD_TEST;
  } else {
    umbral_soft = CICLOS_SOFT_PRO;
    umbral_hard = CICLOS_HARD_PRO;
  }

  sei();
  setup_wdt();
}

void loop() {
  sleep_wdt();

  if (actividad_detectada) {
    actividad_detectada  = 0;
    ciclos_sin_actividad = 0;
  } else {
    ciclos_sin_actividad++;
  }

  ciclos_acumulados++;

  // Nivel 1: reset suave por inactividad
  if (ciclos_sin_actividad == umbral_soft) {
    reset_suave();
  }

  // Nivel 2: reset duro por inactividad
  if (ciclos_sin_actividad >= umbral_hard) {
    reset_duro();
    ciclos_sin_actividad = 0;
    ciclos_acumulados    = 0;
  }

  // Nivel 3: reset preventivo (si activo)
#if PREVENTIVO_ACTIVO
  if (ciclos_acumulados >= CICLOS_PREVENTIVO) {
    reset_duro();
    ciclos_acumulados    = 0;
    ciclos_sin_actividad = 0;
  }
#endif
}