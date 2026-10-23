/** @file BoardSupportBase.h @brief Shared board lifecycle base contract. */

#pragma once

#include "../../RedLilyGOCapability.h"

class BoardSupport : public RedLilyGOCapability {
public:
  /** @brief Construct generic board support. */
  BoardSupport();

  /** @brief Service board-specific periodic state. */
  virtual void loop(Preferences& prefs);
  /** @brief Initialize common board support. */
  virtual void begin(Preferences& prefs);
};
