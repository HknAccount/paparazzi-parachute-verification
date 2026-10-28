# DO-178C-style verification of the Paparazzi UAV parachute safety function

Requirements-based testing, structural coverage up to **MC/DC**, and bidirectional
traceability for real open-source flight code: the automatic parachute deployment
module of the [Paparazzi UAV](https://github.com/paparazzi/paparazzi) autopilot
(`sw/airborne/modules/safety/parachute.c`), using only open-source tools.

> Portfolio project. Paparazzi is research/hobby UAV software and is not certified.
> This project applies the *verification process* of DO-178C to it as an exercise. It does
> not claim DO-178C compliance, and it quotes no text from the (copyrighted) standard.

## Results

| Evidence | Result |
|---|---|
| Requirements | 5 HLR, 8 LLR (reverse-engineered from the code) |
| Requirements-based tests | 20 test cases (normal, boundary, robustness): **20/20 pass** |
| Statement coverage | **100%** (32/32 lines) |
| Decision/branch coverage | **100%** (24/24 branches) |
| MC/DC (GCC masking MC/DC) | **100%** (24/24 condition outcomes, 7 decisions) |
| Traceability | HLR → LLR → test → result, no gaps |
| Test effectiveness | 5/5 injected faults detected (`make mutation`) |
| Findings | 7 problem reports, see [docs/findings.md](docs/findings.md) |

## What maps to what

| DO-178C activity (objective tables named, not quoted) | In this project |
|---|---|
| High-level and low-level requirements (Tables A-2, A-3, A-4) | `reqs/HLR_parachute.sdoc`, `reqs/LLR_parachute.sdoc` (StrictDoc) |
| Requirements-based testing: normal and robustness (Table A-6) | `test/test_parachute.c` (Unity), tagged by LLR |
| Test coverage of requirements (Table A-7) | `scripts/trace_matrix.py` → `build/traceability_matrix.md` |
| Structural coverage: statement, decision, MC/DC (Table A-7) | GCC 14 `-fcondition-coverage` + `gcov --conditions`, gcovr report |
| Configuration management (Table A-8) | Paparazzi pinned to commit `64176905`, files checked against `baseline.sha256` |
| Problem reporting | `docs/findings.md` |

| Industry tool (commercial) | Open-source tool used here |
|---|---|
| IBM DOORS | StrictDoc (requirements as text in Git) |
| VectorCAST / LDRA TBrun | Unity test framework (MIT) |
| LDRA / VectorCAST coverage | GCC 14 gcov MC/DC + gcovr (BSD-3) |
| Tool-generated trace reports | `scripts/trace_matrix.py` |

## Software under test

- Paparazzi commit `64176905c3cbdf592a13a9c2dd08f9c121b9d235` (GPLv2+)
- The upstream file is compiled **unmodified**, straight from the Paparazzi clone. Its
  dependencies (autopilot mode, position, attitude) are replaced by a small test stub
  (`test/stubs/firmwares/rotorcraft/autopilot_firmware.h`), so every input can be controlled.
- Decisions covered for MC/DC:
  - `agl_dist_valid && (agl_dist_value_filtered < 6.0)`
  - `(mode != RATE_DIRECT) && (z > 30) && (!close_to_ground)`
  - `|roll| > 55° || |pitch| > 55° || mode == KILL`
  - plus four single-condition decisions

## How to run

Linux or WSL (Ubuntu 24.04), with `gcc-14`, Python 3, `gcovr` and `strictdoc`:

```bash
git clone https://github.com/paparazzi/paparazzi.git ../paparazzi
git -C ../paparazzi checkout 64176905c3cbdf592a13a9c2dd08f9c121b9d235
make all
```

Outputs in `build/`:

- `test_results.txt`: Unity test log
- `gcov_summary.txt`, `parachute.c.gcov`: raw gcov output with per-decision MC/DC
- `coverage_summary.txt`, `coverage/index.html`: gcovr line/branch/condition report
- `traceability_matrix.md`: HLR → LLR → test → result
- `reqs_html/html/index.html`: requirements documents

The build fails if coverage drops below 100%, if a test fails, or if there is a
traceability gap. The GitHub Actions workflow runs the same pipeline on every push.

## Notes on the MC/DC result

- GCC measures **masking MC/DC**, the form whose use in certification was discussed by
  the certification authorities in CAST-6.
- Every decision in this module is a pure `&&` chain or a pure `||` chain. For such
  "tree-shaped" decisions, object-level branch coverage and MC/DC coincide, so both
  reach 100% with the same test set. A decision like `(A && B) || C` would show the
  difference.
- 100% coverage shows that the tests exercised all the code structure. It does not prove
  that the code is correct. That comes from the requirements-based tests, which is why each
  test checks an LLR and the mutation check is included.

## Free references used

- Hayhurst et al., *A Practical Tutorial on Modified Condition/Decision Coverage*,
  NASA/TM-2001-210876
- FAA AC 20-115D and AC 00-69
- CAST-6 (masking MC/DC), CAST-10 (what is a decision), CAST-15 (HLR vs LLR),
  CAST-18 (reverse engineering): archived FAA position papers
- DOT/FAA/AR-01-18, *An Investigation of Three Forms of the MC/DC Criterion*

## License

Test code, scripts and documents: GPL-2.0-or-later, to match Paparazzi.
Unity (`test/unity/`): MIT, © ThrowTheSwitch.org.
