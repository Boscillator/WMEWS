# ESP32-S3 Firmware

This ESP-IDF application provisions Wi-Fi and device credentials into the
default NVS partition, connects as a Wi-Fi station, synchronizes with
`pool.ntp.org`, and logs the UTC time.

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

## Provision credentials

From this directory, copy the tracked template to the ignored local file and
replace every placeholder. Do not commit `config.csv`.

```sh
cp config.csv.example config.csv
```

The required keys are `ssid`, `password`, `device_id`, `secret_key`, and
`lambda_url` in the `credentials` namespace. `lambda_url` is the Lambda
Function URL base endpoint used to request an upload URL. The application never
logs secret credential values.

## Build

From this directory, build the firmware. The tracked defaults select the
`esp32s3` target on a clean checkout.

```sh
idf.py build
```

The build generates an NVS image from the local `config.csv`. A missing local
CSV intentionally fails the build to avoid compiling credentials into the
application image.

## Update credentials only

After changing `config.csv`, build and flash only the NVS partition:

```sh
idf.py build
idf.py -p <serial-port> nvs-flash
```

`nvs-flash` updates only the generated NVS image. A normal `flash` also writes
the application and reprovisions NVS from `config.csv`.

## Flash and monitor

Connect the board, replace `<serial-port>` with its device path, then flash and
open the serial monitor:

```sh
idf.py -p <serial-port> flash monitor
```

Exit the monitor with `Ctrl+]`. With valid Wi-Fi access and NTP reachability,
the log includes an assigned DHCP address and synchronized UTC time:

```text
I (...) network: Wi-Fi connected; IPv4 address: 192.0.2.10
I (...) wmews: Synchronized UTC time: 2026-07-11T12:34:56Z
```

## Clean rebuild

Remove generated build output and configure again:

```sh
idf.py fullclean
idf.py build
```
