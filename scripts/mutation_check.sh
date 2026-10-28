#!/usr/bin/env bash
# Mutation check: inject realistic faults into a COPY of parachute.c and confirm
# that the requirements-based tests detect each one. The baselined file is never edited.
#
# Usage: scripts/mutation_check.sh <path to paparazzi clone>
set -u
PPRZ="${1:-../paparazzi}"
SRC="$PPRZ/sw/airborne/modules/safety/parachute.c"
WORK="build/mutation"
CFLAGS="-std=c99 -O0 -w -DUNITY_INCLUDE_DOUBLE -Itest/stubs -Itest/unity/src -I$PPRZ/sw/airborne -I$PPRZ/sw/include"

mutants=(
  's/z > 30.f/z >= 30.f/::altitude check: > 30 m changed to >= 30 m'
  's/counter_agl_dist > PARACHUTE_AGL_COUNTER_TRIGGER/counter_agl_dist >= PARACHUTE_AGL_COUNTER_TRIGGER/::AGL counter: > 10 changed to >= 10'
  's/(fabsf(euler->theta) > PARACHUTE_ATT_ANGLE_THRESHOLD)/(fabsf(euler->phi) > PARACHUTE_ATT_ANGLE_THRESHOLD)/::copy-paste bug: pitch check tests roll'
  's/agl_dist_valid \&\& /agl_dist_valid || /::AGL decision: && changed to ||'
  's/(!close_to_ground)/(close_to_ground)/::arming: missing negation of close_to_ground'
)

mkdir -p "$WORK"
killed=0
total=0
for m in "${mutants[@]}"; do
  expr="${m%%::*}"
  desc="${m#*::}"
  total=$((total + 1))
  sed "$expr" "$SRC" > "$WORK/parachute_mut.c"
  if cmp -s "$SRC" "$WORK/parachute_mut.c"; then
    echo "M$total NOT APPLIED  ($desc)"
    continue
  fi
  gcc-14 $CFLAGS "$WORK/parachute_mut.c" test/unity/src/unity.c test/test_parachute.c -lm -o "$WORK/run" || {
    echo "M$total BUILD ERROR ($desc)"; continue; }
  if "$WORK/run" > "$WORK/out.txt"; then
    echo "M$total SURVIVED     ($desc)  <-- tests did not detect this fault"
  else
    killed=$((killed + 1))
    failed=$(grep -c ":FAIL" "$WORK/out.txt")
    echo "M$total DETECTED     ($desc)  by $failed failing test(s)"
  fi
done
echo "Mutation score: $killed/$total faults detected"
[ "$killed" -eq "$total" ]
