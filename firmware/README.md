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

## Host-based unit tests

The host test suite builds selected hardware-independent modules for Linux. It
uses ESP-IDF's bundled Unity source and standard CMake/CTest. From the
host-test project, build and run the suite:

```sh
cd host_test
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The suite currently contains one dummy test that verifies the test project is
configured correctly. Add future test sources and the production C sources
they cover to `host_test/CMakeLists.txt`.

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

## BMI270 any-motion wake and threshold tuning

The firmware automatically enables the production wake path during startup,
before it begins recording. Pressing KEY2 (ESP32-S3 GPIO12) requests an M5PM1
power-off. The BMI270 any-motion interrupt remains armed as the intended wake
source through its INT1-to-M5PM1-GPIO4 path.
The PMIC retains the BMI270's L1 3.3 V supply while the ESP32 is powered off,
so INT1 remains valid until motion occurs.

For a bench check, boot the device, press KEY2 and observe PMIC shutdown, then
move the device enough to trigger any-motion. It should boot again and report
the external-GPIO motion-candidate wake reason.

The default threshold is 64 BMI270 any-motion feature units; valid thresholds
are 0 through 2047. Any-motion always monitors the X, Y, and Z axes, using the
BMI270 hardware default duration of five 50 Hz samples (100 ms). The driver
configures INT1 as active-low push-pull and maps its any-motion output to
M5PM1 GPIO4, which the PMIC arms for falling-edge wake with a pull-up. Call
`bmi270_disable_anymotion_interrupt(imu, power)` before closing either driver
when motion wake is no longer required.

For bench threshold tuning, the driver also includes a disabled-by-default trace.

The BMI270 driver includes a disabled-by-default trace for tuning any-motion
settings. To enable it, change `BMI270_DEBUG_ANYMOTION` from `0` to `1` near
the driver configuration constants in `firmware/main/bmi270.c`, then rebuild
the firmware with `idf.py build`. The trace evaluates every accelerometer
sample but emits one log record for every tenth sample, so it can still produce
high log volume; use it only for bench testing and leave it disabled for normal
battery operation.

Records have this shape:

```text
I (...) bmi270: any-motion trace: sensor_time=123456 accel_lsb=(12,-34,4090) delta_lsb=(2,-5,18) first=0
```

`sensor_time` is the BMI270 sensor-time counter. `accel_lsb` contains raw
accelerometer values, and `delta_lsb` contains signed differences from the
immediately previous sample on each axis, including samples that are not
printed. The first record after initialization or reinitialization is marked
`first=1` and reports `delta_lsb=(n/a,n/a,n/a)`.
The driver is configured for ±8 g, where 4096 LSB equals 1 g; therefore 1 LSB
is approximately 0.244 mg.

The any-motion feature compares an acceleration slope against its programmed
threshold and requires the condition to hold for a configured consecutive
sample duration. See the `any_motion` section of
[`docs/bmi270.md`](../docs/bmi270.md) for the sensor settings. The logged
adjacent-sample deltas are observed measurement data for threshold tuning,
not a software emulation of the BMI270's internal reference algorithm.
