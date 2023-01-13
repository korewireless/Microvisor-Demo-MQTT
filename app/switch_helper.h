/**
 *
 * Microvisor switch Helper
 * Version 1.0.0
 * Copyright © 2023, Twilio
 * Licence: Apache 2.0
 *
 */

/* Simple switch on PA7
 */
#ifndef SWITCH_HELPER_H
#define SWITCH_HELPER_H

/*
 * INCLUDES
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PROTOTYPES
 */
void switch_init();
void switch_close();
void switch_open();

#ifdef __cplusplus
}
#endif

#endif /* SWITCH_HELPER_H */

