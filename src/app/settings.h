#ifndef MINISHOT_SETTINGS_H
#define MINISHOT_SETTINGS_H

#include "config.h"

// Load config from $HOME/MS_CONFIG_FILE into out, falling back to defaults.
void ms_config_load(struct ms_config *out);

// Persist cfg to $HOME/MS_CONFIG_FILE. Returns 0 on success, -1 on failure.
int ms_config_save(const struct ms_config *cfg);

// Open the settings window.
void ms_settings_open(void);

#endif
