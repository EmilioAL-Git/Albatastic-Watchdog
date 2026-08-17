#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// -------------------------------------------------------
// ALBATASTIC FUSION - MONITOREA 2 NODOS
// PINOUT ATTINY13A
// PB0 → MOSFET SI2312 → Relé HFD4/3 → corte corriente AMBOS nodos
// PB1 → R100Ω → RST ProMicro 1 (reset suave nodo 1)
// PB2 ← Watchdog señal nodo 1 (DIO1/SDA/SCK)
// PB3 ← Watchdog señal nodo 2 (DIO1/SDA/SCK)
// PB4 → R100Ω → RST ProMicro 2 (reset suave nodo 2)
// -------------------------------------------------------
// MODO TEST: cortocircuitar PB1 y PB4 al arrancar
// -------------------------------------------------------

// ═══════════════════════════════════════════════════════
// CONFIGURACIÓN
// ═══════════════════════════════════════════════════════

#define PREVENTIVO_ACTIVO     1
#define HORAS_PREVENTIVO      12

#define HORAS_SOFT_PRO        4
#define MINUTOS_EXTRA_HARD    10

#define SEGUNDOS_SOFT_TEST    40
#define SEGUNDOS_HARD_TEST    64

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
#define RSTPIN1     PB1
#define WATCHPIN1   PB2
#define WATCHPIN2   PB3
#define RSTPIN2     PB4

uint16_t ciclos_acumulados = 0;
uint16_t ciclos_sin_act_n1 = 0;
uint16_t ciclos_sin_act_n2 = 0;
uint16_t umbral_soft       = 0;
uint16_t umbral_hard       = 0;

uint8_t soft_hecho_n1 = 0;
uint8_t soft_hecho_n2 = 0;

volatile uint8_t act_n1 = 0;
volatile uint8_t act_n2 = 0;
volatile uint8_t wdt_flag = 0;
volatile uint8_t prev_pinb = 0;

ISR(PCINT0_vect) {
  uint8_t actual = PINB;
  if ((actual ^ prev_pinb) & _BV(WATCHPIN1)) act_n1 = 1;
  if ((actual ^ prev_pinb) & _BV(WATCHPIN2)) act_n2 = 1;
  prev_pinb = actual;
}

ISR(WDT_vect) {
  wdt_flag = 1;
}

// -------------------------------------------------------
// Delay compartido: un solo _delay_ms(1) inline, reutilizado
// -------------------------------------------------------
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

void reset_suave(uint8_t pin) {
  wdt_disable();
  DDRB  |= _BV(pin);
  PORTB &= ~_BV(pin);
  delay_ms(200);
  PORTB |= _BV(pin);
  delay_ms(50);
  DDRB  &= ~_BV(pin);
  PORTB &= ~_BV(pin);
  setup_wdt();
}

void reset_duro() {
  reset_suave(RSTPIN1);
  reset_suave(RSTPIN2);

  wdt_disable();
  delay_ms(1000);

  PORTB |= _BV(RESETPIN);
  delay_ms(500);
  PORTB &= ~_BV(RESETPIN);

  setup_wdt();
}

uint8_t detectar_modo_test() {
  DDRB  |= _BV(RSTPIN1);
  PORTB &= ~_BV(RSTPIN1);

  DDRB  &= ~_BV(RSTPIN2);
  PORTB |= _BV(RSTPIN2);

  delay_ms(10);

  uint8_t test = !(PINB & _BV(RSTPIN2));

  DDRB  &= ~_BV(RSTPIN1);
  PORTB &= ~_BV(RSTPIN2);

  return test;
}

// -------------------------------------------------------
void setup() {
  DDRB  |= _BV(RESETPIN);
  PORTB &= ~_BV(RESETPIN);

  DDRB  &= ~(_BV(RSTPIN1) | _BV(RSTPIN2) | _BV(WATCHPIN1) | _BV(WATCHPIN2));
  PORTB &= ~(_BV(RSTPIN1) | _BV(RSTPIN2) | _BV(WATCHPIN1) | _BV(WATCHPIN2));

  PCMSK |= _BV(PCINT2) | _BV(PCINT3);

  prev_pinb = PINB;

  if (detectar_modo_test()) {
    umbral_soft = CICLOS_SOFT_TEST;
    umbral_hard = CICLOS_HARD_TEST;
  } else {
    umbral_soft = CICLOS_SOFT_PRO;
    umbral_hard = CICLOS_HARD_PRO;
  }

  sei();
  setup_wdt();
}

// -------------------------------------------------------
void loop() {
  sleep_wdt();

  if (!wdt_flag) {
    if (act_n1) { act_n1 = 0; ciclos_sin_act_n1 = 0; }
    if (act_n2) { act_n2 = 0; ciclos_sin_act_n2 = 0; }
    return;
  }
  wdt_flag = 0;

  if (act_n1) { act_n1 = 0; ciclos_sin_act_n1 = 0; }
  else ciclos_sin_act_n1++;

  if (act_n2) { act_n2 = 0; ciclos_sin_act_n2 = 0; }
  else ciclos_sin_act_n2++;

  ciclos_acumulados++;

  if (ciclos_sin_act_n1 >= umbral_soft && !soft_hecho_n1) {
    reset_suave(RSTPIN1);
    soft_hecho_n1 = 1;
  }

  if (ciclos_sin_act_n2 >= umbral_soft && !soft_hecho_n2) {
    reset_suave(RSTPIN2);
    soft_hecho_n2 = 1;
  }

  if (ciclos_sin_act_n1 >= umbral_hard || ciclos_sin_act_n2 >= umbral_hard) {
    ciclos_sin_act_n1 = 0;
    ciclos_sin_act_n2 = 0;
    ciclos_acumulados = 0;
    soft_hecho_n1 = 0;
    soft_hecho_n2 = 0;
    reset_duro();
  }

#if PREVENTIVO_ACTIVO
  if (ciclos_acumulados >= CICLOS_PREVENTIVO) {
    ciclos_acumulados = 0;
    ciclos_sin_act_n1 = 0;
    ciclos_sin_act_n2 = 0;
    soft_hecho_n1 = 0;
    soft_hecho_n2 = 0;
    reset_duro();
  }
#endif
}