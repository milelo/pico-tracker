/*
 * Telemetry strings and formatting
 * Copyright (C) 2014  Richard Meadows <richardeoin>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>

#include "samd20.h"
#include "telemetry.h"
#include "rtty.h"
#include "contestia.h"
#include "rsid.h"
#include "pips.h"
#include "si_trx.h"
#include "si_trx_defs.h"
#include "system/gclk.h"
#include "system/interrupt.h"
#include "system/pinmux.h"
#include "tc/tc_driver.h"
#include "hw_config.h"


/**
 * CYCLIC REDUNDANCY CHECK (CRC)
 * =============================================================================
 */

/**
 * CRC Function for the XMODEM protocol.
 * http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html#gaca726c22a1900f9bad52594c8846115f
 */
uint16_t crc_xmodem_update(uint16_t crc, uint8_t data)
{
  int i;

  crc = crc ^ ((uint16_t)data << 8);
  for (i = 0; i < 8; i++) {
    if (crc & 0x8000) {
      crc = (crc << 1) ^ 0x1021;
    } else {
      crc <<= 1;
    }
  }

  return crc;
}

/**
 * Calcuates the CRC checksum for a communications string
 * See http://ukhas.org.uk/communication:protocol
 */
uint16_t crc_checksum(char *string)
{
  size_t i;
  uint16_t crc;
  uint8_t c;

  crc = 0xFFFF;

  for (i = 0; i < strlen(string); i++) {
    c = string[i];
    crc = crc_xmodem_update(crc, c);
  }

  return crc;
}


/**
 * TELEMETRY OUTPUT
 * =============================================================================
 */

/**
 * The type of telemetry we're currently outputting
 */
enum telemetry_t telemetry_type;
/**
 * Current output
 */
int32_t telemetry_string_length = 0;
/**
 * Where we are in the current output
 */
int32_t telemetry_index;
/**
 * Is the radio currently on?
 */
uint8_t radio_on = 0;


/**
 * Returns 1 if we're currently outputting.
 */
int telemetry_active(void) {
  return (telemetry_string_length > 0);
}

/**
 * Starts telemetry output
 *
 * Returns 0 on success, 1 if already active
 */
int telemetry_start(enum telemetry_t type, int32_t length) {
  if (!telemetry_active()) {

    /* Initialise */
    telemetry_type = type;
    telemetry_index = 0;
    telemetry_string_length = length;

    /* Setup timer tick */
    switch(telemetry_type) {
    case TELEMETRY_CONTESTIA:
      timer0_tick_init(CONTESTIA_SYMBOL_RATE);
      break;
    case TELEMETRY_RTTY:
      timer0_tick_init(RTTY_BITRATE);
      break;
    case TELEMETRY_PIPS:
      timer0_tick_init(PIPS_OFF_FREQUENCY);
      break;
    case TELEMETRY_APRS:
    case TELEMETRY_RSID: /* Not used - see function below */
      break;
    }
    return 0; /* Success */
  } else {
    return 1; /* Already active */
  }
}
/**
 * Start RSID output. Argument: RSID Data
 *
 * Returns 0 on success, 1 if already active
 */
int telemetry_start_rsid(rsid_code_t rsid) {
  if (!telemetry_active()) {

    /* Initialise */
    telemetry_type = TELEMETRY_RSID;
    telemetry_index = 0;
    telemetry_string_length = 5+1+5;

    /* Start RSID */
    rsid_start(rsid);

    /* Setup timer tick */
    timer0_tick_init(RSID_SYMBOL_RATE);

    return 0; /* Success */
  } else {
    return 1; /* Already active */
  }
}




uint8_t is_telemetry_finished(void) {
  if (telemetry_index >= telemetry_string_length) {
    /* All done, deactivate */
    telemetry_string_length = 0;

    /* Turn radio off */
    if (radio_on) {
      si_trx_off(); radio_on = 0;
    }

    /* De-init timer */
    timer0_tick_deinit();

    return 1;
  }
  return 0;
}
/**
 * Called at the telemetry mode's baud rate
 */
void telemetry_tick(void) {
  if (telemetry_active()) {
    switch (telemetry_type) {
    case TELEMETRY_CONTESTIA: /* ---- ---- A block mode */

      if (!radio_on) {
        /* Contestia: We use the modem offset to modulate */
        si_trx_on(SI_MODEM_MOD_TYPE_CW, 1);
        radio_on = 1;
        contestia_preamble();
      }

      if (!contestia_tick()) {
        /* Transmission Finished */
        if (is_telemetry_finished()) return;

        /* Let's start again */
        char* block = &telemetry_string[telemetry_index];
        telemetry_index += CONTESTIA_CHARACTERS_PER_BLOCK;

        contestia_start(block);
      }

      break;
    case TELEMETRY_RTTY: /* ---- ---- A character mode */

      if (!radio_on) {
        /* RTTY: We use the modem offset to modulate */
        si_trx_on(SI_MODEM_MOD_TYPE_CW, 1);
        radio_on = 1;
        rtty_preamble();
      }

      if (!rtty_tick()) {
        /* Transmission Finished */
        if (is_telemetry_finished()) return;

        /* Let's start again */
        uint8_t data = telemetry_string[telemetry_index];
        telemetry_index++;

        rtty_start(data);
      }

      break;

    case TELEMETRY_RSID: /* ---- ---- A block mode */

      /* Wait for 5 bit times of silence before and after */
      if (telemetry_index != 5) {
        is_telemetry_finished();
        telemetry_index++;
        return;
      }

      if (!radio_on) {
        /* RSID: We PWM frequencies with the external pin */
        si_trx_on(SI_MODEM_MOD_TYPE_2FSK, 1);
        telemetry_gpio1_pwm_init();

        radio_on = 1;
      }

      /* Do Tx */
      if (!rsid_tick()) {
        /* Force transmission finished */
        telemetry_index++;
        si_trx_off(); radio_on = 0;
        telemetry_gpio1_pwm_deinit();
        return;
      }
      break;

    case TELEMETRY_PIPS: /* ---- ---- A pips mode! */

      if (!radio_on) { /* Turn on */
        /* Pips: Cw */
        si_trx_on(SI_MODEM_MOD_TYPE_CW, 1); radio_on = 1;
        timer0_tick_frequency(PIPS_ON_FREQUENCY);

      } else { /* Turn off */
        si_trx_off(); radio_on = 0;
        timer0_tick_frequency(PIPS_OFF_FREQUENCY);

        telemetry_index++;
        if (is_telemetry_finished()) return;
      }
    }
  }
}


/**
 * CLOCKING
 * =============================================================================
 */

/**
 * Initialises a timer interupt at the given frequency
 *
 * Returns the frequency we actually initialised.
 */
float timer0_tick_init(float frequency)
{
  /* Calculate the wrap value for the given frequency */
  float gclk_frequency = (float)system_gclk_chan_get_hz(0);
  uint32_t count = (uint32_t)(gclk_frequency / frequency);

  /* Configure Timer 0 */
  bool t0_capture_channel_enables[]    = {false, false};
  uint32_t t0_compare_channel_values[] = {count, 0x0000};
  tc_init(TC0,
//	  GCLK_GENERATOR_3,
          GCLK_GENERATOR_0,
	  TC_COUNTER_SIZE_32BIT,
	  TC_CLOCK_PRESCALER_DIV1,
	  TC_WAVE_GENERATION_MATCH_FREQ,
	  TC_RELOAD_ACTION_GCLK,
	  TC_COUNT_DIRECTION_UP,
	  TC_WAVEFORM_INVERT_OUTPUT_NONE,
	  false,			/* Oneshot  */
	  true,				/* Run in standby */
	  0x0000,			/* Initial value */
	  0x0000,			/* Top value */
	  t0_capture_channel_enables,	/* Capture Channel Enables */
	  t0_compare_channel_values);	/* Compare Channels Values */

  /* Enable Events */
  struct tc_events event;
  memset(&event, 0, sizeof(struct tc_events));
  event.generate_event_on_compare_channel[0] = true;
  event.event_action = TC_EVENT_ACTION_RETRIGGER;
  tc_enable_events(TC0, &event);

  /* Enable Interrupt */
  TC0->COUNT32.INTENSET.reg = (1 << 4);
  irq_register_handler(TC0_IRQn, 0); /* Highest Priority */

  /* Enable Timer */
  tc_enable(TC0);
  tc_start_counter(TC0);

  /* Return the frequency we actually initialised */
  return gclk_frequency / (float)count;
}
/**
 * Changes the timer0 frequency
 */
void timer0_tick_frequency(float frequency)
{
  float gclk_frequency = (float)system_gclk_chan_get_hz(0);
  uint32_t count = (uint32_t)(gclk_frequency / frequency);

  tc_set_compare_value(TC0,
                       TC_COMPARE_CAPTURE_CHANNEL_0,
                       count);
  tc_set_count_value(TC0, 0);
}
/**
 * Disables the timer
 */
void timer0_tick_deinit()
{
  tc_stop_counter(TC0);
  tc_disable(TC0);
}
/**
 * Called at the symbol rate
 */
void TC0_Handler(void)
{
  if (tc_get_status(TC0) & TC_STATUS_CHANNEL_0_MATCH) {
    tc_clear_status(TC0, TC_STATUS_CHANNEL_0_MATCH);

    telemetry_tick();
  }
}



#define GPIO1_PWM_STEPS 200 // ~ 20kHz on a 4 MHz clock

/**
 * Initialised PWM at the given duty cycle on the GPIO1 pin of the radio
 */
void telemetry_gpio1_pwm_init(void)
{
  bool capture_channel_enables[]    = {false, true};
  uint32_t compare_channel_values[] = {0x0000, 0x0000}; // Set duty cycle at 0% by default

  //float gclk_frequency = (float)system_gclk_chan_get_hz(0);

  tc_init(TC5,
	  GCLK_GENERATOR_0,
	  TC_COUNTER_SIZE_8BIT,
	  TC_CLOCK_PRESCALER_DIV1,
	  TC_WAVE_GENERATION_NORMAL_PWM,
	  TC_RELOAD_ACTION_GCLK,
	  TC_COUNT_DIRECTION_UP,
	  TC_WAVEFORM_INVERT_OUTPUT_NONE,
	  false,			/* Oneshot = false */
	  false,			/* Run in standby = false */
	  0x0000,			/* Initial value */
	  GPIO1_PWM_STEPS,		/* Top value */
	  capture_channel_enables,	/* Capture Channel Enables */
	  compare_channel_values);	/* Compare Channels Values */


  /* Enable the output pin */
  system_pinmux_pin_set_config(SI406X_GPIO1_PINMUX >> 16,	/* GPIO Pin	*/
 			       SI406X_GPIO1_PINMUX & 0xFFFF,	/* Mux Position */
 			       SYSTEM_PINMUX_PIN_DIR_INPUT,	/* Direction	*/
 			       SYSTEM_PINMUX_PIN_PULL_NONE,	/* Pull		*/
 			       false);    			/* Powersave	*/

  tc_enable(TC5);
  tc_start_counter(TC5);

}
/**
 * Sets duty cycle on PWM pin
 */
void telemetry_gpio1_pwm_duty(float duty_cycle)
{
  uint32_t compare_value = (float)GPIO1_PWM_STEPS * duty_cycle;

  tc_set_compare_value(TC5,
                       TC_COMPARE_CAPTURE_CHANNEL_1,
                       compare_value);
}
/**
 * Turn the pwm off again
 */
void telemetry_gpio1_pwm_deinit(void)
{
  tc_stop_counter(TC5);
  tc_enable(TC5);
}
