/** @file Format.h @brief Firmware formatting and serialization helpers. */

#pragma once

#include <Preferences.h>

/**
 * @brief Prompt for and optionally format configured storage.
 * @param[in,out] prefs Persistent settings store.
 * @return None.
 * @note Blocking setup-time operation; must not run from ISR or network task.
 */
void AskFormatSD(Preferences& prefs);
