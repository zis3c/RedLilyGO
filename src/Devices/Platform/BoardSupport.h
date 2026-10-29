/** @file BoardSupport.h @brief Selected board support facade. */

#pragma once

/** @brief Board model selected by this firmware profile. */
#define DEVICE_MAKE_MODEL "LILYGO_T_DONGLE_S3"

#include "../Boards/LilyGO_TDongleS3/ESP32BoardSupport.h"

namespace Devices
{
  extern ESP32BoardSupport Board;
}
