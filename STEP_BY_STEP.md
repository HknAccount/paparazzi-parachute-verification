# Step-by-step guide (Windows + WSL)

Do one step, check the "You should see" part, then go to the next step.
If something looks different, stop and send me the output.

Your folder: `E:\JOB_APPLICATIONS_V2_2026\Paparazzi_Open_source`
In Linux (WSL) the same folder is: `/mnt/e/JOB_APPLICATIONS_V2_2026/Paparazzi_Open_source`

At the end the folder will look like this:

```
Paparazzi_Open_source\
├── paparazzi\                 <- real Paparazzi code (untouched, pinned commit)
└── parachute-verification\    <- YOUR project (goes on GitHub)
```

---

## Step 1: Install WSL (Ubuntu 24.04)

Why: GCC 14 (needed for MC/DC) is easy to install on Ubuntu 24.04.

Open **PowerShell as Administrator** and run:

```powershell
wsl --install -d Ubuntu-24.04
```

Restart Windows if asked. Open "Ubuntu 24.04" from the Start menu and create a Linux
user name and password.

**You should see:** a prompt like `hari@PC:~$`

(If WSL is already installed, just run `wsl -l -v` and check that Ubuntu-24.04 is listed.)

---

## Step 2: Install the tools

In the Ubuntu terminal:

```bash
sudo apt update
sudo apt install -y gcc-14 make git python3-venv
python3 -m venv ~/v178
source ~/v178/bin/activate
pip install gcovr==8.8 strictdoc==0.30.4
```

Check:

```bash
gcc-14 --version | head -1
gcov-14 --version | head -1
gcovr --version | head -1
strictdoc --version
```

**You should see:** `gcc-14 ... 14.2.0`, `gcov ... 14.2.0`, `gcovr 8.8`, `0.30.4`

Important: every time you open a new Ubuntu terminal, run `source ~/v178/bin/activate`
first (it enables gcovr and strictdoc).

---

## Step 3: Get the real Paparazzi code (pinned version)

Do this **inside Ubuntu**, not with Git for Windows. Git for Windows converts line
endings, and then the file checksum will not match.

```bash
cd /mnt/e/JOB_APPLICATIONS_V2_2026/Paparazzi_Open_source
git init paparazzi
cd paparazzi
git remote add origin https://github.com/paparazzi/paparazzi.git
git fetch --depth 1 origin 64176905c3cbdf592a13a9c2dd08f9c121b9d235
git checkout FETCH_HEAD
sha256sum sw/airborne/modules/safety/parachute.c
```

**You should see:**
`1d0dc1a253929782587b963e03eb27ae776962097b95d17b1c9f6ac55fb03730  sw/airborne/modules/safety/parachute.c`

Why the fixed commit: in DO-178C you always verify one identified version of the code
(configuration management). If Paparazzi changes later, your results stay valid for this
version.

---

## Step 4: Read the code you are verifying (15 minutes)

Open in VS Code or Notepad++:
`E:\JOB_APPLICATIONS_V2_2026\Paparazzi_Open_source\paparazzi\sw\airborne\modules\safety\parachute.c`

Find the 3 compound decisions. These are what MC/DC is about:

1. `agl_dist_valid && (agl_dist_value_filtered < PARACHUTE_AGL_THRESHOLD)`, 2 conditions
2. `(autopilot.mode != AP_MODE_RATE_DIRECT) && (z > 30.f) && (!close_to_ground)`, 3 conditions
3. `(|phi| > 55°) || (|theta| > 55°) || (autopilot.mode == AP_MODE_KILL)`, 3 conditions

**Check yourself:** for decision 2, which 4 test cases give MC/DC?
(Answer: TTT→armed, FTT→not, TFT→not, TTF→not. Each condition is flipped alone.)

---

## Step 5: Put the project in your folder

Copy `parachute-verification.zip` into `E:\JOB_APPLICATIONS_V2_2026\Paparazzi_Open_source\`,
then in Ubuntu:

```bash
cd /mnt/e/JOB_APPLICATIONS_V2_2026/Paparazzi_Open_source
sudo apt install -y unzip
unzip parachute-verification.zip
cd parachute-verification
ls
```

**You should see:** `Makefile  README.md  STEP_BY_STEP.md  baseline.sha256  docs  reqs  scripts  test`

---

## Step 6: Requirements (HLR and LLR)

Open `reqs/HLR_parachute.sdoc` and `reqs/LLR_parachute.sdoc` and read them. There are
5 HLRs and 8 LLRs, and each LLR has a `Parent` HLR.

Build the HTML version:

```bash
make reqs
```

Then open in Windows:
`E:\JOB_APPLICATIONS_V2_2026\Paparazzi_Open_source\parachute-verification\build\reqs_html\html\index.html`

**You should see:** both documents, with clickable links from LLR to HLR.

Interview point: the code had no requirements, so you **reverse-engineered** them. The
certification authorities discussed this case in CAST-18.

---

## Step 7: Run the requirements-based tests

```bash
make test
```

**You should see:** `Baseline OK: ...` then 20 lines ending in `:PASS`, then

```
20 Tests 0 Failures 0 Ignored
OK
```

Open `test/test_parachute.c`. Each test has a tag like
`// @TC TC-011 @verifies LLR-006 @type boundary`.
There are 3 types: normal, boundary (exactly 30 m, exactly 6 m, exactly 55°) and
robustness (NaN inputs).

---

## Step 8: Structural coverage: statement, branch, MC/DC (the 100% step)

```bash
make coverage
```

**You should see** (end of output):

```
File                         Lines  Exec  Cover  Branches  Taken  Cover  Conditions  Taken  Cover
modules/safety/parachute.c      32    32   100%        24     24   100%          24     24   100%
```

And in the raw gcov output: `Condition outcomes covered:100.00% of 24`

Open the visual report in Windows:
`...\parachute-verification\build\coverage\index.html`
Click `parachute.c`. Every line is green and each decision shows its conditions.

The raw gcov file is `build/parachute.c.gcov`. Search for `condition outcomes covered`:
there are 7 decisions, and all are fully covered.

---

## Step 9: Traceability matrix

```bash
make trace
```

**You should see:** a table HLR → LLR → test cases → PASS, ending with
`No traceability gaps: every HLR is refined by an LLR, every LLR is verified by at least one test, and every test passed.`

The file is saved at `build/traceability_matrix.md`.

---

## Step 10: Prove the tests really find bugs (mutation check)

100% coverage only shows the code was *executed*. This step shows the tests *check* it.

```bash
make mutation
```

**You should see:** 5 lines `DETECTED` and `Mutation score: 5/5 faults detected`

The script changes a *copy* of parachute.c (e.g. `> 30` to `>= 30`) and runs the
tests. The original file is never edited.

---

## Step 11: Read the findings

Open `docs/findings.md`. It lists 7 observations (PR-001 to PR-007). The most
interesting ones:

- PR-001: `deploy` is not latched. It goes back to false when the attitude recovers.
- PR-003: NaN AGL distance is treated as "not close to the ground".
- PR-006: nothing in the repository reads `parachute.deploy`.

Interview point: you found these by writing requirements and robustness tests, not by
chasing coverage.

---

## Step 12: Run everything at once

```bash
make clean
make all
```

**You should see:** tests pass, 100% coverage, no traceability gaps, StrictDoc export done.

---

## Step 13: Put it on GitHub

1. On github.com create a new **empty** public repository named
   `paparazzi-parachute-do178c-verification` (no README, no license; you already have them).
2. In Ubuntu:

```bash
cd /mnt/e/JOB_APPLICATIONS_V2_2026/Paparazzi_Open_source/parachute-verification
git init
git add .
git commit -m "DO-178C-style verification of Paparazzi parachute module: HLR/LLR, RBT, 100% MC/DC"
git branch -M main
git remote add origin https://github.com/<your-github-user>/paparazzi-parachute-do178c-verification.git
git push -u origin main
```

GitHub asks for a password: use a **Personal Access Token** (GitHub → Settings →
Developer settings → Tokens), not your account password.

3. On GitHub open the **Actions** tab. The `verify` workflow runs the whole pipeline and
   should turn green. Click the run and download `verification-evidence`: it contains
   the coverage report and the traceability matrix.

**You should see:** a green check next to your commit.

---

## Step 14: Use it

- CV line: *"DO-178C-style verification of open-source UAV flight code (Paparazzi
  parachute safety function): reverse-engineered HLR/LLR, 20 requirements-based tests,
  100% statement/decision/MC/DC coverage (GCC 14), traceability matrix, CI pipeline."*
- Send the GitHub link to Akshay: he suggested exactly this kind of project.
- Next level (optional): add a Frama-C WP proof that `check_parachute_trigger()` always
  sets `deploy` as LLR-008 says. That combines testing and formal methods, which is your
  strength.
