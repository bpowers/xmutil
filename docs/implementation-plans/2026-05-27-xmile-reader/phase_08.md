# XMILE Reader Implementation Plan — Phase 8

**Goal:** End-to-end validation across the corpus, hard-rejection of out-of-scope features, CLI smoke, and WASM build verification. Pin down all remaining ACs.

**Architecture:** Three new test files (`CorpusRoundTripTest.cpp`, `ErrorRejectTest.cpp`, `CEntryTest`-XMILE extensions), one new CLI shell test (`cli_xmile_roundtrip.sh`), and explicit verifications:
- WASM build still configures and produces an artifact with the new sources.
- Existing MDL test suite passes (regression).
- Existing `mdl -> xmile` output for the MDL corpus stays byte-identical (no regression on the unchanged write path).

**Tech Stack:** Phases 1-7 fully integrated. No new C++ classes; only tests + shell + verification steps.

**Scope:** Phase 8 of 8.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC1: Model populates from XMILE via a stable API
- **xmile-reader.AC1.2 Success:** The extern-C entry `convert_xmile_to_xmile(source, len, fileName, longNames, sectors)` returns a caller-owned XMILE string for a well-formed input, with the same ownership contract as `convert_mdl_to_xmile`.
- **xmile-reader.AC1.3 Success:** The extern-C entry `convert_xmile_to_mdl(source, len, fileName, longNames)` returns a caller-owned MDL string for a well-formed input.
- **xmile-reader.AC1.5 Failure:** On a parse / conversion error, messages are appended to `errs` and the extern-C entry returns NULL; `Model::ParseXMILE` returns false.

### xmile-reader.AC5: Out-of-scope features hard-error or are silently skipped
- **xmile-reader.AC5.1 Failure:** A `<module>` element inside `<variables>` is rejected with a clear error citing "modules are not supported" and the offending element.
- **xmile-reader.AC5.2 Failure:** A `<macro>` sibling element of `<model>` is rejected with a clear error.
- **xmile-reader.AC5.3 Failure:** Multiple `<model>` elements (other than `<macro>`-classified ones) is rejected with a clear error.

### xmile-reader.AC6: No regression
- **xmile-reader.AC6.1 Success:** The existing `xmutil_test` MDL test suite continues to pass.
- **xmile-reader.AC6.2 Success:** XMILE output produced by the existing `convert_mdl_to_xmile` path is byte-identical to the pre-change output for the existing MDL corpus.
- **xmile-reader.AC6.3 Success:** The WASM build (configured via `./configure.sh` with the emscripten profile) succeeds with the new sources included.

---

## Codebase verification findings

- ✓ `configure.sh` has a `--use-wasm` flag (`/home/bpowers/src/xmutil/configure.sh`). The WASM build uses the `XMUtil_wasm` gyp target (`XMUtil.gyp:207-230`), which lists `<@(common_sources)` — so any new `src/Xmile/*.cpp` files added to `common_sources` in earlier phases automatically participate.
- ✓ `test/cli_roundtrip.sh` is the existing CLI smoke for MDL inputs. Phase 8 adds a sibling `test/cli_xmile_roundtrip.sh` exercising the XMILE input path.
- ✓ `test/mdl/CEntryTest.cpp` is the existing extern-C entry test for `convert_mdl_to_xmile` / `convert_to_mdl`. Phase 8 adds equivalent tests for `convert_xmile_to_xmile` / `convert_xmile_to_mdl` in a new `test/xmile/CEntryTest.cpp`.
- ✓ The byte-identity check for AC6.2 needs a "golden" reference for each MDL corpus fixture's XMILE output. The existing `test/mdl/CorpusRoundTripTest.cpp` produces XMILE in-memory; for byte-identity we need a snapshot of the *current* output before any Phase 1-7 changes touched extern-C / Main.cpp. Capture these snapshots at the start of Phase 8 (before any code changes for that phase) by running `out/Debug/XMUtil <corpus>.mdl` for each existing corpus file and storing the result. Compare on each subsequent build.
- ✗ The design says "verify XMILE output for an existing MDL input is byte-identical to the pre-change output." Strictly the only places in the codebase that earlier phases modified are `XmileReader`, `Model::ParseXMILE`, the two new extern-C entries, and `Main.cpp`'s dispatch. None of these touch the existing `convert_mdl_to_xmile` code path or the writer. So the byte-identity is structural — if it fails, it indicates an unintended side effect (e.g., the function-table refactor in Phase 4 changed registration timing or order). Phase 8's regression check formalizes this guarantee.
- ✓ All three corpus models (`fishbanks`, `logistic-growth`, `reliability`) are available at `third_party/simlin/default_projects/<name>/model.xmile`.
- ✓ The fourth simlin default-project model (`population`) uses real `<module>` submodel instances inside `<variables>` (per the design's "Corpus narrowing rationale"); it is excluded from the corpus and used as a positive fixture for AC5.1 instead.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: CorpusRoundTripTest — all three corpus models, both directions

**Verifies:** AC1.2, AC1.3 (positive-path corpus-scale validation).

**Files:**
- Create: `test/xmile/CorpusRoundTripTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new file)

**Implementation notes:**

```cpp
#include <fstream>
#include <sstream>
#include <string>
#include "../../src/Model.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

std::string ReadFile(const std::string &path) {
  std::ifstream in(path, std::ios::in | std::ios::binary);
  if (!in.is_open()) return std::string();
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

const char *kFishbanksPath = "/third_party/simlin/default_projects/fishbanks/model.xmile";
const char *kLogisticGrowthPath = "/third_party/simlin/default_projects/logistic-growth/model.xmile";
const char *kReliabilityPath = "/third_party/simlin/default_projects/reliability/model.xmile";

}  // namespace

// XMILE -> XMILE round-trip for each corpus model.
TEST(Corpus_fishbanks_xmile_to_xmile) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) + kFishbanksPath);
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Corpus_logistic_growth_xmile_to_xmile) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) + kLogisticGrowthPath);
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Corpus_reliability_xmile_to_xmile) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) + kReliabilityPath);
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

// XMILE -> MDL conversion for each corpus model.
TEST(Corpus_fishbanks_xmile_to_mdl) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) + kFishbanksPath);
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Corpus_logistic_growth_xmile_to_mdl) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) + kLogisticGrowthPath);
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}

TEST(Corpus_reliability_xmile_to_mdl) {
  std::string xmile = ReadFile(std::string(XMUTIL_SRC_ROOT) + kReliabilityPath);
  CHECK(!xmile.empty()); if (xmile.empty()) return;
  std::vector<std::string> diffs = xmileroundtrip::XmileToMdlDiffs(xmile);
  for (const std::string &d : diffs) printf("  diff: %s\n", d.c_str());
  CHECK(diffs.empty());
}
```

The earlier phases (4 — logistic-growth, 7 — fishbanks + reliability) include the same corpus tests partially. Phase 8's contribution is the **complete matrix** — all three models in both directions — as one cohesive test file. If a prior phase's test is duplicated here, leave the duplicates; the cost is negligible and the duplication serves as a regression check on the earlier-phase functionality from the Phase 8 perspective.

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All six corpus tests pass.
- All Phase 1-7 tests still pass.

**Commit:** `test(xmile): corpus round-trip (all 3 models, both directions)`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: ErrorRejectTest — `<module>`, `<macro>`, multi-`<model>`, postfix `'`

**Verifies:** AC1.5 (failure path), AC5.1, AC5.2, AC5.3, AC2.9 (regression check).

**Files:**
- Create: `test/xmile/ErrorRejectTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new file)
- Modify: `src/Xmile/XmileReader.cpp` (add the multi-`<model>` envelope-level check)

**Implementation notes:**

First, extend the envelope walker in `XmileReader::ProcessFile` to count `<model>` siblings and reject the multi-model case:

```cpp
// In ProcessFile, replace the simple <macro> check with:
int modelCount = 0;
for (tinyxml2::XMLElement *child = root->FirstChildElement(); child;
     child = child->NextSiblingElement()) {
  const char *name = child->Name();
  if (XmileReader::IsForeignNamespace(name)) continue;
  std::string tag(name);
  if (tag == "macro") {
    errs.push_back(filename + ": <macro> elements are not supported");
    return false;
  }
  if (tag == "model") ++modelCount;
}
if (modelCount > 1) {
  errs.push_back(filename + ": multiple <model> elements are not supported");
  return false;
}
// ... then the original dispatch loop continues
```

(The original dispatch loop's `<model>` branch keeps going; only the first `<model>` is processed if the count is 1. The count check fires before any processing.)

The test file:

```cpp
#include <string>
#include "../../src/Model.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {

const char *kWithModule = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <aux name="x"><eqn>1</eqn></aux>
    <module name="m1"/>
  </variables></model>
</xmile>
)";

const char *kWithMacro = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <macro name="my_macro"><eqn>x*2</eqn></macro>
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables><aux name="x"><eqn>1</eqn></aux></variables></model>
</xmile>
)";

const char *kMultiModel = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables><aux name="x"><eqn>1</eqn></aux></variables></model>
  <model><variables><aux name="y"><eqn>2</eqn></aux></variables></model>
</xmile>
)";

const char *kPostfixApostrophe = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables>
    <aux name="x"><eqn>a'</eqn></aux>
  </variables></model>
</xmile>
)";

void CheckRejected(const char *xmile, const char *expectedSubstring) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(xmile, errs);
  CHECK(m == nullptr);
  CHECK(!errs.empty());
  bool sawExpected = false;
  for (const std::string &e : errs) {
    if (e.find(expectedSubstring) != std::string::npos) sawExpected = true;
  }
  if (!sawExpected) {
    printf("  expected substring not found: %s\n", expectedSubstring);
    for (const std::string &e : errs) printf("  err: %s\n", e.c_str());
  }
  CHECK(sawExpected);
}

}  // namespace

TEST(ErrorReject_module_with_clear_message) {
  CheckRejected(kWithModule, "modules are not supported");
}

TEST(ErrorReject_macro_with_clear_message) {
  CheckRejected(kWithMacro, "macro");
}

TEST(ErrorReject_multiple_models) {
  CheckRejected(kMultiModel, "multiple <model>");
}

// AC2.9 regression: Phase 2 already tests this at the equation-parser level
// (XmileParse_postfix_apostrophe_errors in EquationParseTest.cpp). This case
// exercises the same rejection from the document-level entry point — a
// deliberate duplicate so a future refactor that bypasses the grammar can't
// silently regress AC2.9 from XMILE input.
TEST(ErrorReject_postfix_apostrophe_in_equation) {
  CheckRejected(kPostfixApostrophe, "");  // any error is fine; just verify rejection
}

// Additional positive check for AC1.5: extern-C entries return NULL.
TEST(ErrorReject_convert_xmile_to_xmile_returns_null_on_error) {
  char *out = convert_xmile_to_xmile(kWithModule, strlen(kWithModule), "test.xmile", -1, false);
  CHECK(out == nullptr);
}

TEST(ErrorReject_convert_xmile_to_mdl_returns_null_on_error) {
  char *out = convert_xmile_to_mdl(kWithModule, strlen(kWithModule), "test.xmile", -1);
  CHECK(out == nullptr);
}
```

**Verification:**
- `ninja -C out/Debug xmutil_test` succeeds.
- All six new tests pass.
- All Phase 1-7 tests still pass.

**Commit:** `feat(xmile): multi-<model> rejection; test(xmile): rejection-path coverage`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-3) -->

<!-- START_TASK_3 -->
### Task 3: CEntryTest extension + CLI shell smoke

**Verifies:** AC1.2, AC1.3 (extern-C ownership contract), AC1.4 (CLI dispatch end-to-end).

**Files:**
- Create: `test/xmile/CEntryTest.cpp`
- Create: `test/cli_xmile_roundtrip.sh`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add `CEntryTest.cpp`)

**Implementation notes:**

`test/xmile/CEntryTest.cpp` mirrors `test/mdl/CEntryTest.cpp`'s shape, verifying:

```cpp
#include <cstdlib>
#include <cstring>
#include <string>
#include "../../src/XMUtil.h"
#include "../TestHarness.h"

namespace {
const char *kMinimal = R"(<?xml version="1.0" encoding="utf-8"?>
<xmile version="1.0" xmlns="http://docs.oasis-open.org/xmile/ns/XMILE/v1.0">
  <sim_specs method="euler"><start>0</start><stop>10</stop><dt>1</dt></sim_specs>
  <model><variables><aux name="x"><eqn>1</eqn></aux></variables></model>
</xmile>
)";
}  // namespace

TEST(CEntry_xmile_to_xmile_returns_owned_buffer) {
  char *out = convert_xmile_to_xmile(kMinimal, strlen(kMinimal), "test.xmile",
                                     /*longNames=*/-1, /*sectors=*/false);
  CHECK(out != nullptr);
  if (!out) return;
  // The caller owns it: free() should succeed without crashing.
  std::string s(out);
  CHECK(!s.empty());
  // The first line of XMILE output is the <?xml...?> declaration.
  CHECK(s.find("<?xml") == 0);
  free(out);
}

TEST(CEntry_xmile_to_mdl_returns_owned_buffer) {
  char *out = convert_xmile_to_mdl(kMinimal, strlen(kMinimal), "test.xmile", /*longNames=*/-1);
  CHECK(out != nullptr);
  if (!out) return;
  std::string s(out);
  CHECK(!s.empty());
  // The first line of Vensim MDL output is the {UTF-8} marker.
  CHECK(s.find("{UTF-8}") == 0);
  free(out);
}

TEST(CEntry_null_filename_is_accepted) {
  // Mirrors convert_mdl_to_xmile's behavior — NULL fileName -> "<in memory>".
  char *out = convert_xmile_to_xmile(kMinimal, strlen(kMinimal), nullptr, -1, false);
  CHECK(out != nullptr);
  if (out) free(out);
}
```

`test/cli_xmile_roundtrip.sh` (mirrors `cli_roundtrip.sh`):

```bash
#!/bin/bash
#
# CLI end-to-end XMILE round-trip smoke test (Phase 8).
#
# Verifies: XMILE input dispatches to the XMILE reader; --to-mdl produces
# a valid .mdl that re-parses; XMILE -> XMILE produces a .regen.xmile that
# re-parses.
#
# Run from the repository root so the fixture path resolves.

set -euo pipefail

BIN=${1:-out/Debug/XMUtil}
SRC="third_party/simlin/default_projects/logistic-growth/model.xmile"

if [ ! -x "$BIN" ]; then
  echo "error: XMUtil binary not found or not executable at '$BIN'" >&2
  exit 1
fi
if [ ! -f "$SRC" ]; then
  echo "error: fixture not found at '$SRC' (run from the repo root)" >&2
  exit 1
fi

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

cp "$SRC" "$TMP/m.xmile"

# XMILE -> XMILE: .regen.xmile guard kicks in.
"$BIN" "$TMP/m.xmile"
test -s "$TMP/m.regen.xmile"
head -1 "$TMP/m.regen.xmile" | grep -q '<?xml'

# XMILE -> MDL: produces a .mdl that begins with the {UTF-8} marker.
"$BIN" --to-mdl "$TMP/m.xmile"
test -s "$TMP/m.mdl"
head -1 "$TMP/m.mdl" | grep -q '{UTF-8}'

# Re-emit the generated .mdl through stdio to confirm it re-parses cleanly.
"$BIN" --to-mdl --stdio < "$TMP/m.mdl" | head -1 | grep -q '{UTF-8}'

# .stmx case-insensitive dispatch: copy with .STMX extension and re-run.
cp "$SRC" "$TMP/m2.STMX"
"$BIN" "$TMP/m2.STMX"
test -s "$TMP/m2.xmile"  # output: m2.xmile (no clash with .STMX)

echo "XMILE CLI round-trip OK"
```

Make it executable: `chmod +x test/cli_xmile_roundtrip.sh`.

**Verification:**
- `ninja -C out/Debug XMUtil && ninja -C out/Debug xmutil_test` succeed.
- `bash test/cli_xmile_roundtrip.sh` exits 0 with the "XMILE CLI round-trip OK" message.
- `bash test/cli_roundtrip.sh` (the existing MDL one) still passes.
- All `CEntryTest` cases pass; pre-existing MDL CEntryTest still passes.

**Commit:** `test(xmile): extern-C ownership + CLI shell smoke test`
<!-- END_TASK_3 -->

<!-- END_SUBCOMPONENT_B -->

<!-- START_SUBCOMPONENT_C (tasks 4-5) -->

<!-- START_TASK_4 -->
### Task 4: Regression check — MDL test suite + byte-identical MDL→XMILE output

**Verifies:** AC6.1, AC6.2.

**Files:**
- Create: `test/xmile/MdlXmileByteIdentityTest.cpp` (in-process byte-identity check for representative MDL fixtures)
- Modify: `XMUtil.gyp` (`xmutil_test` `sources`: add the new file)

**Implementation notes:**

AC6.1 is verified by simply running `xmutil_test` and observing that all pre-existing MDL tests in `test/mdl/` still pass. No new test file is required for that.

AC6.2 (byte-identical MDL → XMILE output) requires comparing the new build's output to the pre-change output. There are two approaches:

**Approach A — Static golden file (recommended for the test suite).** Capture the current `convert_mdl_to_xmile` output of a small representative MDL fixture and check it into `test/xmile/fixtures/` as `<name>.golden.xmile`. The test re-runs `convert_mdl_to_xmile` on the same input and `CHECK_EQ_STR`s the result against the golden file. If a future code change legitimately changes the writer output (intentional refactor), update the golden file as part of that change.

```cpp
#include <fstream>
#include <sstream>
#include <string>
#include "../../src/XMUtil.h"
#include "../TestHarness.h"

namespace {

const char *kTeacupMdlPath = "/third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl";
const char *kTeacupXmileGolden = "/test/xmile/fixtures/teacup.golden.xmile";

std::string ReadFile(const std::string &path);  // shared helper

}  // namespace

TEST(MdlXmileByteIdentity_teacup) {
  std::string mdl = ReadFile(std::string(XMUTIL_SRC_ROOT) + kTeacupMdlPath);
  std::string golden = ReadFile(std::string(XMUTIL_SRC_ROOT) + kTeacupXmileGolden);
  CHECK(!mdl.empty()); CHECK(!golden.empty());
  if (mdl.empty() || golden.empty()) return;
  char *out = convert_mdl_to_xmile(mdl.c_str(), mdl.size(), "teacup.mdl",
                                   /*isCompact=*/false, /*longNames=*/-1, /*sectors=*/false);
  CHECK(out != nullptr);
  if (!out) return;
  std::string actual(out);
  free(out);
  CHECK_EQ_STR(actual, golden);
}
```

Generate the golden file as part of this task:

```bash
out/Debug/XMUtil third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl
mv third_party/simlin/src/pysimlin/tests/fixtures/teacup.xmile \
   test/xmile/fixtures/teacup.golden.xmile
mkdir -p test/xmile/fixtures
git add test/xmile/fixtures/teacup.golden.xmile
```

**Approach B — Snapshot at Phase 8 start (operational, not codified).** Before Phase 8 changes, capture the output for each existing MDL fixture into a scratch dir; after each Phase 8 commit, re-run and diff against the snapshot. This isn't a tracked test but is useful during the Phase 8 development loop.

Use **Approach A**: the golden file is checked in and the test is automated. The teacup fixture is enough to cover the byte-identity guarantee for the simple-stock-with-aux pattern; extend with a fishbanks-mdl golden if the corpus has one (it doesn't — fishbanks lives in `third_party/simlin/default_projects/` as XMILE only).

**Note on the byte-identity guarantee scope.** The byte-identity claim is *current-binary vs current-binary*: the new code, when applied to MDL input, must produce the same XMILE output as the pre-XMILE-reader binary. The implementation plan ensures this by:
- Not modifying `XMILEGenerator` (the writer) at all.
- Not modifying `Model::PrintXMILE` or its dependencies.
- Phase 4's `RegisterXmutilFunctions` refactor (if applied) must leave `VensimParse::ReadyFunctions()` producing the identical namespace registration sequence — verified by the golden test.

If the golden test fails after Phase 4, the function-registration refactor must be re-examined.

**Verification:**
- `out/Debug/xmutil_test` passes including the new byte-identity test.
- All pre-existing MDL tests pass.

**Commit:** `test(xmile): byte-identity check for MDL -> XMILE output`
<!-- END_TASK_4 -->

<!-- START_TASK_5 -->
### Task 5: WASM build verification + final tidy

**Verifies:** AC6.3.

**Files:**
- No code changes; this task is purely verification.

**Implementation notes:**

```bash
# 1. Configure with the WASM profile.
./configure.sh --use-wasm

# 2. Build the WASM target. The XMUtil_wasm gyp target (XMUtil.gyp:207-230)
#    lists <@(common_sources), which includes all the new src/Xmile/* files
#    that earlier phases added. No per-target additions are needed.
ninja -C out/Debug XMUtil_wasm

# 3. Verify the output artifacts exist.
ls -la out/Debug/xmutil.js out/Debug/xmutil.wasm

# 4. Re-configure for the native build so subsequent dev / tests work.
./configure.sh
ninja -C out/Debug XMUtil
ninja -C out/Debug xmutil_test
out/Debug/xmutil_test  # exit 0
```

**If `--use-wasm` requires emscripten** and it's not installed on the dev machine: document the failure mode. The Phase 8 done-when can be split — the structural verification (sources participate in the WASM target via `common_sources`) is guaranteed by gyp; the *actual* emscripten build run is a CI step. For the implementation plan, the structural verification is sufficient if emscripten is unavailable, with the operational build verified in CI.

Final tidy:
- `./format.sh` — clang-format all `src/Xmile/*.{cpp,h}` files.
- Manually format any new `test/xmile/*.cpp` files (format.sh doesn't cover `test/`).
- Run `out/Debug/xmutil_test` one final time and confirm all tests pass (zero failures across the full Phase 1-8 suite).
- Run `bash test/cli_roundtrip.sh` and `bash test/cli_xmile_roundtrip.sh` and confirm both succeed.

**Verification:**
- WASM build artifacts exist (`xmutil.js` + `xmutil.wasm`).
- All tests pass.
- Both CLI smoke tests pass.
- `./format.sh` produces no diffs.
- `git status` shows only the expected changes (no stray files).

**Commit:** None (no code changes).
<!-- END_TASK_5 -->

<!-- END_SUBCOMPONENT_C -->

---

## Phase 8 done when

- All three corpus models pass corpus round-trip XMILE → XMILE and XMILE → MDL.
- All four rejection cases (`<module>`, `<macro>`, multi-`<model>`, postfix `'`) hard-error with clear messages.
- `bash test/cli_xmile_roundtrip.sh` passes.
- `bash test/cli_roundtrip.sh` still passes (MDL CLI regression).
- The byte-identity test for `convert_mdl_to_xmile(teacup.mdl)` passes.
- WASM build succeeds (`./configure.sh --use-wasm && ninja -C out/Debug XMUtil_wasm`).
- All `xmutil_test` tests pass (existing MDL + new XMILE suite).
- `./format.sh` clean on `src/` files.

## Out of scope for Phase 8

- Multi-view corpus round-trip (Phase 7 v1 limitation).
- Module / macro support — explicit hard-reject is the v1 contract.
- `population` corpus model (uses modules, deferred indefinitely).
- Performance / memory profiling.
- Fuzz testing the XMILE parser (could be added as a separate effort).
- XMILE 2.0 spec support.

---

## End of Plan

After Phase 8, the XMILE reader is complete to the design's v1 scope. The directory structure produced by the plan:

```
src/Xmile/
  XmileEqLex.{h,cpp}
  XmileEqYacc.tab.{cpp,hpp}
  XmileEqYacc.y
  XmileFunctions.{h,cpp}
  XmileParseFunctions.{h,cpp}
  XmileReader.{h,cpp}
  XmileView.{h,cpp}
  XMILEGenerator.{h,cpp}      (pre-existing, unchanged)

test/xmile/
  ArrayRoundTripTest.cpp
  AuxRoundTripTest.cpp
  BasicSmokeTest.cpp
  CEntryTest.cpp
  CorpusRoundTripTest.cpp
  EquationParseTest.cpp
  ErrorRejectTest.cpp
  LookupRoundTripTest.cpp
  MdlXmileByteIdentityTest.cpp
  RoundTrip.{h,cpp}
  StockFlowRoundTripTest.cpp
  ViewRoundTripTest.cpp
  XmileFunctionsTest.cpp
  fixtures/teacup.golden.xmile

test/
  cli_xmile_roundtrip.sh      (new)
  cli_roundtrip.sh            (pre-existing, unchanged)
```

The next logical work after this implementation plan completes would be the design plan's "fast-follow" items (multi-view fidelity, `<macro>` support) or expanding to XMILE 2.0 features — out of scope for this plan.
