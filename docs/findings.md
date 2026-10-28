# Verification findings (problem reports)

Software under test: `sw/airborne/modules/safety/parachute.c`, Paparazzi commit
`64176905c3cbdf592a13a9c2dd08f9c121b9d235`.

These are observations from requirements reverse-engineering, test design and code review.
Each one is backed by a test or by a line of code. They are **not** claims that the
Paparazzi project has a bug: the module is research/hobby UAV code, not certified software,
and the intended behaviour was never written down. In a DO-178C project, each item would go
to the development team as a problem report to confirm or reject.

| ID | Type | Summary | Evidence |
|---|---|---|---|
| PR-001 | Behaviour / safety | `deploy` is not latched | TC-016 |
| PR-002 | Behaviour / safety | `deploy` keeps a stale value when disarmed | TC-017 |
| PR-003 | Robustness | NaN inputs are not detected | TC-018, TC-020 |
| PR-004 | Documentation | Comments and module description do not match the code | Code review |
| PR-005 | Configurability | `PARACHUTE_AGL_THRESHOLD` cannot be overridden | Code review |
| PR-006 | Integration | `parachute.deploy` is not consumed anywhere in the repository | Repository search |
| PR-007 | Information | The 55° threshold is compared in double precision | TC-013, TC-014 |

## PR-001: `deploy` is not latched
`check_parachute_trigger()` sets `parachute.deploy = false` whenever the trigger condition is
false. If the aircraft exceeds 55° of roll for one cycle and then recovers, `deploy` goes
back to false on the next cycle (TC-016). Physically, a parachute cannot be un-deployed. If
the actuator only fires while `deploy` is true, a one-cycle (20 ms at 50 Hz) pulse may be
too short. **Question for designers:** should `deploy` latch until reset on the ground?

## PR-002: `deploy` keeps a stale value when disarmed
When the arming method is changed to OFF after a deployment, the trigger is no longer
evaluated, so `deploy` stays true forever (TC-017). The ground station would keep showing
"deployed" in the DEBUG telemetry. This is consistent with LLR-007 as reverse-engineered,
but the intent is unclear.

## PR-003: NaN inputs are not detected
- NaN AGL distance with the valid flag set: `NaN < 6` is false, so the counter resets and the
  aircraft is treated as *not* close to the ground, which allows arming (TC-018).
- NaN roll and pitch: `fabsf(NaN) > threshold` is false, so the parachute never deploys on
  attitude (TC-020).
- NaN altitude: `NaN > 30` is false, so the parachute is not armed (TC-019). This case fails safe.

DO-178C robustness testing (abnormal inputs) is expected for Level A to C software. The
code relies on upstream modules never producing NaN.

## PR-004: Comments and module description do not match the code
- The comment in `check_parachute_trigger()` says "Set mode to KILL and deploy parachute",
  but the code only sets `parachute.deploy`. It does not change the autopilot mode.
- The comment above `PARACHUTE_ATT_ANGLE_THRESHOLD` says "number of consequtive threshold
  samples", copied from the counter parameter above it.
- `conf/modules/parachute.xml` describes a different module ("keeps track of the current
  state of a rotating wing drone and desired state set by the RC or flightplan").

## PR-005: `PARACHUTE_AGL_THRESHOLD` cannot be overridden
Every other parameter is wrapped in `#ifndef ... #define`, so it can be set from the airframe
file. `PARACHUTE_AGL_THRESHOLD` (6.0 m) has no guard, so redefining it causes a compiler
warning and the value is fixed.

## PR-006: `parachute.deploy` is not consumed anywhere in the repository
A search of `sw/` and `conf/` finds no reader of `parachute.deploy` or `parachute.armed`
other than this module and its DEBUG telemetry message. The connection to a release
servo/actuator must be done in airframe-specific code outside the repository, so it is out
of scope of this verification (an integration-level check in DO-178C terms).

## PR-007: The 55° threshold is compared in double precision (information)
`RadOfDeg(55.f)` expands to `55.f * (M_PI/180.)`, a `double`. The float roll angle is
promoted to double for the comparison. The float nearest to 55° rounds slightly *below* the
double threshold, so a roll of exactly `(float)55°` does **not** deploy; the next float
above it does (TC-013, TC-014). This matches the "strictly greater than" requirement, so it
is not an error, but boundary tests must use the exact float values.

## Testability note
The AGL counter is a function-local `static` variable (`counter_agl_dist`). A test cannot
read or reset it directly. Each test resets it by running one AUTO cycle with an invalid AGL
sample (see `setUp()`), and the saturation at 11 (LLR-005) is checked by its observable
effect (TC-008) plus the structural coverage of the saturation statement.
