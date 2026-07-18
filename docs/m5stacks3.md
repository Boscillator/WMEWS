# StickS3

<span class="product-sku">SKU:K150</span>

<PictureViewer>
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_01.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_02.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_03.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_13.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_04.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_05.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_06.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_07.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_08.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_09.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_10.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_11.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_12.webp">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-weight.jpg">
</PictureViewer>

## Description

StickS3 is a compact and high-performance programmable controller designed for remote control and IoT applications. It is powered by the ESP32-S3-PICO-1-N8R8 main control chip, supporting 2.4 GHz Wi-Fi wireless communication, with built-in 8MB Flash and 8MB PSRAM to meet diverse application development needs, delivering excellent performance and scalability.

For human–machine interaction, it features a 1.14" LCD display, a 6-axis IMU sensor, and programmable buttons. The audio system adopts the ES8311 mono audio codec, combined with a high-sensitivity MEMS microphone and AW8737 power amplifier, enabling clear audio capture and high-fidelity audio output, empowering voice recognition and interactive experiences. It also integrates IR transmitter and receiver, a 250mAh lithium battery, making it suitable for smart home control, AI voice assistants, and IoT project development scenarios.

## Tutorial

learn>| ![Arduino IDE](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/assets/img/arduino/arduino_banner_01.png) | [Arduino IDE](/en/arduino/m5sticks3/program) | This tutorial introduces how to program and control the StickS3 device using the Arduino IDE. |

learn>| ![UiFlow2](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/static/assets/img/uiflow2/uiflow2.0_banner_01.png) | [UiFlow2](/en/uiflow2/sticks3/program) | This tutorial will introduce how to control the StickS3 device through the UiFlow2 graphical programming platform. |

learn>| ![StickS3 Xiaozhi Voice Assistant](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/sticks3_xiaozhi_cover.jpg) | [StickS3 Xiaozhi Voice Assistant](/en/guide/realtime/xiaozhi/sticks3) | This tutorial introduces how to use the StickS3 device to flash the Xiaozhi voice assistant firmware via M5Burner and build a personal voice assistant application. |

learn>| ![StickS3 ESP-Claw Firmware Flashing](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/StickS3-ESP-Claw-cover.jpg) | [StickS3 ESP-Claw Firmware Flashing](/en/guide/agent/esp_claw/sticks3) | This tutorial introduces how to flash the ESP-Claw firmware onto the StickS3, enabling users to quickly configure their StickS3 as an intelligent terminal capable of AI interaction, hardware programming, and automation control. |

## Note

!> EXT_5V_EN Input Power Supply Notice | The device 5V power interface can be configured as DC 5V output / input mode. The interface defaults to input mode. In this state, DC 5V power can be supplied via the Grove interface, the EXT_5V on the top Hat2-Bus, or the 5VIN interface. When configured to output mode, power input is only allowed via USB or the 5VIN on the top Hat2-Bus. Do not supply power through other output interfaces, otherwise there is a risk of short circuit and device damage.

!> Infrared Reception Notes | 1.The infrared receiving and decoding of StickS3 **must use the ESP32 RMT peripheral** and does not support receiving and decoding via GPIO method. <br>2. When using the infrared receiver function, the speaker amplifier must be turned off; otherwise, reception will not work properly. The operation method can be referenced in the [tutorial](/en/arduino/m5sticks3/m5pm1?id=spk%20amp) <br>3. Ensure that the transmitter and receiver are aligned as directly as possible, and keep a distance of no less than 30 cm between them. If the distance is too short, abnormal reception may occur.

!> Speaker Volume Notice | When powered by battery (USB not connected), it is recommended to keep the speaker volume below 75% to avoid unexpected device reboot caused by excessive power consumption.

?> Abnormal Device Noise | Early batches of StickS3 may produce slight abnormal noise after startup, which does not affect functional use.

?> Precautions for Use | Do not disassemble the product enclosure without authorization. Disassembly may damage the antenna PFC circuit and affect normal device operation.

## Features

- Integrated ESP32-S3-PICO-1-N8R8 main controller
- 8MB Flash and 8MB PSRAM
- ES8311 mono audio codec chip
- MEMS microphone + speaker
- Integrated IR transmitter + IR receiver
- Magnetic back design
- Expansion interfaces:
  - Expandable Hat2 bus (2.54-16P)
  - HY2.0-4P interface
- Development Platform
  - Arduino
  - UiFlow2
  - ESP-IDF
  - PlatformIO

## Includes

- 1 x StickS3

## Applications

- Smart home control
- AI voice assistant
- IoT project development

## Specifications

| Specification         | Parameter                                                                                                                           |
| --------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| SoC | ESP32-S3-PICO-1-N8R8 @ Xtensa® 32-bit LX7 dual-core processor, clock frequency 240MHz |
| Flash                 | 8MB                                                                                                                                 |
| PSRAM                 | 8MB Octal                                                                                                                           |
| IMU                   | BMI270                                                                                                                              |
| Wi-Fi                 | 2.4 GHz Wi-Fi                                                                                                                       |
| Display               | Model: ST7789P3<br>Resolution: 135x240                                                                                              |
| Input Power           | USB Type-C DC 5V                                                                                                                    |
| Audio Codec           | ES8311: 24-bit resolution, I2S protocol                                                                                             |
| Microphone            | MEMS microphone, Signal-to-Noise Ratio (SNR): 65 dB                                                                                 |
| Speaker               | AW8737 power amplifier + 8Ω@1W 2011 cavity speaker                                                                                  |
| Operating Temperature | 0 ~ 40°C                                                                                                                            |
| Battery Capacity      | 250mAh                                                                                                                              |
| Grove Load Capacity   | No load: 5V<br>Max: 4.88V@0.38A                                                                                                     |
| Power Consumption     | Power off: 4.2V@14.02uA<br>L1 state: 4.2V@52.47uA<br>L2 state: 4.2V@102.40uA<br>L3A state: 4.2V@36.69mA<br>Full load: 4.2V@519.02mA |
| Product Size          | 48.0 x 24.0 x 15.0mm                                                                                                                |
| Product Weight        | 20.0g                                                                                                                               |
| Package Size          | 65.0 x 25.0 x 15.0mm                                                                                                                |
| Gross Weight          | 22.4g                                                                                                                               |

## Learn

### Compatibility Notes

**StickS3** is structurally incompatible with the following Hat series products: Hat Mini JoyC (SKU: U156), Hat Mini EncoderC (SKU: U157), and Hat 18650C (SKU: U080).

### Download Mode

Connect the device with a USB cable and press and hold the reset button on the side of the device. When the internal green LED flashes, the device has successfully entered download mode.

<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/stickS3_operate_01.gif" width="50%">

### Button Operation Instructions

<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-stickS3_main-products_13.jpg" width="50%">

- Long press: Enter download mode
- Double click: Power off
- Single click: Power on / Reset

### EXT_5V_EN

In the default initialization of M5Unified, EXT_5V_EN is disabled. This operation turns off the power supply to the Grove, Hat EXT_5V interfaces, and IR TX/RX, and switches them to input mode. In this state, an external 5V power input is required for IR TX/RX to operate properly. For use cases without an external power supply, you can re-enable the EXT_5V output mode through the following API to restore power to IR TX/RX.

```cpp
M5.Power.setExtOutput(true); // EXT_5V OUTPUT
// M5.Power.setExtOutput(false); // EXT_5V INPUT
```

### IMU Triaxial Direction Schematic Diagram

<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/IMU-StickS3.jpg" width="70%">

## Schematics

- [StickS3 Schematics PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf)

<SchViewer>
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24_page_02.png">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24_page_03.png">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24_page_04.png">
<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24_page_05.png">
</SchViewer>

## PinMap

### LCD

| ESP32‑S3 | G39  | G40 | G45 | G41 | G21 | G38 |
| -------- | ---- | --- | --- | --- | --- | --- |
| ST7789P3 | MOSI | SCK | RS  | CS  | RST | BL  |

### IMU & M5PM1

| ESP32‑S3      | G48 | G47 |
| ------------- | --- | --- |
| BMI270 (0x68) | SCL | SDA |
| M5PM1 (0x6e)  | SCL | SDA |

### M5PM1

| M5PM1          | G0            | G1       | G2          | G3             | G4           |
| -------------- | ------------- | -------- | ----------- | -------------- | ------------ |
| Battery Charge | PYG0_CHG_STAT |          |             |                |              |
| ESP32-S3       |               | PYG1_IRQ |             |                |              |
| L3B Power      |               |          | PYG2_L3B_EN |                |              |
| Speaker        |               |          |             | PYG3_SPK_Pulse |              |
| IMU INT        |               |          |             |                | PYG4_IMU_INT |

### Audio

| ESP32‑S3      | G18  | G14  | G17  | G15  | G16 | G48 | G47 |
| ------------- | ---- | ---- | ---- | ---- | --- | --- | --- |
| ES8311 (0x18) | MCLK | DOUT | BCLK | LRCK | DIN | SCL | SDA |

### Button

| ESP32‑S3 | G11   | G12   |
| -------- | ----- | ----- |
| KEY1     | Input |       |
| KEY2     |       | Input |

### IR

| ESP32‑S3 | G46   | G42   |
| -------- | ----- | ----- |
| IR       | IR_TX | IR_RX |

### HY2.0-4P

- PORT.A

::grove-table
| HY2.0-4P    | Black | Red | Yellow | White |
| ----------- | ----- | --- | ------ | ----- |
| PORT.CUSTOM | GND   | 5V  | G9     | G10   |
::

### Hat2-Bus

::m5-bus-table
| PIN    | LEFT | RIGHT | PIN |
| ------ | ---- | ----- | --- |
| GND    | 1    | 2     | G5  |
| EXT_5V | 3    | 4     | G4  |
| Boot   | 5    | 6     | G6  |
| G1     | 7    | 8     | G7  |
| G8     | 9    | 10    | G43 |
| BAT    | 11   | 12    | G44 |
| 3V3_L2 | 13   | 14    | G2  |
| 5V_IN  | 15   | 16    | G3  |
::

## Model Size

- [StickS3 Model Size PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-sticks3.pdf)

<img src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-sticks3_page_01.png" width="100%">

## Structure

- [StickS3 Structure Files](https://github.com/m5stack/M5_Hardware/tree/master/Products/K150_StickS3/Structures)

## Datasheets

- [ESP32S3](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/477/esp32-s3_technical_reference_manual_en.pdf)
- [ES8311](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/products/atom/Atomic%20Echo%20Base/ES8311.pdf)
- [BMI270](https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/core/K128%20CoreS3/BMI270.PDF)

## Softwares

### Quick Start

- [StickS3 ESP-Claw Firmware Flashing](/en/guide/agent/esp_claw/sticks3)

### Arduino

- [StickS3 Arduino Quick Start](/en/arduino/m5sticks3/program)
- [StickS3 M5PM1 Power Management](/en/arduino/m5sticks3/m5pm1)
- [M5PM1 Arduino Library](https://github.com/m5stack/M5PM1)
- [StickS3 Arduino M5Unified Driver Library](https://github.com/m5stack/M5Unified)
- [StickS3 Arduino M5GFX Driver Library](https://github.com/m5stack/M5GFX)

### UiFlow2

- [StickS3 UiFlow2 Quick Start](/en/uiflow2/sticks3/program)

### Protocol

- [M5PM1 Power Management Chip](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/M5PM1_Datasheet_EN.pdf)

### Other

- [StickS3 Xiaozhi Voice Assistant](/en/guide/realtime/xiaozhi/sticks3)

### PlatformIO

```bash
[env:m5stack-sticks3]
platform = espressif32@6.12.0
board = esp32-s3-devkitc-1
framework = arduino
board_build.arduino.partitions = default_8MB.csv
board_build.arduino.memory_type = qio_opi
build_flags =
    -DESP32S3
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=5
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
lib_deps =
    M5Unified=https://github.com/m5stack/M5Unified
    M5PM1=https://github.com/m5stack/M5PM1
```

## Video

- StickS3 Product Introduction and Feature Overview

<video class="video-container" controls><source src="https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150-StickS3-video-EN.mp4" type="video/mp4"></video>

<TabPanel>
  <template #tab-Bilibili>
      <div class="video-iframe">
        <iframe src="//player.bilibili.com/player.html?isOutside=true&aid=115949470486273&bvid=BV1G3zCBGEdh&p=1&autoplay=0" loading="lazy" scrolling="no" border="0" frameborder="no" framespacing="0" allowfullscreen="true"></iframe>
      </div>
  </template>
  <template #tab-Youtube>
      <div class="video-iframe">
        <iframe width="560" height="315" src="https://www.youtube.com/embed/bBwT7dZOZl8?si=JIrlXGf4ra1oGdmf" title="YouTube video player" frameborder="0" loading="lazy" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
      </div>
  </template>
</TabPanel>

<TabPanel>
  <template #tab-Bilibili>
      <div class="video-iframe">
        <iframe src="//player.bilibili.com/player.html?isOutside=true&aid=116815778876283&bvid=BV1vD756jETQ&p=1&autoplay=0" loading="lazy" scrolling="no" border="0" frameborder="no" framespacing="0" allowfullscreen="true"></iframe>
      </div>
  </template>
  <template #tab-Youtube>
      <div class="video-iframe">
        <iframe width="560" height="315" src="https://www.youtube.com/embed/OOgfKi4Al-Q?si=0q8Ao4FFXRXfChP9" title="YouTube video player" frameborder="0" loading="lazy" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
      </div>
  </template>
</TabPanel>

## Product Comparison

If you need to compare information for Stick series products, you can visit the [Product Selector](/en/products_selector/m5stick_compare?select=K150), select the target products, and obtain the comparison results. The selector covers key information such as core parameters and functional features, and supports simultaneous comparison of multiple products.
