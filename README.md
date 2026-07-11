# Washing Machine Early Warning System

Goal: Detect poorly balanced washing machine loads before they
cause violant shaking on the laundry machine using an accelerometer
and machine learning.

Secondary Goal: Learn AWS ML-Ops.

## Project Architecture
### IOT Device
- M5Stack M5StickS3 with ESP32-S3 and BMI270 IMU
- Deep sleeps until IMU detects motion
- Collects IMU measurements and forwards to cloud
- Runs local model to predict unbalanced load and sounds alarm

### Injest pipeline
- IOT device calls lambda function to get pre-signed URL for S3 upload
- IOT device uploads chucnked protobuf files to cloud

## Current Limitations
- M5StickS3 is in the mail, currently limited to ESP32-S3-WROOM dev kit with no
  imu.


