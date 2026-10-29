/** @file TemperatureRead.cpp @brief Board temperature reading with a usable range. */

#include "TemperatureRead.h"

#include <cmath>
#include <driver/temperature_sensor.h>

static temperature_sensor_handle_t sensorHandle = nullptr;
static bool sensorInitAttempted = false;

static void ensureSensor()
{
  if (sensorInitAttempted) return;
  sensorInitAttempted = true;

  // The sensor only accepts its predefined ranges (S3: -10..80 C). A wider
  // range is rejected and every read would return NAN.
  temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
  if (temperature_sensor_install(&config, &sensorHandle) != ESP_OK)
  {
    sensorHandle = nullptr;
    return;
  }
  if (temperature_sensor_enable(sensorHandle) != ESP_OK)
  {
    temperature_sensor_uninstall(sensorHandle);
    sensorHandle = nullptr;
  }
}

float readBoardTemperatureC()
{
  ensureSensor();
  if (sensorHandle == nullptr) return NAN;

  float celsius = NAN;
  if (temperature_sensor_get_celsius(sensorHandle, &celsius) != ESP_OK) return NAN;
  return celsius;
}
