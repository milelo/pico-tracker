## SAM D20 Peripherals usage

| Type | Peripheral | Function | Notes
| --- | --- | --- | ---
|*GLCK*|
||gclk0|main clock, internal osc8m|4 MHz
||gclk1|tcxo clock, fed from xosc OR osc8m
||gclk7|aprs clock, fed from gclk1, div 6 / 11
|
|*TC*||
||tc0|telemetry tick timer. 32-bit. glck1
||tc1|^^^^^
||tc2|counts cycles of tcxo. 32-bit. gclk1
||tc3|^^^^^
||tc4|unused (osc8m event source)
||tc5|telemetry pwm 16-bit glck0, ALSO aprs carrier 16-bit gclk7
|
|*EXTINT*|
||extint[5]|gps timepulse
|
|*event channels*|
||0|event source for timer 2 xosc measurement
||1|tc4 retrigger
|
|*SERCOM*||
||sercom0|spi flash
||sercom1|ublox gps
||sercom2|
||sercom3|radio|currently bitbanged as required pin layout broken in sercom

## SAM D20 Interrupts usage

| Name | Function | Priority H(0-3)L | Notes
| --- | --- | --- | ---
|TC0_IRQn|telemetry tick timer|1|latency critical for symbol timing. rate <= 1200Hz
|[GPS_SERCOM]_IRQn|gps usart rx|2|latency not so critical. rate <= 960Hz
|EIC_IRQn|timepulse|3|latency not so critical. rate = 1
|TC2_IRQn|xosc measurement done|3|latency not critical
|ADC_IRQn|adc measurement done|3|latency not critical



## Clock Layout

```
[osc8m] --> [glck0] +
                    |--> [tc5]
                    |--> [adc]
                    |--> [extint]
                    |--> [cm0+, apbs etc.]


                     |\
         [osc8m]--> 0| |
                     | | --> [glck1] +--> [tc0, telemetry tick]
tcxo --> [xosc] --> 1| |             |--> [tc2, count tcxo] <-- gps timepulse
                     |/              |--> [glck7] --> [tc5] --> si_gpio1
                      |              |--> [gps usart]
                 *USE_XOSC*


[osculp32k] --> [gclk4] --> [wdt]
```
