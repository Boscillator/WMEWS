# ESP32-S3 Firmware

This is a minimal ESP-IDF application for the ESP32-S3 development board. It
logs a hello-world message once per second after boot.

## First-time setup

Clone the repository with all ESP-IDF dependencies:

```sh
git clone --recurse-submodules <repository-url>
cd WMEWS
```

For an existing clone, initialize them instead:

```sh
git submodule update --init --recursive
```

Install the ESP32-S3 tools using the checked-in SDK, then activate its shell
environment. Repeat the activation command in each new shell.

```sh
./esp-idf/install.sh esp32s3
. ./esp-idf/export.sh
```

## Build

From this directory, build the firmware. The tracked defaults select the
`esp32s3` target on a clean checkout.

```sh
idf.py build
```

## Flash and monitor

Connect the board, replace `<serial-port>` with its device path, then flash and
open the serial monitor:

```sh
idf.py -p <serial-port> flash monitor
```

Exit the monitor with `Ctrl+]`. The log includes:

```text
I (...) wmews: Hello from the WMEWS ESP32-S3 firmware!
```

## Clean rebuild

Remove generated build output and configure again:

```sh
idf.py fullclean
idf.py build
```
