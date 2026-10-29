/** @file TemperatureRead.h @brief Board temperature reading with a usable range. */

#pragma once

/**
 * Read the ESP32-S3 internal temperature sensor.
 *
 * The Arduino core installs the driver with a 10-50 C range, so readings above
 * 50 C are extrapolated and read far too high. This helper installs the driver
 * once with the widest range the sensor accepts on ESP32-S3 (-10..80 C).
 *
 * @return Temperature in Celsius, or NAN when the sensor is unavailable.
 */
float readBoardTemperatureC();
