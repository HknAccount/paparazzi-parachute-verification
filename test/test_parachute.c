/*
 * Requirements-based tests for Paparazzi sw/airborne/modules/safety/parachute.c
 *
 * Each test is tagged with:
 *   @TC        test case ID
 *   @verifies  the LLR(s) it verifies (see reqs/LLR_parachute.sdoc)
 *   @type      normal | boundary | robustness
 * scripts/trace_matrix.py reads these tags to build the traceability matrix.
 *
 * MC/DC design (masking MC/DC, short-circuit evaluation):
 *   D4  agl_dist_valid && (agl_filtered < 6)    -> TC-005, TC-006, TC-007
 *   D6  (mode!=RATE) && (z>30) && (!close)      -> TC-009, TC-010, TC-011, TC-012
 *   D7  |phi|>55 || |theta|>55 || mode==KILL     -> TC-013, TC-014, TC-015, TC-016
 */
#include <math.h>
#include "unity.h"
#include "modules/safety/parachute.h"
#include "modules/sonar/agl_dist.h"
#include "firmwares/rotorcraft/autopilot_firmware.h"   /* test stub */

/* ---- Stub data owned by the test harness ---- */
struct pprz_autopilot_stub autopilot;
struct EnuCoor_f stub_enu_pos;
struct FloatEulers stub_eulers;
float agl_dist_valid;
float agl_dist_value;
float agl_dist_value_filtered;

/* Global defined in parachute.c */
extern bool close_to_ground;

/* 55 degrees exactly, as a float, and the next float above it */
#define DEG55_F     ((float)(55.0 * M_PI / 180.0))
#define ABOVE_55_F  (nextafterf(DEG55_F, 10.f))

static void set_agl(bool valid, float filtered)
{
  agl_dist_valid = valid ? 1.f : 0.f;
  agl_dist_value_filtered = filtered;
}

static void run_cycles(int n)
{
  for (int i = 0; i < n; i++) {
    periodic_parachute();
  }
}

void setUp(void)
{
  /* Nominal flight: NAV mode, 50 m high, level, AGL out of range */
  autopilot.mode = AP_MODE_NAV;
  stub_enu_pos.x = 0.f; stub_enu_pos.y = 0.f; stub_enu_pos.z = 50.f;
  stub_eulers.phi = 0.f; stub_eulers.theta = 0.f; stub_eulers.psi = 0.f;
  set_agl(false, 20.f);

  /* Reset the internal static AGL counter: one AUTO cycle with invalid AGL */
  parachute.arming_method = AUTO;
  periodic_parachute();

  init_parachute();
}

void tearDown(void) {}

/* ===================== Initialization and arming method ===================== */

// @TC TC-001 @verifies LLR-001 @type normal
void test_init_sets_auto_unarmed_not_deployed(void)
{
  parachute.arming_method = ON;
  parachute.armed = true;
  parachute.deploy = true;
  init_parachute();
  TEST_ASSERT_EQUAL_INT(AUTO, parachute.arming_method);
  TEST_ASSERT_FALSE(parachute.armed);
  TEST_ASSERT_FALSE(parachute.deploy);
}

// @TC TC-002 @verifies LLR-002 LLR-007 @type normal
void test_off_disarms_and_skips_trigger(void)
{
  parachute.arming_method = OFF;
  parachute.armed = true;
  stub_eulers.phi = 1.5f;          /* would deploy if trigger were evaluated */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.armed);
  TEST_ASSERT_FALSE(parachute.deploy);
}

// @TC TC-003 @verifies LLR-003 LLR-008 @type normal
void test_on_arms_level_flight_no_deploy(void)
{
  parachute.arming_method = ON;
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.armed);
  TEST_ASSERT_FALSE(parachute.deploy);
}

// @TC TC-004 @verifies LLR-003 LLR-008 @type normal
void test_on_arms_even_low_and_deploys_on_roll(void)
{
  parachute.arming_method = ON;
  stub_enu_pos.z = 2.f;            /* below 30 m: ON ignores altitude */
  stub_eulers.phi = 1.2f;          /* ~69 deg */
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.armed);
  TEST_ASSERT_TRUE(parachute.deploy);
}

/* ===================== AGL counter / close to ground ===================== */

// @TC TC-005 @verifies LLR-004 LLR-005 @type boundary
void test_close_to_ground_after_more_than_10_samples(void)
{
  set_agl(true, 1.f);
  run_cycles(10);                  /* counter = 10, not > 10 */
  TEST_ASSERT_FALSE(close_to_ground);
  TEST_ASSERT_TRUE(parachute.armed);
  periodic_parachute();            /* counter = 11 > 10 */
  TEST_ASSERT_TRUE(close_to_ground);
  TEST_ASSERT_FALSE(parachute.armed);
}

// @TC TC-006 @verifies LLR-004 @type normal
void test_invalid_agl_resets_counter(void)
{
  set_agl(true, 1.f);
  run_cycles(10);
  set_agl(false, 1.f);             /* valid = false, distance still low */
  periodic_parachute();            /* counter reset to 0 */
  set_agl(true, 1.f);
  run_cycles(10);                  /* counter = 10 again */
  TEST_ASSERT_FALSE(close_to_ground);
}

// @TC TC-007 @verifies LLR-004 @type boundary
void test_agl_at_6m_resets_counter_below_6m_counts(void)
{
  set_agl(true, 5.99f);
  run_cycles(10);
  set_agl(true, 6.0f);             /* exactly 6.0: not < 6.0, reset */
  periodic_parachute();
  set_agl(true, 5.99f);
  run_cycles(10);
  TEST_ASSERT_FALSE(close_to_ground);
  periodic_parachute();            /* 11th consecutive valid low sample */
  TEST_ASSERT_TRUE(close_to_ground);
}

// @TC TC-008 @verifies LLR-005 @type robustness
void test_counter_saturates_close_to_ground_stays_true(void)
{
  set_agl(true, 1.f);
  run_cycles(5000);
  TEST_ASSERT_TRUE(close_to_ground);
  set_agl(false, 1.f);             /* one invalid sample clears it at once */
  periodic_parachute();
  TEST_ASSERT_FALSE(close_to_ground);
}

/* ===================== Automatic arming decision (D6) ===================== */

// @TC TC-009 @verifies LLR-006 @type normal
void test_auto_arms_when_all_conditions_met(void)
{
  periodic_parachute();            /* NAV, 50 m, not close: (T,T,T) */
  TEST_ASSERT_TRUE(parachute.armed);
}

// @TC TC-010 @verifies LLR-006 @type normal
void test_auto_not_armed_in_rate_direct(void)
{
  autopilot.mode = AP_MODE_RATE_DIRECT; /* (F,-,-) */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.armed);
}

// @TC TC-011 @verifies LLR-006 @type boundary
void test_auto_altitude_boundary_30m(void)
{
  stub_enu_pos.z = 30.f;           /* (T,F,-): not > 30 */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.armed);
  stub_enu_pos.z = nextafterf(30.f, 100.f);
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.armed);
}

// @TC TC-012 @verifies LLR-006 @type normal
void test_auto_not_armed_when_close_to_ground(void)
{
  set_agl(true, 1.f);              /* (T,T,F) */
  run_cycles(11);
  TEST_ASSERT_TRUE(close_to_ground);
  TEST_ASSERT_FALSE(parachute.armed);
}

/* ===================== Deployment decision (D7) ===================== */

// @TC TC-013 @verifies LLR-008 @type boundary
void test_deploy_on_roll_beyond_55deg(void)
{
  stub_eulers.phi = DEG55_F;       /* exactly 55 deg: not > threshold */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.deploy);
  stub_eulers.phi = ABOVE_55_F;    /* (T,-,-) */
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.deploy);
  stub_eulers.phi = -ABOVE_55_F;   /* negative roll, absolute value */
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.deploy);
}

// @TC TC-014 @verifies LLR-008 @type boundary
void test_deploy_on_pitch_beyond_55deg(void)
{
  stub_eulers.theta = -DEG55_F;    /* exactly 55 deg nose down */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.deploy);
  stub_eulers.theta = -ABOVE_55_F; /* (F,T,-) */
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.deploy);
}

// @TC TC-015 @verifies LLR-008 @type normal
void test_deploy_in_kill_mode(void)
{
  autopilot.mode = AP_MODE_KILL;   /* (F,F,T), level, 50 m */
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.armed);
  TEST_ASSERT_TRUE(parachute.deploy);
}

// @TC TC-016 @verifies LLR-008 @type normal
void test_deploy_cleared_when_attitude_recovers(void)
{
  stub_eulers.phi = 1.2f;
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.deploy);
  stub_eulers.phi = 0.f;           /* (F,F,F): deploy goes back to false */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.deploy);  /* see finding PR-001 */
}

// @TC TC-017 @verifies LLR-007 @type normal
void test_deploy_kept_when_disarmed(void)
{
  parachute.arming_method = ON;
  stub_eulers.phi = 1.2f;
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.deploy);
  parachute.arming_method = OFF;   /* disarm: trigger not evaluated */
  stub_eulers.phi = 0.f;
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.armed);
  TEST_ASSERT_TRUE(parachute.deploy);   /* see finding PR-002 */
}

/* ===================== Robustness (abnormal inputs) ===================== */

// @TC TC-018 @verifies LLR-004 @type robustness
void test_nan_agl_with_valid_flag_resets_counter(void)
{
  set_agl(true, 1.f);
  run_cycles(10);
  set_agl(true, NAN);              /* NaN < 6 is false: counter reset */
  periodic_parachute();
  TEST_ASSERT_FALSE(close_to_ground);
  TEST_ASSERT_TRUE(parachute.armed);    /* see finding PR-003 */
}

// @TC TC-019 @verifies LLR-006 @type robustness
void test_nan_altitude_does_not_arm(void)
{
  stub_enu_pos.z = NAN;            /* NaN > 30 is false */
  periodic_parachute();
  TEST_ASSERT_FALSE(parachute.armed);
}

// @TC TC-020 @verifies LLR-008 @type robustness
void test_nan_attitude_does_not_deploy(void)
{
  stub_eulers.phi = NAN;
  stub_eulers.theta = NAN;
  periodic_parachute();
  TEST_ASSERT_TRUE(parachute.armed);
  TEST_ASSERT_FALSE(parachute.deploy);  /* see finding PR-003 */
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_init_sets_auto_unarmed_not_deployed);
  RUN_TEST(test_off_disarms_and_skips_trigger);
  RUN_TEST(test_on_arms_level_flight_no_deploy);
  RUN_TEST(test_on_arms_even_low_and_deploys_on_roll);
  RUN_TEST(test_close_to_ground_after_more_than_10_samples);
  RUN_TEST(test_invalid_agl_resets_counter);
  RUN_TEST(test_agl_at_6m_resets_counter_below_6m_counts);
  RUN_TEST(test_counter_saturates_close_to_ground_stays_true);
  RUN_TEST(test_auto_arms_when_all_conditions_met);
  RUN_TEST(test_auto_not_armed_in_rate_direct);
  RUN_TEST(test_auto_altitude_boundary_30m);
  RUN_TEST(test_auto_not_armed_when_close_to_ground);
  RUN_TEST(test_deploy_on_roll_beyond_55deg);
  RUN_TEST(test_deploy_on_pitch_beyond_55deg);
  RUN_TEST(test_deploy_in_kill_mode);
  RUN_TEST(test_deploy_cleared_when_attitude_recovers);
  RUN_TEST(test_deploy_kept_when_disarmed);
  RUN_TEST(test_nan_agl_with_valid_flag_resets_counter);
  RUN_TEST(test_nan_altitude_does_not_arm);
  RUN_TEST(test_nan_attitude_does_not_deploy);
  return UNITY_END();
}
