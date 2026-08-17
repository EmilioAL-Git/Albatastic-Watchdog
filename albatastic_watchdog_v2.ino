#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// -------------------------------------------------------
// PINOUT ATTINY13A
// PB0 → pcbEN → R100Ω → Gate SI2312 → reset DURO
// PB1 → R100Ω → RST ProMicro → reset SUAVE
// PB2 ← DIO1 SX1262/LR1121 → watchdog actividad LoRa
// PB3 ← Jumper OPCION2 → activa/desactiva reset preventivo
// PB4 ← Jumper OPCION1 → modo test
// -------------------------------------------------------
// OPCION2 abierto  → preventivo DESACTIVADO
// OPCION2 soldado  → preventivo ACTIVADO (cada HORAS_PREVENTIVO)
// -------------------------------------------------------

// ═══════════════════════════════════════════════════════
// CONFIGURACIÓN
// ═══════════════════════════════════════════════════════

#define HORAS_PREVENTIVO      12     // Horas entre resets preventivos (si OPCION2 activo)

#define HORAS_SOFT_PRO        4      // Horas sin DIO1 → reset suave
#define MINUTOS_EXTRA_HARD    10     // Minutos extra tras soft → reset duro

#define SEGUNDOS_SOFT_TEST    40     // Modo test → reset suave
#define SEGUNDOS_HARD_TEST    64     // Modo test → reset duro

// ═══════════════════════════════════════════════════════
// NO TOCAR
// ═══════════════════════════════════════════════════════

#define WDT_SEG               8UL
#define H_A_CICLOS(h)         ((uint16_t)((h) * 3600UL / WDT_SEG))
#define M_A_CICLOS(m)         ((uint16_t)((m) * 60UL  / WDT_SEG))
#define S_A_CICLOS(s)         ((uint16_t)((s) / WDT_SEG))

#define CICLOS_SOFT_PRO       H_A_CICLOS(HORAS_SOFT_PRO)
#define CICLOS_HARD_PRO       (CICLOS_SOFT_PRO + M_A_CICLOS(MINUTOS_EXTRA_HARD))
#define CICLOS_SOFT_TEST      S_A_CICLOS(SEGUNDOS_SOFT_TEST)
#define CICLOS_HARD_TEST      S_A_CICLOS(SEGUNDOS_HARD_TEST)
#define CICLOS_PREVENTIVO     H_A_CICLOS(HORAS_PREVENTIVO)

#define RESETPIN    PB0
#define RSTPIN      PB1
#define WATCHPIN    PB2
#define OPCION2     PB3
#define OPCION1     PB4

uint16_t ciclos_acumulados    = 0;
uint16_t ciclos_sin_actividad = 0;
uint16_t umbral_soft          = 0;
uint16_t umbral_hard          = 0;
uint8_t  preventivo_activo    = 0;
uint8_t  soft_hecho           = 0;

volatile uint8_t actividad_detectada = 0;
volatile uint8_t wdt_flag            = 0;

ISR(PCINT0_vect) {
  actividad_detectada = 1;
}

ISR(WDT_vect) {
  wdt_flag = 1;
}

void delay_ms(uint16_t ms) {
  while (ms--) _delay_ms(1);
}

// -------------------------------------------------------
void setup_wdt() {
  cli();
  wdt_reset();
  MCUSR &= ~_BV(WDRF);
  WDTCR |= _BV(WDCE) | _BV(WDE);
  WDTCR = _BV(WDTIE) | _BV(WDP3) | _BV(WDP0); // 8s
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

// -------------------------------------------------------
void reset_suave() {
  wdt_disable();
  DDRB  |= _BV(RSTPIN);
  PORTB &= ~_BV(RSTPIN);
  delay_ms(200);
  PORTB |= _BV(RSTPIN);
  delay_ms(50);
  DDRB  &= ~_BV(RSTPIN);
  PORTB &= ~_BV(RSTPIN);
  setup_wdt();
}

void reset_duro() {
  reset_suave();
  wdt_disable();
  delay_ms(1000);
  PORTB |= _BV(RESETPIN);
  delay_ms(500);
  PORTB &= ~_BV(RESETPIN);
  setup_wdt();
}

// -------------------------------------------------------
// SETUP
// -------------------------------------------------------
void setup() {
  DDRB  |= _BV(RESETPIN);
  PORTB &= ~_BV(RESETPIN);

  DDRB  &= ~_BV(RSTPIN);
  PORTB &= ~_BV(RSTPIN);

  DDRB  &= ~_BV(WATCHPIN);
  PORTB &= ~_BV(WATCHPIN);
  PCMSK |= _BV(PCINT2);

  // OPCION1: modo test — pullup forzado
  DDRB  &= ~_BV(OPCION1);
  PORTB |= _BV(OPCION1);

  // OPCION2: activa/desactiva preventivo — pullup forzado
  DDRB  &= ~_BV(OPCION2);
  PORTB |= _BV(OPCION2);

  delay_ms(10);

  if (!(PINB & _BV(OPCION1))) {
    umbral_soft = CICLOS_SOFT_TEST;
    umbral_hard = CICLOS_HARD_TEST;
  } else {
    umbral_soft = CICLOS_SOFT_PRO;
    umbral_hard = CICLOS_HARD_PRO;
  }

  // OPCION2 soldado (LOW) → preventivo activo
  preventivo_activo = !(PINB & _BV(OPCION2));

  PORTB &= ~_BV(OPCION1);   // quitar pullups tras leer
  PORTB &= ~_BV(OPCION2);

  sei();
  setup_wdt();
}

// -------------------------------------------------------
// LOOP
// -------------------------------------------------------
void loop() {
  sleep_wdt();

  if (!wdt_flag) {
    if (actividad_detectada) {
      actividad_detectada  = 0;
      ciclos_sin_actividad = 0;
    }
    return;
  }
  wdt_flag = 0;

  if (actividad_detectada) {
    actividad_detectada  = 0;
    ciclos_sin_actividad = 0;
  } else {
    ciclos_sin_actividad++;
  }

  ciclos_acumulados++;

  // Nivel 1: reset suave (una vez por episodio)
  if (ciclos_sin_actividad >= umbral_soft && !soft_hecho) {
    reset_suave();
    soft_hecho = 1;
  }

  // Nivel 2: reset duro
  if (ciclos_sin_actividad >= umbral_hard) {
    ciclos_sin_actividad = 0;
    ciclos_acumulados    = 0;
    soft_hecho           = 0;
    reset_duro();
  }

  // Nivel 3: reset preventivo (solo si OPCION2 activo)
  if (preventivo_activo && ciclos_acumulados >= CICLOS_PREVENTIVO) {
    ciclos_acumulados    = 0;
    ciclos_sin_actividad = 0;
    soft_hecho           = 0;
    reset_duro();
  }
}
