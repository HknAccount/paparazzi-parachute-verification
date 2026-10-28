/*
 * TEST STUB - replaces the real Paparazzi header
 *   sw/airborne/firmwares/rotorcraft/autopilot_firmware.h
 * for host-based unit testing of modules/safety/parachute.c.
 *
 * It provides only the symbols parachute.c uses:
 *   - autopilot.mode and the AP_MODE_* values (same values as
 *     sw/airborne/firmwares/rotorcraft/autopilot_static.h)
 *   - stateGetPositionEnu_f() and stateGetNedToBodyEulers_f()
 *
 * The test harness owns the stub data, so each test case can set
 * the aircraft mode, altitude and attitude it needs.
 */
#ifndef AUTOPILOT_FIRMWARE_STUB_H
#define AUTOPILOT_FIRMWARE_STUB_H

#include "std.h"

/* Same numeric values as autopilot_static.h in Paparazzi */
#define AP_MODE_KILL         0
#define AP_MODE_FAILSAFE     1
#define AP_MODE_HOME         2
#define AP_MODE_RATE_DIRECT  3
#define AP_MODE_ATTITUDE_DIRECT 4
#define AP_MODE_NAV          13

struct pprz_autopilot_stub {
  uint8_t mode;
};
extern struct pprz_autopilot_stub autopilot;

struct EnuCoor_f {
  float x;
  float y;
  float z;   /* altitude (up), metres */
};

struct FloatEulers {
  float phi;    /* roll, radians */
  float theta;  /* pitch, radians */
  float psi;    /* yaw, radians */
};

extern struct EnuCoor_f stub_enu_pos;
extern struct FloatEulers stub_eulers;

static inline struct EnuCoor_f *stateGetPositionEnu_f(void) { return &stub_enu_pos; }
static inline struct FloatEulers *stateGetNedToBodyEulers_f(void) { return &stub_eulers; }

#endif /* AUTOPILOT_FIRMWARE_STUB_H */
