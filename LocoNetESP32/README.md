Before you download: This is outdated. A new library IoTT_LocoNetESP32 will be released with Video #27. Please use the new library as it is much better.

# EspSoftwareSerial

Implementation of the Arduino software serial library for the ESP32

Same functionality as the corresponding AVR library but several instances can be active at the same time.
Speed up to 115200 baud is supported. The constructor also has an optional input buffer size.

Please note that due to the fact that the ESP always have other activities ongoing, there will be some inexactness in interrupt
timings. This may lead to bit errors when having heavy data traffic in high baud rates.


