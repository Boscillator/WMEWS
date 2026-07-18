# V 1. 9

### Shenzhen Mingzhan Information Technology Co., Ltd.


## Table of contents

- I. Overview..........................................................................................................................................................................
   - 1 Resource
   - 2 Function
   - 3 Custom firmware pin arrangement
- II. Pin Definitions................................................................................................................................................................
- III. Register Mapping
- IV. Key Register Description
   - 1 System Registers
   - 2 GPIO Register
   - 3 ADC Register
   - 4 PWM Control Register
   - 5 Systerm Timer
   - 6 Interrupt and Wake-up Control
   - 7 Button Configuration
   - 8 NeoPixel Control Module
   - 9 AW8737A PULSE........................................................................................................................................................................................................
   - 10 NEO Buffer
   - 11 RTC Buffer
- V. Additional Function Description
   - 1 ADC
   - 2 PWM Output
   - 3 PWR_BTN Button
   - 4 LED Indicator
   - 5 Low Voltage Protection
   - 6 I2C Idle Sleep
   - 7 Interrupt Wake-up
   - 8 IRQ Handling
- VI. Use Examples
   - 1 GPIO Wake-up
   - 2 RGB
   - 3 ADC
   - 4 PWM
   - 5 TIM
- Appendix


#### 1 / 33

## I. Overview..........................................................................................................................................................................

```
M5PM1 is a power management IC programmed with M5Stack custom power management firmware, designed to provide
```
fixed implementations of power port control, charging control, timed wake-up, and other power management functions.

### 1 Resource

```
(1) 5 multiplexed GPIOs
(2) 1 set of I^2 C^ interfaces
(3) 32 - bit timer
(4) 32-byte RAM protected area
```
### 2 Function

```
(1) 5 GPIOs with extended functions :
2 channels multiplexed as 12-bit ADC
2 channels multiplexed as PWM
1 channel multiplexed for LED control (RGB565)
(2) GPIO supports programmable pull-up/down resistors, open-drain / push-pull output, and interrupt polarity control
(3) Supports reading the built-in temperature sensor and internal reference voltage
(4) I^2 C interface supports 100 kHz (default) / 400 kHz modes, with address 0x6E
(5) Supports PWM-based AW8737A control for audio signal amplitude
(6) Supports driving up to 32 Neopixel RGB LEDs simultaneously
```
### 3 Custom firmware pin arrangement

```
Figure 1 M5PM1 Pin Diagram
```

#### 2 / 33

```
Table 1 Custom Firmware Pin
```
**Pin Num Pin Name Pin Type Pull Up/Down**

```
1 BAT_ADC_EN Push-Pull Output None
2 CHG_EN Push-Pull Output None
3 IO 0 GPIO None^
4 VSS Power None^
5 LED_EN Push-Pull Output None^
6 VCC Power None^
7 PWR_BTN Input Pull up
8 SDA I^2 C None
9 SCL I^2 C None^
10 DCDC_5V_EN Push-Pull Output None^
11 BAT_ADC ADC None^
12 IO 4 GPIO None
13 IO 3 GPIO None^
14 DCDC_3V3_EN Push-Pull Output None
15 BOOT_OUT Open-Drain Output None^
16 5VIN_ADC ADC None^
17 IO 1 GPIO None^
18 LDO_3V3_EN Push-Pull Output None
19 5VOUT_ADC ADC None^
20 IO 2 GPIO None
```

#### 3 / 33

## II. Pin Definitions................................................................................................................................................................

```
Table 2 Pin Definition
Pin Description Default MUX Note
```
```
IO 0 GPIO port 0, supports wake-up GPIO
```
### Neopixel

```
output
```
```
Wake-up is mutually
exclusive with IO
IO 1 GPIO port 1 GPIO ADC1 -
```
```
IO 2 GPIO port 2 , supports wake-up GPIO ADC
Wake-up is mutually
exclusive with IO 0
IO 3 GPIO port 3 , supports wake-up GPIO PWM
Wake-up is mutually
exclusive with IO 4
IO4 GPIO port 4 , supports wake-up GPIO PWM 2 Wake-up is mutually
exclusive with IO 3
BAT_ADC_EN Battery sampling enable, active high Sampling enable - High level by default
CHG_EN Battery charging enable, active high Charging enable - High level by default
DCDC_5V_EN 5V DC/DC control, active high DC control - Low level by default
DCDC_3V3_EN 3.3V DC/DC control, active high DC control - High level by default
LDO_3V3_EN 3.3V LDO control, active high LDO control - High level by default
5VIN_ADC 5V input ADC sampling pin ADC - Voltage divider ratio 1:
5VOUT_ADC 5V output ADC sampling pin ADC - Voltage divider ratio 1:
BAT_ADC Battery voltage ADC sampling pin ADC - Voltage divider ratio 1:
PWR_BTN Power control button input Button detection - Pull-up enabled by default
LED_EN Status indicator LED control LED control High level by default
BOOT_OUT Controls main controller ESP32 BOOT BOOT - High level by default
```
```
SDA I²C data line I²C -
Open-drain mode, external
pull-up resistor required
SCL I²C clock line I²C -
Open-drain mode, external
pull-up resistor required
```
Note:

1. All GPIO output types default to open-drain mode, including Neopixel driving, PWM output, etc. If no external pull-up

resistor is connected, the pin must be configured as push-pull mode to output correctly.

2. LED uses M5PM1 proprietary LED self-control logic and is not recommended for other purposes.


#### 4 / 33

## III. Register Mapping

```
Table 3 Register Map
```
**Register Name Type Addr Bit R/W Default Description Reset Download Power Off**

```
Device_ID System 0x00 [7:0] R 0x50 Device Type — — —
Device_Model System 0x01 [7:0] R 0x20 Device Model — — —
HW_REV System 0x02 [7:0] R 0x05 Hardware Version — — —
SW_REV System 0x03 [7:0] R 0x06 Firmware Version — — —
```
```
PWR_SRC System 0x
[7:3] Reserved
[2:0] VALID
R — Power Source Bitmap — — —
```
```
WAKE_SRC System 0x05 [7]^ Reserved^
[6:0] FLAGS
R/W — Wake-up Source Flag — — —
```
```
PWR_CFG System 0x
```
```
[7:5] Reserved
[4] LED CONTROL
[3] 5VIN/OUT
[2] 3.3V_LDO_EN
[1] 3.3V_DCDC_EN
[0] CHG_EN
```
```
R/W 0x17 Power Management Bit
```
```
0b0001011x
Charging status is not
affected by reset
```
```
0b0001011x
Charging status is not
affected by reset
```
```
0b0001011x
Charging status is not
affected by reset
```
```
HOLD_CFG System 0x
```
```
[7] Reserved
[6] 5VIN/OUT
[5] 3.3V LDO
[4:0] GPIO4~
```
```
R/W 0x
```
```
As corresponding bit is
set to 1, the states of
corresponding GPIO,
LDO, and 5VIN/OUT
will be retained after
power-off. This register
```
### will be reset to 0x00.

```
when Download mode
or a Reset is triggered
(I^2 C watchdog reset,
command reset, and
user timer reset),
```
```
0x00 0x00 —
```

#### 5 / 33

**Register Name Type Addr Bit R/W Default Description Reset Download Power Off**

```
BATT_LVP System 0x08 [7:0] R/W 0x
Low-voltage threshold:
2000 mV + n×7.81 mV
```
#### — — —

```
I2C_CFG System 0x
```
```
[7:5] Reserved
[4] SPD
[3:0] SLP_TO
```
```
R/W 0x00 — — — —
```
```
WDT_CNT System 0x0A [7:0] R/W 0x
```
```
Watchdog countdown
(seconds); set to 0 to
disable the watchdog
function
```
#### — — —

```
WDT_KEY System 0x0B [7:0] W —
Write 0xA5 to clear and
reload
```
#### — — —

```
SYS_CMD System 0x0C
```
```
[7:4] KEY(0xA)
[3:2] Reserved
[1:0] CMD
```
```
W — System command — — —
```
```
— — 0x0D-0x0F Reserved — — —
```
```
GPIO_MODE GPIO 0x10 [7:5]^ Reserved^
[4:0] GPIO4~
R/W 0x
```
```
1 = Output, 0 = Input
(Must take effect only
when the related
GPIO_FUNC is set to 00)
```
```
0x00 0x
```
```
0b000xxxxx
It is determined by bits 0 to
4 of the GPIO_Power_Hold
register (0x07): 1 = Hold, 0
= State reset
```
```
GPIO_OUT GPIO 0x
[7:5] Reserved
[4:0] GPIO4~
R/W 0x
```
```
1 =High, 0=Low
(Must take effect only
when the related
GPIO_FUNC is set to 00)
```
```
0x00 0x
```
```
0b000xxxxx
It is determined by bits 0 to
4 of the GPIO_Power_Hold
register (0x07): 1 = Hold, 0
= State reset
```
```
GPIO_IN GPIO 0x12 [7:5]^ Reserved^
[4:0] GPIO4~
R — Real-time input value — — —
```

#### 6 / 33

```
Register Name Type Addr Bit R/W Default Description Reset Download Power Off
```
```
GPIO_DRV GPIO 0x
```
```
[7:6] Reserved
[5] LED EN
[4:0] GPIO4~
```
```
R/W 0x1F
1 = Open-drain,
0 = Push-pull
0x1F 0x1F
```
```
0b00xxxxxx
Among them, LED EN is not
affected. GPIO0–GPIO4 are
determined by bits 0 to 4 of
GPIO_Power_Hold register
(0x07): 1 = Hold, 0 = State
reset
```
```
GPIO_PU/PD_0 GPIO 0x
```
#### [7:6] GPIO

#### [5:4] GPIO

#### [3:2] GPIO

#### [1:0] GPIO

```
R/W 0x
```
```
Configured per 2 bits
PULL_NO: 00
PULL_UP: 01
PULL_DOWN: 10
```
```
0x00 0x
```
```
0bxxxxxxxx
It is determined by bits 0 to
3 of the GPIO_Power_Hold
register (0x07): 1 = Hold, 0
= State reset.
```
```
GPIO_PU/PD_1 GPIO 0x15 [7:2]^ Reserved^
[1:0] GPIO
R/W 0x00 Same as above; other
bits are reserved.
0x00 0x
```
```
0b000000xx
Determined by bit 4 of the
GPIO_Power_Hold register
(0x07): 1 = Hold, 0 = State
reset.
```
```
GPIO_FUNC0 GPIO 0x
```
#### [7:6] GPIO

#### [5:4] GPIO

#### [3:2] GPIO

#### [1:0] GPIO

```
R/W 0x
```
```
Configured per 2 bits
GPIO: 00
IRQ: 01
Special function: 11
Reserved: 10
```
```
0x00 0x
```
```
0bxxxxxxxx
It is determined by bits 0 to
3 of the GPIO_Power_Hold
register (0x07): 1 = Hold, 0
= State reset
```
```
GPIO_FUNC1 GPIO 0x
[7:2] Reserved
[1:0] GPIO4 R/W^ 0x^
```
```
Same as above; other
bits are reserved. 0x00^ 0x^
```
```
0b000000xx
Determined by bit 4 of the
GPIO_Power_Hold register
(0x07): 1 = Hold, 0 = State
reset.
```
**GPIO_WAKE_EN GPIO 0x18 [7:5]**^ Reserved^
**[4:0]** GPIO4~
R/W 0x

```
Description: 1 = Enable the corresponding GPIO Wake function; 0 = Disable the corresponding GPIO Wake function
(GPIO1 interrupt line conflicts with SDA and cannot be used for the WAKE function. GPIO0 and GPIO2 share one
interrupt line and are mutually exclusive. GPIO3 and GPIO4 share one interrupt line and are mutually exclusive).
```

#### 7 / 33

```
Register Name Type Addr Bit R/W Default Description Reset Download Power Off
```
**GPIO_WAKE_CFG GPIO 0x
[7:5]** Reserved
**[4:0]** GPIO4~
R/W 0x

```
GPIO: wake-up
1 = Rising-edge
0 = Falling-edge
```
#### — — —

```
— — 0x1A-0x1F Reserved — — —
```
```
VREF_L ADC 0x20 [7:0] R —
MCU ADC (mV)
VREF low 8 bits
```
#### — — —

```
VREF_H ADC 0x21 [7:0] R —
MCU ADC (mV)
VREF high 8 bits
```
#### — — —

```
VBAT_L ADC 0x22 [7:0] R — BAT Voltage^ (mV)^
low 8 bit
```
#### — — —

```
VBAT_H ADC 0x23 [7:0] R —
BAT Voltage (mV)
high 8 bit
```
#### — — —

```
VIN_L ADC 0x24 [7:0] R — VIN Voltage (mV)^
low 8 bit
```
#### — — —

```
VIN_H ADC 0x25 [7:0] R —
VIN Voltage (mV)
high 8 bit
```
#### — — —

```
5VOUT_L ADC 0x26 [7:0] R — 5VOUT Voltage (mV)^
low 8 bit
```
#### — — —

```
5VOUT_H ADC 0x27 [7:0] R —
5VOUT Voltage (mV)
high 8 bit
```
#### — — —

```
ADC_RES_L ADC 0x28 [7:0] R — ADC result low 8 bit — — —
```
```
ADC_RES_H ADC 0x
[7:4] Reserved
[3:0] Data[11:8]
R — ADC result high 4 bit — — —
```
```
ADC_CTRL ADC 0x2A
```
```
[7:4] Reserved
[3:1] CH_SEL
[0] START
```
```
R/W 0x
```
```
Description: START = 1 starts the conversion (automatically cleared to 0 after conversion is complete). CH_SEL
selects the channel (valid channels are 1, 2, and 6; 1 and 2 correspond to GPIO1 and GPIO2 and take effect only
when the corresponding GPIO_FUNC is set to 11; 6 is the internal chip temperature measurement, with the unit
in °C).
```

#### 8 / 33

```
Register Name Type Addr Bit R/W Default Description Reset Download Power Off
```
```
— — 0x2B-0x2F Reserved — — —
```
```
PWM0_L PWM 0x30 [7:0] Duty[7:0] R/W 0x
PWM0 duty cycle
低 8 bit —^ —^ —^
```
```
PWM0_HC PWM 0x
```
```
[7:6] Reserved
[5] POL
[4] EN
[3:0] Duty[11:8]
```
```
R/W 0x
```
```
EN = 1, Enable
POL = 1, Active low
PWM1 duty cycle
high 8 bits
```
#### — — —

```
PWM1_L PWM 0x32 [7:0] Duty[7:0] R/W 0x
PWM1 duty cycle
低 8 bit
```
#### — — —

```
PWM1_HC PWM 0x
```
```
[7:6] Reserved
[5] POL
[4] EN
[3:0] Duty[11:8]
```
```
R/W 0x
```
```
EN = 1, Enable
POL = 1, Active low
PWM1 duty cycle
high 8 bits
```
#### — — —

```
PWM_FREQ_L PWM 0x34 [7:0] R/W 0xF4 PWM frequency^
low 8 bit
```
#### — — —

```
PWM_FREQ_H PWM 0x35 [7:0] R/W 0x
PWM frequency
high 8 bit
```
#### — — —

```
— — 0x36-0x37 Reserved — — —
```
**TIM_CNT_BYTE_0 Timer 0x38 [7:0]** R/W 0x
Timed wake-up
counter Byte0 (s)

#### — — —

**TIM_CNT_BYTE_1 Timer 0x39 [7:0]** R/W 0x
Timed wake-up
counter Byte1 (s)

#### — — —

**TIM_CNT_BYTE_2 Timer 0x3A [7:0]** R/W 0x
Timed wake-up
counter Byte2 (s)

#### — — —

**TIM_CNT_BYTE_3 Timer 0x3B
[7]** Reserved
**[6:0]**
R/W 0x
Timed wake-up
counter Byte3 (s)

#### — — —


#### 9 / 33

```
Register Name Type Addr Bit R/W Default Description Reset Download Power Off
```
```
TIM_CFG Timer 0x3C
```
```
[7:4] Reserved
[3] ARM
[2:0] ACTION
```
```
R/W 0x
```
```
ARM = 1 , starts
counting (if TIM_CNT is
0, ARM will be
automatically cleared);
ACTION: see Table 4.
```
```
Cleared to 0 when the
timer is triggered
```
```
Cleared to 0 when the
timer is triggered
0x
```
```
TIM_KEY Timer 0x3D [7:0] W —
Write 0xA5 to clear and
reload
```
#### — — —

```
— — 0x3E-0x3F Reserved — — —
```
```
IRQ Status 1 IRQ 0x
[7:5] Reserved
[4:0] GPIO4~
R/W 0x
```
```
Description: When the bit of the corresponding GPIO equals 1, it indicates that the level of the corresponding GPIO
has changed. The corresponding GPIO level can be read via register 0x06 (except for GPIOs configured as IRQ;
other IOs must have the corresponding GPIO_FUNC set to 00 and GPIO_MODE set to 0). In this case, GPIOs set as
IRQ will be pulled low and will only be released and pulled high again after the IRQ Status is cleared.
```
```
IRQ Status 2 IRQ 0x
```
```
[7:6] Reserved
[5] 电池移除
[4] 电池插入
[3] 5VINOUT移除
[2] 5VINOUT插入
[1] 5V IN移除
[0] 5V IN插入
```
```
R/W 0x
```
```
Description: When the corresponding bit equals 1, it indicates that the corresponding event has occurred. At this
time, the GPIO configured as IRQ will be pulled low and will only be released and pulled high again after the IRQ
Status is cleared.
Note:
```
1. Battery insertion/removal is only valid when battery charging is disabled; it is invalid when battery charging is
enabled.
2. 5VIN/OUT insertion/removal is only valid when 5VIN/OUT is set to INPUT; it is invalid when set to OUTPUT.

```
IRQ Status 3 IRQ 0x
```
```
[7:3] Reserved
[2] DOUBLE_CLICK
[1] WAKEUP
[0] SINGLE_CLICK
```
```
R/W 0x
```
```
Description:
```
1. Bit0 is also a reset detection bit. After the PWR_BTN reset function is disabled, a single click of the PWR_BTN will
trigger the button single-click interrupt.
2. WAKE_SRC (0x2F) is related to bit1 of IRQ Status 3. That is, if WAKE_SRC (0x2F) is not cleared, IRQ Status 3 bit
will remain 1.
3. Bit2 is also a power-off detection bit. After the PWR_BTN double-click power-off function is disabled, a double
click of the PWR_BTN will trigger the button double-click interrupt.

**IRQ Status 1_Mask IRQ 0x
[7:5]** Reserved
**[4:0]** GPIO4~0 R/W^ 0x^

```
Set corresponding bit
to 1 indicates that the
interrupt is masked
```
#### — — —


#### 10 / 33

```
Register Name Type Addr Bit R/W Default Description Reset Download Power Off
```
**IRQ Status 2_Mask IRQ 0x**

```
[7:6] Reserved
[5] Battery remove
[4] Battery add
[3] 5VINOUT remove
[2] 5VINOUT add
[1] 5V IN remove
[0] 5V IN add
```
```
R/W 0x
```
```
Set corresponding bit
to 1 indicates that the
interrupt is masked
```
#### — — —

**IRQ Status 3_Mask IRQ 0x**

```
[7:3] Reserved
[2] Double click
[1] Wakeup
[0] Click
```
```
R/W 0x
```
```
Set corresponding bit
to 1 indicates that the
interrupt is masked
```
#### — — —

```
— — 0x46-0x47 Reserved — — —
```
```
BTN_Status BTN 0x
```
#### [7] BTN_EVENT

```
[6:1] Reserved
[0] BTN_Status
```
```
R 0x00 Button status — — —
```
```
BTN_CFG_1 BTN 0x
```
#### [7] DL_LOCK

#### [6:5] DBL

#### [4:3] LONG

#### [2:1] SINGLE

#### [0] SINGLE_RESET_DIS

```
R/W 0x2A
Button Configuration
Register 1
```
#### — — —

```
BTN_CFG_2 BTN 0x4A
[7:1] Reserved
[0] DOUBLE_POWEROFF_DIS R/W^ 0x^
```
```
Button Configuration
Register 2
```
#### — — —

```
— — 0x4B-0x4F Reserved — — —
```
```
NEO_CFG RGB 0x
```
```
[7] Reserved
[6] REFRESH
[5:0] LED_CNT
```
```
R/W 0x
```
```
Description:
NeoPixel count, refresh control for 32 LEDs takes approximately 7 ms. During this time, interrupts are disabled,
meaning I^2 C communication will be blocked
— — 0x51-0x52 Reserved — — —
```

#### 11 / 33

**Register Name Type Addr Bit R/W Default Description Reset Download Power Off**

```
AW8737A PULSE 0x
```
#### [7] REFRESH

#### [6:5] NUM

#### [5:0] GPIO

```
R/W 0x
GPIO effective value
0 ～ 4 —^ —^ —^
```
```
— — 0x54-0x5F Reserved — — —
```
**NEO_PLXn_L/H RGB
0x60-
0x9F
[7:0]** R/W 0x
Up to 32 × RGB
pixel data, 64 bytes.

#### — — —

#### RTC_MEM RTC

```
0xA0-
0xBF
[7:0] R/W 0x
32 Byte
RTC backup RAM
```
#### — — —

```
— — 0xC0-0xFF Reserved — — —
```
```
Note:
```
1. RES: Reserved bits
2. Reset includes button reset, command reset, I2C watchdog reset, and timer reset
3. Power-off includes button power-off, command power-off, and timer power-off


#### 12 / 33

## IV. Key Register Description

### ⚠️ Register Access Limitation: I2C continuous read/write operations are only supported for specific address ranges

**(0x00–0x0C, 0x10–0x19, 0x20–0x2A, 0x30–0x35, 0x38–0x3D, 0x40–0x45, 0x48–0x4A, 0x50, 0x53, 0x60–0x9F, 0xA0–0xBF).**

**Cross-range operations must be performed in multiple transactions.**

### 1 System Registers

```
（ 1 ） Device_ID (0x00）:
⚫ Access: R
⚫ Default: 0x
⚫ Function: Device Type
（ 2 ） Device_Model (0x0 1 ):
⚫ Access: R
⚫ Default: 0x
⚫ Function: Device Model
（ 3 ） HW_REV (0x02):
⚫ Access: R
⚫ Default: 0x
⚫ Function: Hardware Version
（ 4 ） SW_REV (0x03):
⚫ Access: R
⚫ Default: 0x
⚫ Function: Firmware Version
（ 5 ） PWR_SRC (0x04):
⚫ Access: R
⚫ Default: None
⚫ Function: Power Source Status
⚫ Bit definition:
[7~3]: Reserved
[2] BAT: Battery valid
[1] 5VINOUT: 5VINOUT valid (only when 5V boost is disabled; when 5V boost is enabled, this bit is 0)
[0] 5VIN: 5VIN valid
Note :
```
1. Multiple power sources may exist simultaneously. The system determines the current power source status
by detecting whether there is voltage on the corresponding ADC pins.


#### 13 / 33

2. When battery charging is enabled but no battery is connected, the voltage measured by the ADC may float
and become unstable, causing instability in battery power source detection. It is recommended to disable
battery charging when no battery is present.

**（ 6 ） WAKE_SRC (0x05):**

```
⚫ Access: R/W
⚫ Default: None
⚫ Function: Wake-up Source Flag
⚫ Bit definition:
[7]: Reserved
```
### [6] 5V INOUT: 5V INOUT insertion wake-up (only when 5V boost is disabled)

### [5] EXT_WAKE: GPIO WAKE wake-up

```
[4] CMD_RST: Reset command wake-up
[3] RSTBTN: Reset button wake-up
[2] PWRBTN: Power button wake-up
[1] VIN: 5VIN insertion wake-up
[0] TIM: Timer wake-up
Note: Writing to this register can only clear the flag bit to ensure valid detection of the wake-up source the next time;
it cannot specify a wake-up source.
```
**（ 7 ） PWR_CFG (0x06):**

```
⚫ Access: R/W
⚫ Default: 0x17（0b0001 0111）
⚫ Function: Power switch control
⚫ Bit definition:
[7:5]: Reserved
[4] LED CONTROL: 1 = LED EN outputs high level, 0 = LED EN outputs low level
[3] 5VIN/OUT: 1 = 5V boost output enabled, 0 = 5V boost disabled (external input power can be connected)
[2] 3.3V_LDO_EN: 1 = Enable 3.3V LDO, 0 = Disable 3.3V LDO
[1] 3.3V_DCDC_EN: 1 = Enable 3.3V DC/DC, 0 = Disable 3.3V DC/DC
[0] CHG_EN: 1 = Enable charging, 0 = Disable charging
```
**（ 8 ） HOLD_CFG(0x07):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: Hold register, including power hold and GPIO hold
```

#### 14 / 33

```
⚫ Bit definition:
[7]: Reserved
```
### [6] 5vin/out: 5vin/out = 1 , 5vin/out power is retained after power-off; 5vin/out = 0, the 5vin/out power is not

```
retained after power-off
[5] ldo_3v3: ldo_3v3 = 1, ldo_3v3 power is retained after power-off; ldo_3v3 = 0, the ldo_3v3 power is not
retained after power-off
```
### [4] gpio4: gpio4 = 1 , GPIO 4 state is retained after power-off; gpio4 = 0, the state of GPIO 4 is reset after

```
power-off
[3] gpio3: gpio3 = 1, GPIO 3 state is retained after power-off; gpio 3 = 0, the state of GPIO 3 is reset after
power-off
[2] gpio2: gpio2 = 1, GPIO 2 state is retained after power-off; gpio 2 = 0, the state of GPIO 2 is reset after
power-off
[1] gpio1: gpio1 = 1, GPIO1 state is retained after power-off; gpio 1 = 0, the state of GPIO1 is reset after
power-off
[0] gpio0: gpio0 = 1, GPIO 0 state is retained after power-off; gpio 0 = 0, the state of GPIO 0 is reset after
power-off
```
**（ 9 ） BATT_LVP (0x08):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: Low-voltage threshold register.
When the voltage is lower than the low-voltage threshold, the system will forcibly power off
Low-voltage value calculation: 2.0 V + reg_value × 7.81 mV
Note : Power-on recovery conditions: the system can power on again when any one of the following conditions
```
1. Battery voltage is 100 mV higher than the configured voltage
2. 5VIN is inserted
3. 5VINOUT is inserted (only when the 5V boost is disabled)

**（ 10 ） I2C_CFG (0x09):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: I^2 C speed configuration and idle sleep configuration
⚫ Bit definition:
[7:5] DBL: Reserved
```

#### 15 / 33

```
[4] SPD: 0=100 k, 1=400 k
[3-0] SLP_TO: Specifies how many seconds without I^2 C communication before the M5PM1 enters sleep; set to
0 to disable this function.
Note :
1. Once the idle sleep function is configured, M5PM1 will not automatically clear this setting and must be
cleared manually by the user.
2. After the idle sleep function is configured and successfully triggered, M5PM1 enters sleep mode. If it is
woken up via I^2 C communication, the first I^2 C transaction is used only for wake-up and will fail. Subsequent
communications will operate normally. Note that within 300 ms after the first wake-up communication, if
M5PM1 does not receive a complete address, it will enter sleep mode again.
（ 11 ） WDT_CNT (0x0A):
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: Software watchdog, timeout to reset
Note : The watchdog countdown unit is seconds; set to 0 to disable this function.
（ 12 ） WDT_KEY (0x0B):
⚫ Access: W
⚫ Default: None
⚫ Function: Feed software watchdog
Note : Write 0xA5 to clear and reload
（ 13 ） SYS_CMD (0x0C):
⚫ Access: W
⚫ Default: None
⚫ Function: System Command Register
⚫ Bit definition:
[7:4] KEY: 0xA
[3-2]: Reserved
```
### [1-0] CMD: Command，01=Power off，10= Restart，11=Download

```
⚫ Write command: KEY=0xA + CMD (01 = Power off, 10 = Restart, 11 = Download)
```
### 2 GPIO Register

```
（ 1 ） GPIO_MODE (0x10):
⚫ Access: R/W
⚫ Default: 0x
```

#### 16 / 33

```
⚫ Function: GPIO mode
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Corresponding GPIO4–GPIO0 direction (1 = Output, 0 = Input)
⚫ Effective condition: GPIO_FUNCx set to 00
```
**（ 2 ） GPIO_OUT (0x11):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: GPIO output level
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Corresponding GPIO4–GPIO0 output level（1=High, 0=Low）
⚫ Effective condition:
GPIO_FUNCx set to 00 ；GPIO_MODE set to 1
```
**（ 3 ） GPIO_IN (0x12):**

```
⚫ Access: R
⚫ Default: None
⚫ Function: GPIO input status
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Corresponding GPIO4-GPIO0 input level（1=High, 0=Low）
```
**（ 4 ） GPIO_DRV (0x13):**

```
⚫ Access: R/W
⚫ Default: 0x1F
⚫ Function: GPIO output type
```
```
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Corresponding GPIO4-GPIO0 output type（1=Open-drain，0=Push-pull）
Priority description:
Output type configuration has higher priority than the multiplexing function and does not become invalid
when the multiplexing function is enabled or disabled.
For example, when a GPIO is multiplexed as PWM, if GPIO_DRV = 1 (open-drain), the actual output remains in
open-drain mode.
```

#### 17 / 33

**（ 5 ） GPIO_PU/PD_0 (0x14), GPIO_PU/PD_1 (0x15):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: GPIO pull-up / pull-down configuration
⚫ Bit definition: (Each 2 bits control one GPIO)
00 : No pull-up / pull-down
01 : Pull-up
10 : Pull-down
```
**（ 6 ） GPIO_FUNC0 (0x16), GPIO_FUNC1 (0x17):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: GPIO function
⚫ Bit definition: （Each 2 bits control one GPIO）
00 : Standard GPIO; 01 : IRQ interrupt
11 : Multiplexed function (NeoPixel/ADC/PWM); 10 : Reserved
```
**（ 7 ） GPIO_WAKE_EN (0x18):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: GPIO wake-up enable
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Corresponding GPIO4-GPIO0 wake enable (1 = Enable, 0 = Disable; GPIO1 is not supported)
Note :
```
1. GPIO WAKE configuration does not become invalid when entering Download mode, Reset, or Power-off.
2. Whether pull-up or pull-down is enabled for WAKE is controlled by GPIO_PU/PD_0 (0x14) and
GPIO_PU/PD_1 (0x15).

**（ 8 ） GPIO_WAKE_CFG (0x19):**

```
⚫ Access: R/W
⚫ Default: 0x
⚫ Function: Wakeup configuration
⚫ Bit definition:
[7:5]: Reserved
```

#### 18 / 33

```
[4:0]: Corresponding GPIO4–GPIO0 edge configuration (1 = Rising edge, 0 = Falling edge; GPIO1 is not
supported).
⚫ Effective condition: Takes effect only after WAKE_EN is enabled.
Note : GPIO WAKE configuration does not become invalid when entering Download mode, Reset, or Power-off.
```
### 3 ADC Register

```
（ 1 ） VREF_L（0x20）、VREF_H（0x21）:
⚫ Access: R
⚫ Default: None
⚫ Function: Internal reference voltage VREF
Note : MCU ADC internal reference voltage, unit: mV
（ 2 ） VBAT_L（0x22）、VBAT_H（0x23）:
⚫ Access: R
⚫ Default: None
⚫ Function: Battery voltage VBAT
Note : Battery voltage, unit: mV
（ 3 ） VIN_L（0x24）、VIN_H（0x25）:
⚫ Access: R
⚫ Default: None
⚫ Function: 5V input voltage VIN
Note : 5VIN voltage, unit: mV
（ 4 ） 5VOUT_L（0x26）、5VOUT_L（0x27）:
⚫ Access: R
⚫ Default: None
⚫ Function: 5V output voltage 5VOUT
Note : 5V output voltage, unit: mV
（ 5 ） ADC_RES_L (0x28)、ADC_RES_H (0x29):
⚫ Access: R
⚫ Default: None
⚫ Function: Channel conversion value
⚫ Note :
```
1. Combined into 12-bit data ([11:0]). When the channel selection is 1 or 2, it represents a 12-bit ADC result
with a range of 0–0xFFF.


#### 19 / 33

2. When the channel selection is 6, it represents the internal chip temperature, in °C.
**（ 6 ） ADC_CTRL (0x2A):**
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: ADC Conversion Register
⚫ Bit definition:
**[7:4]:** Reserved
**[3:1] CH_SEL:** Channel selection (1 = GPIO1, 2 = GPIO2, 6 = Internal temperature channel)
**[0] START:** Write 1 to start conversion (automatically cleared after completion)

### 4 PWM Control Register

```
（ 1 ） PWM0_L（0x30）:
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: PWM Register (Duty Cycle Low 8 Bits)
（ 2 ） PWM0_HC（0x31）:
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: PWM Register
⚫ Bit definition:
[7:6]: Reserved
[5] POL: Polarity (1 = Active low)
[4] EN: Enable (1 = Start)
[3:0] Duty[11:8]: Duty cycle high 4 bits (combined with PWM0_L to form a 12-bit duty cycle)
（ 3 ） PWM1_L（0x32）:
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: PWM Register (Duty Cycle Low 8 Bits)
（ 4 ） PWM1_HC（0x33）:
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: PWM Register
⚫ Bit definition:
```

#### 20 / 33

```
[7:6]: Reserved
[5] POL: Polarity (1 = Active low)
[4] EN: Enable (1 = Start)
[3:0] Duty[11:8]: Duty cycle high 4 bits (combined with PWM 1 _L to form a 12-bit duty cycle)
（ 5 ） PWM_FREQ_L (0x34)、PWM_FREQ_H (0x35):
⚫ Access: R/W
⚫ Default: 0xF4、0x01
⚫ Function: Configure PWM frequency, unit: Hz
```
### 5 Systerm Timer

```
（ 1 ） TIM_CNT_BYTE_0~3 (0x38-0x3B):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: Timed wake-up counter, unit: seconds, range: 0– 214748364
（ 2 ） TIM_CFG (0x3C):
```
```
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: Timer function configuration
⚫ Bit definition:
[7:6]: Reserved
[3] ARM: 1=Start the timer (automatically cleared when the count reaches 0)
[2:0] ACTION: Timer action (see Table 4)
Note :
```
1. System power-off and re-power-on will clear the TIM_CFG register.
2. After the wake-up flag is set, system power-on, system restart, or system power-off takes effect once, the
TIM_CFG register will be cleared.
    **Table 4 Timer Action Truth Table**
       **ACTION Function**
          0 Stop counter
          1 Set wake-up flag
10 System restart
11 System power on
100 System power off
**（ 3 ） TIM_KEY (0x3D):**
⚫ Access: W
⚫ Default: None
⚫ Function: Reload timer (write 0xA5)


#### 21 / 33

### 6 Interrupt and Wake-up Control

```
（ 1 ） IRQ Status 1 (0x40):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: IRQ Register 1
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Indicates the level change status of GPIO4–GPIO0; a value of 1 indicates that the corresponding GPIO
has a level change.
Note : This register can only be cleared by the user; setting bits to 1 is performed automatically by the system.
（ 2 ） IRQ Status 2 (0x41):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: IRQ Register 2
⚫ Bit definition: Corresponding bit value of 1 indicates that the event has occurred
[7:6]: Reserved
[5] Battery Remove: Battery removed, BAT voltage ≥ 2400 mV → BAT voltage < 2400 mV
[4] Battery Add: Battery inserted, BAT voltage ≤ 2400 mV → BAT voltage > 2400 mV
[3] 5VINOUT Remove: 5VIN/OUT inserted, 5VINOUT voltage ≤ 2400 mV → 5VINOUT voltage > 2400 mV
[2] 5VINOUT Add: 5VIN/OUT inserted, 5VINOUT voltage ≤ 2400 mV → 5VINOUT voltage > 2400 mV
[1] 5VIN Remove: 5VIN removed, 5VIN voltage ≥ 2400 mV → 5VIN voltage < 2400 mV
[0] 5VIN Add: 5VIN inserted, 5VIN voltage ≤ 2400 mV → 5VIN voltage > 2400 mV
Note : This register can only be cleared by the user; setting bits to 1 is performed automatically by the system.
⚫ Effective condition:
```
1. Battery insertion/removal events are only valid when CHG_EN = 0 (charging disabled).
2. 5VINOUT insertion/removal events are only valid when 5VIN/OUT = 0 (INPUT mode).
**（ 3 ） IRQ Status 3 (0x42):**
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: IRQ Register 3
⚫ Bit definition: Corresponding bit to 1 indicates that the event has occurred
**[7:3]:** Reserved


#### 22 / 33

```
[2] DOUBLE_CLICK: Button double-click
[1] WAKEUP: Power-on
[0] SINGLE_CLICK: Button single-click
Note : This register can only be cleared by the user; setting bits to 1 is performed automatically by the system.
⚫ Effective condition:
```
1. Bit0 is also a reset detection bit. After the PWR_BTN reset function is disabled, a single click of the PWR_BTN
will trigger the button single-click interrupt.
2. WAKE_SRC (0x2F) is related to bit1 of IRQ Status 3. That is, if WAKE_SRC (0x2F) is not cleared, IRQ Status 3
bit1 will remain 1.
3. Bit2 is also a power-off detection bit. After the PWR_BTN double-click power-off function is disabled, a
double click of the PWR_BTN will trigger the button double-click interrupt.

**（ 4 ） IRQ Status 1 Mask(0x43):**

```
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: IRQ Mask Register 1
⚫ Bit definition:
[7:5]: Reserved
[4:0]: Corresponding GPIO4-GPIO0
```
### Note : Setting the corresponding bit to 1 masks the interrupt

**（ 5 ） IRQ Status 2 Mask(0x44):**

```
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: IRQ Mask Register 2
⚫ Bit definition:
[7:6]: Reserved
[5] Battery Remove: Battery removed, BAT voltage ≥ 2400 mV → BAT voltage < 2400 mV
[4] Battery Add: Battery inserted, BAT voltage ≤ 2400 mV → BAT voltage > 2400 mV
[3] 5VINOUT Remove: 5VIN/OUT inserted, 5VINOUT voltage ≤ 2400 mV → 5VINOUT voltage > 2400 mV
[2] 5VINOUT Add: 5VIN/OUT inserted, 5VINOUT voltage ≤ 2400 mV → 5VINOUT voltage > 2400 mV
[1] 5VIN Remove: 5VIN removed, 5VIN voltage ≥ 2400 mV → 5VIN voltage < 2400 mV
[0] 5VIN Add: 5VIN inserted, 5VIN voltage ≤ 2400 mV → 5VIN voltage > 2400 mV
Note : Setting the corresponding bit to 1 masks the interrupt
```

#### 23 / 33

```
（ 6 ） IRQ Status 3 Mask (0x45):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: IRQ Mask Register 3
⚫ Bit definition:
[7:3]: Reserved
[2] DOUBLE_CLICK: Button double-click
[1] WAKEUP: Power-on
[0] SINGLE_CLICK: Button single-click
Note : Setting the corresponding bit to 1 masks the interrupt
IRQ Attention:
```
1. When no GPIO is configured as an IRQ pin, IRQ Status 1 (0x40), IRQ Status 2 (0x41), and IRQ Status 3 (0x42) will
be cleared.
2. During Reset and when entering Download mode, GPIOs are reset. At this time, no GPIO is configured as an IRQ
pin, so IRQ Status 1 (0x40), IRQ Status 2 (0x41), and IRQ Status 3 (0x42) will be cleared. If you need to use the IRQ
Status 3 (0x42) Wakeup IRQ in this situation, do not clear the WAKE_SRC (0x05) register until the GPIO is configured
as an IRQ pin again; otherwise, the IRQ Status 3 (0x42) Wakeup IRQ cannot be triggered.

### 7 Button Configuration

```
（ 1 ） BTN_Status（0x48）:
⚫ Access: R
⚫ Default: None
⚫ Function: Button status
⚫ Bit definition:
[7] BTN_Event: Press event, 1 = Button has been pressed, 0 = Button has not been pressed; automatically
cleared after reading, set to 1 automatically by the system
[6:1]: Reserved
[0] BTN_Status: Press status, 1 = Pressed, 0 = Released
（ 2 ） BTN_CFG (0x49):
⚫ Access: R/W
⚫ Default: 0x2A
⚫ Function: Button Configuration 1
⚫ Bit definition:
```

#### 24 / 33

```
[7] DL_LOCK: DL_LOCK = 1, Disable Download mode
[6:5] DBL: Double-click, 00 = 125 ms， 01 = 250 ms， 10 = 500 ms， 11 = 1 s
[4:3] LONG: Long press, 00 = 1 s， 01 = 2 s， 10 = 3 s， 11 = 4 s
[2:1] SINGLE: Single-click, 00 = 125 ms， 01 = 250 ms， 10 = 500 ms， 11 = 1 s
[0] SINGLE_RESET_DIS: SINGLE_RESET_DIS = 1 — Disable single-click reset; SINGLE_RESET_DIS = 0 — Enable
single-click reset
（ 3 ） BTN_CFG_2 (0x4A):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: Button Configuration 2
⚫ Bit definition:
[7:1]: Reserved
[0] DOUBLE_POWEROFF_DIS:
DOUBLE_POWEROFF_DIS = 1 — Disable double-click power-off
DOUBLE_POWEROFF_DIS = 0 — Enable double-click power-off
```
### 8 NeoPixel Control Module

```
NEO_CFG (0x50):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: RGB Configuration
⚫ Bit definition:
[7]: Reserved
[6] REFRESH: Refresh control (refreshing 32 LEDs takes approximately 7 ms, during which I^2 C interrupts are
disabled)
[5:0] LED_CNT: LED count (0–32, 0 = Disable driver)
```
### 9 AW8737A PULSE........................................................................................................................................................................................................

```
PULSE_CTRL (0x 53 ):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: AW8737A pulse output control
⚫ Bit definition:
[7] REFRESH: 1 = Refresh; 0 = No refresh
[6:5] NUM: Value range 0–3, actual output of 0–3 pulse(s)
```

#### 25 / 33

```
[4:0] GPIO: Value range 0–4, corresponding to hardware GPIO0 ~ GPIO4
Note :
```
1. After successful configuration, the corresponding GPIO is set to output mode and the GPIO output is

### modulated with the corresponding pulse.

2. If open-drain output mode is used, an external pull-up is required; otherwise, configure the corresponding
GPIO to push-pull output mode first.

### 10 NEO Buffer

```
NEO_PIXn_L/H (0x60-0x9F):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: RGB buffer
Note : Each LED occupies 2 bytes (RGB565 format), stored sequentially (PIX0_L = 0x36, PIX0_H = 0x37, ..., PIX31_H =
0x75).
```
### 11 RTC Buffer

```
RTC_MEM[0:31] (0xA0-0xBF):
⚫ Access: R/W
⚫ Default: 0x00
⚫ Function: RTC buffer
Note : 32 - byte power-retention RAM (power retention here refers to ESP32 power-off retention; M5PM1 power-off
does not retain data).
```

#### 26 / 33

## V. Additional Function Description

### 1 ADC

```
（ 1 ） Flow:
Write ADC_CTRL to select the channel and start (START = 1) → wait until BUSY = 0 → read ADC_D_H/L.
（ 2 ） Usage of Internal ADC Channels
⚫ 5VOUT:
Low-voltage threshold detection
5VOUT power insertion wake-up
5VOUT power removal and insertion detection
⚫ BAT: Low-voltage threshold detection, battery removal and insertion detection
⚫ 5VOUT: Low-voltage threshold detection, 5VIN removal and insertion detection
Attention:
```
1. 5VOUT has both input and output functions. Before enabling the 5VOUT boost, the 5VOUT voltage must be
detected first to ensure that there is no external power input; otherwise, it should remain in input mode.
2. All three channels have specific functions and must be connected strictly according to their intended usage.

### 2 PWM Output

```
Duty cycle = (DUTY[11:0] / 0xFFF) × 100%. The frequency is set by the PWM_FREQ register. The high byte and low byte of
```
the value are located in two registers and must be written at the same time; otherwise, two different duty cycles or frequencies

may be set within a short period.

### 3 PWR_BTN Button

```
Single click for reset, double click for power-off, long press to enter Download mode. The specific timing intervals are
```
determined by the BTN_CFG_1 register.

```
（ 1 ） Power-off
After power-off, the 3.3V DC/DC is turned off and LED_EN is turned off, but the charging enable is not affected.
```
```
Figure 2 Power-off Flowchart
```

#### 27 / 33

```
（ 2 ） Download Mode
```
```
Figure 3 Download Flowchart
（ 3 ） Reset
```
```
Figure 4 Reset Flowchart
```
### 4 LED Indicator

```
（ 1 ） Flow:
Set the LED count (LED_CFG[5:0]) → Write LED_RAM (RGB565 format) → Trigger REFRESH = 1.
（ 2 ） LED indication status under different conditions
⚫ Reset: LED flashes once
⚫ Download mode: LED flashes once every 500 ms
⚫ Button reset disabled and GPIO IRQ Function enabled: LED flashes once every 200 ms
⚫ Button double-click power-off disabled and GPIO IRQ Function enabled: LED flashes once every 100 ms
```
### 5 Low Voltage Protection

```
The low-voltage threshold is determined by the BATT_LVP (0x08) register. When neither 5VIN nor 5VINOUT is inserted
```
and the battery voltage falls below the threshold, the system will automatically power off.

```
Figure 5 Low-Voltage Detection Flowchart
Note : When VIN is not inserted, the BAT voltage must be greater than the BATT_LVP voltage + 100 mV to exit the
low-voltage standby loop.
```
### 6 I2C Idle Sleep

```
Configure SLEEP[3:0] to set the idle sleep time; 0 = Disable sleep.
The complete sleep procedure is as follows:
⚫ Enable PWR_BTN external interrupt
⚫ Enable 5VIN external interrupt
```

#### 28 / 33

```
⚫ Enable 5VINOUT external interrupt
⚫ Enable SDA external interrupt
⚫ Configure the timer to wake up every 100 ms (this interval cannot be changed)
⚫ Reset the ADC peripheral
```
### ⚫ Set I^2 C idle sleep flag to 1

```
Note : When the PWM function is enabled, I^2 C idle sleep is invalid; when entering Download mode, I^2 C idle sleep is invalid.
```
### 7 Interrupt Wake-up

```
（ 1 ） SDA Wake-up
In sleep mode, if there is I^2 C communication activity on SDA, M5PM1 will first disable the SDA and PWR_BTN external
interrupts, then reinitialize the I^2 C communication configuration. After that, there will be a 300 ms non-blocking
delay. During this period, if the PWR_BTN is pressed, it is considered a PWR_BTN wake-up; otherwise, the system
waits for the next I^2 C communication signal and checks whether the I^2 C address matches. If it matches, wake-up is
successful; otherwise, the system continues sleeping.
（ 2 ） PWR_BTN Wake-up
In sleep mode, if the PWR_BTN is pressed, wake-up is successful. The SDA and PWR_BTN external interrupts will be
disabled, and the I^2 C communication configuration will be reinitialized.
（ 3 ） 5VIN Wake-up（same as（ 2 ））
（ 4 ） 5VINOUT Wake-up（same as（ 2 ））
Note : The above wake-up mechanisms are based on a timer waking up every 100 ms. This is a firmware-fixed process
and cannot be changed.
```
### 8 IRQ Handling

```
（ 1 ）When any GPIO is configured as an IRQ pin, the GPIO configured as IRQ will be pulled low when IRQ Status 1 (0x40),
```
IRQ Status 2 (0x41), or IRQ Status 3 (0x42) is non-zero. It will be released and pulled high only when IRQ Status 1 (0x40), IRQ

Status 2 (0x41), and IRQ Status 3 (0x42) are all cleared.

```
（ 2 ）When any GPIO is configured as an IRQ pin, the non-IRQ IOs (GPIO_FUNC set to 00 and GPIO_MODE set to 0) will
```
be scanned. When the level of a scanned IO changes, the corresponding bit in IRQ Status 1 (0x40) will be set to 1.

```
（ 3 ）When any of the five GPIOs is configured as an IRQ pin, power events will be scanned. When a power event occurs,
```
the corresponding bit in IRQ Status 2 (0x41) will be set to 1.

```
Power events are as follows:
⚫ Battery Remove: Battery removed, BAT voltage ≥ 2400 mV → BAT voltage < 2400 mV
⚫ Battery Add: Battery inserted, BAT voltage ≤ 2400 mV → BAT voltage > 2400 mV
⚫ 5VINOUT Remove: 5VIN/OUT removed, 5VINOUT voltage ≥ 2400 mV → 5VINOUT voltage < 2400 mV
⚫ 5VINOUT Add: 5VIN/OUT inserted, 5VINOUT voltage ≤ 2400 mV → 5VINOUT voltage > 2400 mV
```

#### 29 / 33

```
⚫ 5VIN Remove: 5VIN removed, 5VIN voltage ≥ 2400 mV → 5VIN voltage < 2400 mV
⚫ 5VIN Add: 5VIN inserted, 5VIN voltage ≤ 2400 mV → 5VIN voltage > 2400 mV
（ 4 ）After Reset or entering Download mode, the GPIOs will be reset. At this time, no GPIO is configured as an IRQ pin,
```
and IRQ Status 1 (0x40), IRQ Status 2 (0x41), and IRQ Status 3 (0x42) will be cleared. If you need to use the IRQ Status 3 (0x42)

Wakeup IRQ, do not clear the WAKE_SRC (0x04) register until the GPIO is configured as an IRQ pin again; otherwise, the IRQ

Status 3 (0x42) Wakeup IRQ cannot be triggered.


#### 30 / 33

## VI. Use Examples

### 1 GPIO Wake-up

```
（ 1 ）Rising-edge wake-up control
```
### pm1.gpioSetMode(M5PM1_GPIO_NUM_0, M5PM1_GPIO_MODE_INPUT); // Set GPIO0 to input mode

### pm1.gpioSetPull(M5PM1_GPIO_NUM_0, M5PM1_GPIO_PULL_DOWN); // Set GPIO0 pull-down

(gpioSetPull can be ignored if external pull-down is present)
pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_0, true); // Enable GPIO0 wake-up
pm1.gpioSetWakeEdge(M5PM1_GPIO_NUM_0, M5PM1_GPIO_WAKE_RISING);// Set GPIO0 rising-edge wake-up
（ 2 ）Falling-edge wake-up control
pm1.gpioSetMode(M5PM1_GPIO_NUM_0, M5PM1_GPIO_MODE_INPUT); // Set GPIO0 to input mode
pm1.gpioSetPull(M5PM1_GPIO_NUM_0, M5PM1_GPIO_PULL_UP); // Set GPIO0 pull-up
(gpioSetPull can be ignored if external pull-up is present)
pm1.gpioSetWakeEnable(M5PM1_GPIO_NUM_0, true); // Enable GPIO0 wake-up
pm1.gpioSetWakeEdge(M5PM1_GPIO_NUM_0, M5PM1_GPIO_WAKE_FALLING); // Set GPIO0 falling-edge wake-up
（ 3 ）Attention
GPIO0, GPIO2, GPIO3, and GPIO4 support wake-up. However, GPIO0 and GPIO2 are mutually exclusive, and GPIO3 and

GPIO4 are mutually exclusive and cannot be used at the same time.

### 2 RGB

（ 1 ）RGB control
pm1.gpioSetFunc(M5PM1_GPIO_NUM_0, M5PM1_GPIO_FUNC_OTHER); // Set GPIO0 to multiplexed function
pm1.gpioSetDrive(M5PM1_GPIO_NUM_0, M5PM1_GPIO_DRIVE_PUSHPULL); // Set GPIO0 to push-pull output mode

m5pm1_rgb_t rgb_red = { 255 , 0 , 0 };
m5pm1_rgb_t rgb_green = { 0 , 255 , 0 };
m5pm1_rgb_t rgb_blue = { 0 , 0 , 255 };
m5pm1_rgb_t rgb_array[ 3 ] = { rgb_red, rgb_green, rgb_blue };

### pm1.setLeds(&rgb_red, 1 , 3 , true); // Set 3 RGB LEDs to red and refresh

// Appropriate delay

### pm1.setLeds(&rgb_green, 1 , 3 , true); // Set 3 RGB LEDs to green and refresh

// Appropriate delay

### pm1.setLeds(&rgb_blue, 1 , 3 , true); // Set 3 RGB LEDs to blue and refresh

// Appropriate delay

### pm1.setLeds(rgb_array, 3 , 1 , true); // Set 3 RGB LEDs to red, green, blue in sequence and refresh

// Appropriate delay
（ 2 ）Attention
1 ）When GPIO0 is set to a multiplexed function, the system switches the clock to 24 MHz to prepare for RGB timing
refresh. Switching the clock will reset I^2 C, so you must wait for a period of time before performing I^2 C
communication; otherwise, communication will fail.
2 ）The output mode strictly follows GPIO_DRV (0x13). If there is no external pull-up, GPIO must be configured as
push-pull to output correct RGB timing.
3 ）Interrupts are disabled during RGB output. Refreshing 32 LEDs takes about 7 ms, during which I^2 C communication
cannot be performed. After refreshing RGB timing, wait for a period of time before communication.
4 ）Only GPIO0 supports the RGB function.


#### 31 / 33

### 3 ADC

```
（ 1 ）ADC usage example
```
### pm1.gpioSetFunc(M5PM1_GPIO_NUM_ 1 , M5PM1_GPIO_FUNC_OTHER); // Set GPIO1 to multiplexed function

### pm1.gpioSetFunc(M5PM1_GPIO_NUM_ 2 , M5PM1_GPIO_FUNC_OTHER); // Set GPIO2 to multiplexed function

uint16_t gpio1_adc_value = 0 ;
uint16_t gpio2_adc_value = 0 ;
uint16_t temp_value = 0 ;
uint16_t vref_value = 0 ;

### pm1.analogRead(M5PM1_ADC_CH_1, &gpio1_adc_value); // Read ADC value of GPIO1

### pm1.analogRead(M5PM1_ADC_CH_ 2 , &gpio2_adc_value); // Read ADC value of GPIO2

### pm1.readTemperature(M5PM1_ADC_CH_TEMP, &temp_value); // Read internal MCU temperature

### pm1.readVref(&vref_value); // Read reference voltage for ADC calibration

### uint16_t gpio1_volt = (gpio1_adc_value * vref_value) / 4096 ; // Calculate actual voltage of GPIO1

### uint16_t gpio 2 _volt = (gpio 2 _adc_value * vref_value) / 4096 ; // Calculate actual voltage of GPIO2

```
（ 2 ）Attention
1 ) The ADC channel input voltage must not exceed 3.3 V; otherwise, the ADC module may malfunction.
2) The actual voltage is calculated using the reference voltage VREF.
```
### 4 PWM

```
（ 1 ）PWM usage example
```
### pm1.gpioSetFunc(M5PM1_GPIO_NUM_ 3 , M5PM1_GPIO_FUNC_OTHER); // Set GPIO3 to multiplexed function

### pm1.gpioSetFunc(M5PM1_GPIO_NUM_ 4 , M5PM1_GPIO_FUNC_OTHER); // Set GPIO4 to multiplexed function

### pm1.gpioSetDrive(M5PM1_GPIO_NUM_ 3 , M5PM1_GPIO_DRIVE_PUSHPULL); // Set GPIO3 to push-pull output mode

### pm1.gpioSetDrive(M5PM1_GPIO_NUM_ 4 , M5PM1_GPIO_DRIVE_PUSHPULL); // Set GPIO4 to push-pull output mode

### pm1.setPwmFrequency( 20000 ); // Set PWM frequency to 20 kHz

### pm1.setPwmDuty(M5PM1_PWM_CH_0, 50 , false, true); // Set 50% duty cycle PWM on GPIO3

### pm1.setPwmDuty(M5PM1_PWM_CH_ 1 , 50 , false, true); // Set 50% duty cycle PWM on GPIO4

```
（ 2 ）Attention
1 ) The two PWM channels are controlled by the same timer, so both channels share the same frequency.
2 ) Duty cycle range: 0–100%.
3 ) If there is no external pull-up, GPIO must be configured as push-pull to output PWM correctly.
```
### 5 TIM

```
（ 1 ）System reboot
```
pm1.timerSet( 10 , M5PM1_TIM_ACTION_REBOOT); // Reboot after 10 seconds

```
（ 2 ）System power on
```
pm1.timerSet( 10 , M5PM1_TIM_ACTION_POWERON); // Power on after 10 seconds;
if the system is powered off, it can automatically power on when the timer expires

```
（ 3 ）System power off
```
pm1.timerSet( 10 , M5PM1_TIM_ACTION_POWEROFF); // Power off after 10 seconds


#### 32 / 33

## Appendix

**Firmware Modification History
Version Date Change Description**
HW:2 / SW:1 2025 - 06 - 30 Initial version

#### HW: 3 / SW:2 2025 - 07 - 23

1. According to the hardware revision, modify the I2C pins and BOOT_OUT pin, adjust
the ADC voltage divider ratio, and change CHG_EN to push-pull output with the
control logic modified to high = enable and low = disable.
2. Change the default GPIO output type to open-drain.
3. Change the system timer TIM_CNT to 31 bits and adjust the register mapping.
4. Add 5VINOUT wake-up support (only when the 5V boost is disabled).

#### HW: 4 / SW:3 2025 - 09 - 01

1. The maximum time limit of the 32-bit timer is 214,748,364 seconds.
2. Added the GPIO_Power_Hold register (0x33). When the corresponding bit is set to
1, the state of the corresponding GPIO or LDO will be retained after power-off.
3. During power-off, GPIOs will be restored to the default state of input with no pull-
up/down (if the corresponding bit in the GPIO_Power_Hold register is set to 1, the
corresponding GPIO state will be retained after power-off). The power state will be
restored to the default state (charging control is not affected; if the corresponding bit
in the GPIO_Power_Hold register is set to 1, the corresponding LDO state will be
retained after power-off).
4. When entering Download mode, the GPIO_Power_Hold register will be reset to 0,
the I2C watchdog will stop, I2C idle sleep will stop, the user timer will stop, GPIOs will
be restored to the default input with no pull-up/down state, and the power state will
be restored to the default state.
5. When entering Reset mode (including button reset, command reset, I2C watchdog
reset, and user timer reset), the GPIO_Power_Hold register will be reset to 0, GPIOs will
be restored to the default input with no pull-up/down state, and the power state will
be restored to the default state.
6. Added LED indications:
    a. During reset, the LED flashes once;
    b. In Download mode, the LED flashes at 500 ms intervals;
    c. When button reset is disabled and a GPIO has IRQ enabled, the LED flashes at
    200 ms intervals;
    d. When button double-click power-off is disabled and a GPIO has IRQ enabled,
    the LED flashes at 100 ms intervals.a.
7. Added IRQ Status 3 (0x23).
    Bit0 is the reset interrupt. After button reset is disabled, a single click of the PWR_BTN
triggers this interrupt.
    Bit1 is the wake-up interrupt. On power-on or reset, the corresponding bit in
WAKE_SRC (0x2F) is set, and bit1 is also set (note that WAKE_SRC (0x2F) is related to
bit1 of IRQ Status 3; if WAKE_SRC (0x2F) is not cleared, IRQ Status 3 bit1 will remain 1).
    Bit2 is the btn_dl_click interrupt. After button double-click power-off is disabled, a
double click of the PWR_BTN triggers this interrupt.
8. Added IRQ Mask registers to allow masking of specific interrupts as required.
9. Charging is enabled by default.
10. Added bit0 to BTN_CFG: SINGLE_RESET_DIS. SINGLE_RESET_DIS = 1 disables
single-click reset; SINGLE_RESET_DIS = 0 enables single-click reset.


#### 33 / 33

```
Firmware Modification History
Version Date Change Description
```
11. Added register BTN_CFG_2 (0x31):
bit0-DOUBLE_POWEROFF_DIS. DOUBLE_POWEROFF_DIS = 1 disables double-click
power-off; DOUBLE_POWEROFF_DIS = 0 enables double-click power-off.
HW: 5 / SW:4 2025 - 09 - 17
1. Added AW8737A pulse-width modulation functionality.
2. Updated the firmware version to V4.

HW:5 / SW:5 2025 - 11 - 04

1. Added AW8737A pulse-width modulation output functionality.
2. Added DCDC_5V hold functionality.
3. Modified the watchdog to be disabled by default.

HW:5 / SW:6 2025 - 12 - 13

1. Updated the register map.
2. Added the BTN_Status (0x48) register.
3. Removed the UID register and added Device_ID and Device_Model.

HW:5 / SW:S 2026 - 01 - 06

1. Modified the timer function so that the timer configuration takes effect once and is
automatically cleared; the timer configuration is also cleared after power-off.
2. Adjusted the invalid threshold for USB and 5VOUT to 4V.

```
Document Revision History
Version Version Version
1 .0 2025 -^06 -^30 Initial version^
1.1 2025 - 07 - 23 Hardware revision: 3, Firmware major version: 2
1.2 2025 - 08 - 04 Revised some incorrect descriptions^
1.3 2025 - 09 - 01 Hardware revision: 4, Firmware major version: 3
1.4 2025 - 09 - 10 Added detailed explanations for key registers^
1.5 2025 - 09 - 17 Hardware revision: 5, Firmware major version: 4
1.6 2025 - 11 - 04 Hardware revision: 5, Firmware major version: 5^
1.7 2025 - 12 - 13 Hardware revision: 5, Firmware major version: 6, Added use cases
```
1. 8 2026 - 01 - 06 Hardware revision: 5, Firmware major version: S^
1.9 2026 - 01 - 22 Revised some incorrect descriptions


