# Vensim mdl Writer Implementation Plan

**Goal:** Add a C++ `MDLGenerator` that serializes xmutil's in-memory `Model` back to Vensim `.mdl` text, enabling Vensim->Vensim (normalize) and Dynamo->Vensim conversion, mirroring the existing `XMILEGenerator`.

**Architecture:** A new `MDLGenerator` class in `src/Mdl/` consumes the post-`MarkVariableTypes` `Model` (the same prepared state `XMILEGenerator` consumes) and assembles `.mdl` text section by section: optional `:MACRO:` blocks, the equation section (dimensions, grouped variables, `.Control` sim specs), the `\\\---///` sketch (re-serialized from the parsed `VensimView`; never synthesized), and the `:L...` settings tail. It includes its own recursive Vensim expression walker (no reuse of the XMILE-specific `OutputComputable`). Output uses CRLF. Format-specific algorithms with no xmutil precedent are ported from simlin's vendored Rust `mdl/writer.rs`.

**Tech Stack:** C++17, gyp build (`XMUtil.gyp`; no CMake in main repo), clang-format (Google base, 120 cols, right-aligned pointers, run via `./format.sh`). No third-party test framework — a minimal custom test harness is introduced in this phase. Reference algorithm: `third_party/simlin/src/simlin-engine/src/mdl/writer.rs`.

**Scope:** 7 phases (full design). This is Phase 1 of 7.

**Codebase verified:** 2026-05-22 (via codebase-investigator over `src/Xmile/`, `src/Model.{h,cpp}`, `src/XMUtil.{h,cpp}`, `src/Main.cpp`, `XMUtil.gyp`, `build/common.gypi`).

---

## Decisions baked into this plan (read first)

These resolve discrepancies found between the design and the actual codebase. They were confirmed with the user:

1. **Build wiring is gyp-only.** The main repo has **no root `CMakeLists.txt`** (the only CMake is the off-limits `third_party/bpowers-xmutil` fork). New files go into `XMUtil.gyp` only. (A separate, already-completed task verified/fixed the gyp build on this machine.)
2. **Tests use a minimal custom harness + a new gyp executable target** (`xmutil_test`). No gtest/Catch2/doctest. The project had zero tests before this work.
3. **The expression walker ports simlin's precedence/parenthesization logic** (Phase 2), rather than relying on preserved `ExpressionParen` nodes.
4. **`MDLGenerator` mirrors `XMILEGenerator` structurally, not literally.** `XMILEGenerator` builds a `tinyxml2` DOM; `MDLGenerator` builds a text string. The "mirror" is: a `Model*`-constructed class with `Print(...) -> std::string` collecting errors in a `std::vector<std::string>&`, plus per-section private builder methods.
5. **Use `Variable::GetName()` (original Vensim name) for all Vensim output** — never `GetAlternateName()` or `SpaceToUnderBar()` (those are XMILE-specific). Confirmed at `src/Symbol/Variable.cpp:70-76` and `src/XMUtil.cpp:24-33`.
6. **The C entry is named `convert_to_mdl`** and the CLI flag `--to-mdl`, per design AC1.2/AC1.3.
7. **Deep equation-AST comparison in `ModelComparator` is added in Phase 3** (it depends on the value/operand getters added in Phase 2). Phase 1 builds the comparator framework plus all non-expression comparisons (variable presence, type, units, comment, subscripts, dimensions, sim specs, groups, view geometry), which is sufficient for the AC3.5 guard.

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC1: Model serializes to Vensim mdl via a stable API
- **mdl-writer.AC1.1 Success:** `Model::PrintMDL(errs)` returns Vensim `.mdl` text for a parsed, `MarkVariableTypes`-processed `Model`.
- **mdl-writer.AC1.3 Success:** The CLI emits a `.mdl` file (e.g. `xmutil --to-mdl model.mdl`, or selected by output extension).

### mdl-writer.AC3: Round-trip preserves the model (primary bar)
- **mdl-writer.AC3.5 Guard:** The `Model` comparator detects and reports a deliberately introduced non-equivalence (round-trip tests cannot pass vacuously).

(Phase 1 establishes the API shell and the comparator. The full equation/spec/sketch round-trip ACs are covered in Phases 3-7.)

---

<!-- START_SUBCOMPONENT_A (tasks 1-3) -->
<!-- START_TASK_1 -->
### Task 1: `MDLGenerator` skeleton + `Model::PrintMDL`

**Files:**
- Create: `src/Mdl/MDLGenerator.h`
- Create: `src/Mdl/MDLGenerator.cpp`
- Modify: `src/Model.h` (add `PrintMDL` declaration next to `PrintXMILE` at line 41)
- Modify: `src/Model.cpp` (add `#include "Mdl/MDLGenerator.h"` near the `#include "Xmile/XMILEGenerator.h"` at line 10; add `PrintMDL` definition next to `PrintXMILE` at lines 651-654)

**Implementation:**

`MDLGenerator.h` declares a class mirroring `XMILEGenerator` (`src/Xmile/XMILEGenerator.h:16-43`) structurally:

```cpp
#pragma once
#ifndef __MDLGENERATOR_H
#define __MDLGENERATOR_H

#include <string>
#include <vector>

class Model;

class MDLGenerator {
public:
  explicit MDLGenerator(Model *model);

  // Returns the full .mdl text (CRLF line endings). On error, appends
  // messages to errs and returns an empty string.
  std::string Print(std::vector<std::string> &errs);

private:
  Model *_model;
};

#endif  // __MDLGENERATOR_H
```

`MDLGenerator.cpp` for Phase 1 emits a minimal valid shell. Vensim `.mdl` files begin with the `{UTF-8}` marker; an otherwise-empty file re-parses to an empty model. Use a CRLF helper now because every later phase relies on CRLF output.

```cpp
#include "MDLGenerator.h"

#include "../Model.h"

MDLGenerator::MDLGenerator(Model *model) : _model(model) {
}

std::string MDLGenerator::Print(std::vector<std::string> &errs) {
  // Phase 1: emit a minimal, parseable shell. Later phases fill in macros,
  // equations, the .Control section, the sketch, and the settings tail.
  std::string out;
  out += "{UTF-8}\r\n";
  return out;
}
```

`Model::PrintMDL` (in `src/Model.cpp`, mirroring `PrintXMILE` at lines 651-654) — note: **no scale arguments**, because the parsed `VensimView` is already in Vensim coordinates (design "Additional Considerations: Coordinates"):

```cpp
std::string Model::PrintMDL(std::vector<std::string> &errs) {
  MDLGenerator generator(this);
  return generator.Print(errs);
}
```

Declaration in `src/Model.h` (after line 41, the `PrintXMILE` declaration):
```cpp
std::string PrintMDL(std::vector<std::string> &errs);
```

**Verification:** Compiles after Task 3 wires the build. No standalone test yet.

**Commit:** `feat(mdl): add MDLGenerator skeleton and Model::PrintMDL`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: C entry `convert_to_mdl` + CLI `--to-mdl`

**Files:**
- Modify: `src/XMUtil.h` (add the `extern "C"` declaration next to `convert_mdl_to_xmile` at lines 68-72)
- Modify: `src/XMUtil.cpp` (add `convert_to_mdl` definition next to `convert_mdl_to_xmile` at lines 254-327)
- Modify: `src/Main.cpp` (add a `--to-mdl` flag in the arg loop at lines 48-73; branch the conversion call at line 98 and the output extension at lines 106-129; update usage text at lines 29-38)

**Implementation:**

`convert_to_mdl` mirrors `convert_mdl_to_xmile` (`src/XMUtil.cpp:254-327`) exactly through the parse + `MarkVariableTypes` pipeline, then calls `PrintMDL` instead of `PrintXMILE`. It is `extern "C"`, returns a `strdup`'d caller-owned string, and returns `NULL` on parse failure or when `errs` is non-empty.

Declaration (`src/XMUtil.h`, after line 72):
```cpp
extern "C" {
// returns NULL on error or a string containing Vensim .mdl that the caller now owns
XMUTIL_EXPORT char *convert_to_mdl(const char *mdlSource, uint32_t mdlSourceLen, const char *fileName,
                                   int isLongName);
}
```

Definition (`src/XMUtil.cpp`) — copy the parse dispatch from `convert_mdl_to_xmile` (the `.dyn`/`.DYN` extension check at lines 259-286), then run the **full post-parse pipeline exactly as the XMILE path does at `XMUtil.cpp:296-304`**: `MarkVariableTypes(nullptr)` (296), then `AdjustGroupNames()` (297 — **important:** this mutates `ModelGroup::sName`, appending `" 1"` etc. to disambiguate names that collide with a symbol or a prior group, per `Model.cpp:443-467`), then per-macro `MarkVariableTypes(mf->NameSpace())` (299-301), then `CheckGhostOwners()` (304). The writer therefore emits the **post-`AdjustGroupNames` `sName`**. The tail becomes:
```cpp
  std::vector<std::string> errs;
  std::string mdl = m.PrintMDL(errs);
  if (errs.size() != 0) {
    return nullptr;
  }
  return strdup(mdl.c_str());
```
Do not pass `isCompact`/`isAsSectors` (they are XMILE-specific). Keep `SetAsSectors`/`isLongName` handling identical to the XMILE path so parsing behaves the same.

CLI changes in `src/Main.cpp`:
- Add `bool toMdl = false;` and a parse case `else if (strcmp("--to-mdl", arg) == 0) { toMdl = true; }` in the loop at lines 48-73.
- At line 98, branch: if `toMdl`, call `convert_to_mdl(contents.c_str(), contents.size(), path, longNames)`; else the existing `convert_mdl_to_xmile(...)`.
- For output extension (lines 121-129 non-Apple branch, and 106-120 Apple branch): when `toMdl`, replace the extension with `.mdl` instead of `.xmile`. **Guard against clobbering the input:** if the computed output path equals the input `path`, use `.regen.mdl` instead (e.g. build `p.replace_extension(".regen.mdl")`). This keeps Vensim->Vensim from overwriting its own input.
- Update the usage text (lines 29-38) to mention `--to-mdl`.

**Verification:** After Task 3 build, run:
- `out/Debug/XMUtil --to-mdl --stdio < some.mdl` prints text starting with `{UTF-8}`.
- `out/Debug/XMUtil --to-mdl test_models/"C-LEARN v77 for Vensim.mdl"` produces a `.regen.mdl` (not clobbering input) that begins with `{UTF-8}`.

**Commit:** `feat(mdl): add convert_to_mdl C entry and --to-mdl CLI flag`
<!-- END_TASK_2 -->

<!-- START_TASK_3 -->
### Task 3: Wire `src/Mdl/` into gyp + add the `xmutil_test` target

**Files:**
- Modify: `XMUtil.gyp` (add the two new source files to the `common_sources` list — the list begins at line 189; the Xmile entries are at lines 201-202 — just after the Xmile block; add a new `xmutil_test` executable target. Note the `XMUtil` target's Linux `dependencies: ['tinyxml2', 'utf']` are at lines 115-116.)

**Implementation:**

1. In the `common_sources` variable list, after:
   ```
   './src/Xmile/XMILEGenerator.h',
   './src/Xmile/XMILEGenerator.cpp',
   ```
   add:
   ```
   './src/Mdl/MDLGenerator.h',
   './src/Mdl/MDLGenerator.cpp',
   ```
   This single edit wires the files into both the `XMUtil` executable and the `XMUtil_wasm` targets (both use `<@(common_sources)`).

2. Add a new `xmutil_test` executable target alongside the `XMUtil` target. It links the same `common_sources` plus the test harness, comparator, and per-phase test files (created in Tasks 4-6 and later phases). Model it on the `XMUtil` target (`XMUtil.gyp:6-112`) but with `target_name: 'xmutil_test'` and test sources instead of `Main.cpp`:
   ```python
   {
     'target_name': 'xmutil_test',
     'type': 'executable',
     'include_dirs': [ '.', './src' ],
     'sources': [
       '<@(common_sources)',
       './test/TestHarness.cpp',
       './test/mdl/ModelComparator.cpp',
       './test/mdl/RoundTrip.cpp',
       './test/mdl/MDLGeneratorTest.cpp',
     ],
     # plus the same dependencies/libraries/conditions the XMUtil target uses
   },
   ```
   Copy the `dependencies`, `libraries`, `cflags`, and platform `conditions` blocks from the `XMUtil` target so the test binary links exactly as `XMUtil` does. **Build note (verified 2026-05-22):** a prior commit (`d8a609eb`, "build: make the gyp/ninja build work without system ICU and tinyxml2") made the Linux build use in-tree `static_library` targets `tinyxml2` and `utf` (under `third_party/tinyxml2` and `third_party/libutf`) instead of system `-licuuc`/`-ltinyxml2`, and `XMUtil` now `dependencies`-on them. So the `xmutil_test` target must likewise add `'dependencies': ['tinyxml2', 'utf']` on Linux (mirror whatever the `XMUtil` target declares — do not hardcode system libs). The test source files listed here are created in Tasks 4-6; create empty placeholder files now if needed so the first configure succeeds, or add the target after Task 6 (preferred: add the target in this task but only list `TestHarness.cpp` + `MDLGeneratorTest.cpp`, and append `ModelComparator.cpp`/`RoundTrip.cpp` in Task 5).

**Verification (operational — this is an infrastructure task, Verifies: none):**
- `./configure.sh` succeeds (read it first; use the plain native build, not `--use-wasm`/`--with-ui`).
- `ninja -C out/Debug XMUtil` builds `out/Debug/XMUtil` cleanly.
- `ninja -C out/Debug xmutil_test` builds `out/Debug/xmutil_test` cleanly (after Task 4 supplies `TestHarness.cpp` + a test main).

**Commit:** `build(mdl): wire src/Mdl into gyp and add xmutil_test target`
<!-- END_TASK_3 -->
<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 4-6) -->
<!-- START_TASK_4 -->
### Task 4: Minimal custom test harness

**Files:**
- Create: `test/TestHarness.h`
- Create: `test/TestHarness.cpp`
- Create: `test/mdl/MDLGeneratorTest.cpp` (one trivial smoke test to prove the harness + target work)

**Implementation:**

A header-only-style registration macro plus a shared `main()`. Tests self-register via a static initializer; `main()` runs all and returns non-zero if any check failed.

`test/TestHarness.h`:
```cpp
#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

struct TestCase {
  std::string name;
  std::function<void()> fn;
};

// Defined in TestHarness.cpp.
std::vector<TestCase> &AllTests();
int RegisterTest(const char *name, std::function<void()> fn);
extern int g_test_failures;       // total failed checks across all tests
extern int g_current_test_failures;  // failed checks in the running test

#define TEST(name)                                                    \
  static void name();                                                 \
  static int _reg_##name = RegisterTest(#name, name);                 \
  static void name()

#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      g_test_failures++;                                                     \
      g_current_test_failures++;                                            \
      printf("  CHECK failed: %s (%s:%d)\n", #cond, __FILE__, __LINE__);    \
    }                                                                        \
  } while (0)

#define CHECK_EQ_STR(a, b)                                                   \
  do {                                                                       \
    std::string _va = (a), _vb = (b);                                        \
    if (_va != _vb) {                                                        \
      g_test_failures++;                                                     \
      g_current_test_failures++;                                            \
      printf("  CHECK_EQ failed: \"%s\" != \"%s\" (%s:%d)\n", _va.c_str(),   \
             _vb.c_str(), __FILE__, __LINE__);                              \
    }                                                                        \
  } while (0)
```

`test/TestHarness.cpp` defines the registry and `main()`:
```cpp
#include "TestHarness.h"

int g_test_failures = 0;
int g_current_test_failures = 0;

std::vector<TestCase> &AllTests() {
  static std::vector<TestCase> tests;
  return tests;
}

int RegisterTest(const char *name, std::function<void()> fn) {
  AllTests().push_back({name, fn});
  return 0;
}

int main() {
  int failed_tests = 0;
  for (const TestCase &t : AllTests()) {
    g_current_test_failures = 0;
    printf("[ RUN  ] %s\n", t.name.c_str());
    t.fn();
    if (g_current_test_failures == 0) {
      printf("[ PASS ] %s\n", t.name.c_str());
    } else {
      printf("[ FAIL ] %s (%d checks failed)\n", t.name.c_str(), g_current_test_failures);
      failed_tests++;
    }
  }
  printf("\n%zu tests, %d failed (%d total check failures)\n", AllTests().size(), failed_tests,
         g_test_failures);
  return g_test_failures == 0 ? 0 : 1;
}
```

`test/mdl/MDLGeneratorTest.cpp` (smoke test — proves the harness, the generator, and the in-memory parse path all link and run):
```cpp
#include "../TestHarness.h"
#include "../../src/Model.h"
#include "../../src/Vensim/VensimParse.h"

// Parses .mdl text in-memory (no file needed) and returns a heap Model the
// caller owns. ProcessFile uses filename only for error messages
// (src/Vensim/VensimParse.cpp:216-221).
static Model *ParseVensim(const std::string &text) {
  Model *m = new Model();
  VensimParse vp{m};
  if (!vp.ProcessFile("<test>", text.c_str(), text.size())) {
    delete m;
    return nullptr;
  }
  m->MarkVariableTypes(nullptr);
  m->AdjustGroupNames();
  for (MacroFunction *mf : m->MacroFunctions()) m->MarkVariableTypes(mf->NameSpace());
  m->CheckGhostOwners();
  return m;
}
// (Task 5 promotes this into roundtrip::ParseVensim so all tests share one pipeline.)

TEST(MDLGenerator_emits_utf8_shell) {
  Model m;
  std::vector<std::string> errs;
  std::string out = m.PrintMDL(errs);
  CHECK(errs.empty());
  CHECK(out.rfind("{UTF-8}", 0) == 0);  // starts with {UTF-8}
  // The shell re-parses without error.
  Model *reparsed = ParseVensim(out);
  CHECK(reparsed != nullptr);
  delete reparsed;
}
```

**Testing:** This task's deliverable IS the test infrastructure. The smoke test verifies **mdl-writer.AC1.1** (PrintMDL returns text) at the shell level.

**Verification:**
- `ninja -C out/Debug xmutil_test` builds.
- `out/Debug/xmutil_test` runs, prints `[ PASS ] MDLGenerator_emits_utf8_shell`, exits 0.

**Commit:** `test(mdl): add minimal custom test harness and shell smoke test`
<!-- END_TASK_4 -->

<!-- START_TASK_5 -->
### Task 5: `ModelComparator` (framework + non-expression comparisons) and round-trip util

**Verifies:** mdl-writer.AC3.5 (the comparator can detect non-equivalence)

**Files:**
- Create: `test/mdl/ModelComparator.h`
- Create: `test/mdl/ModelComparator.cpp`
- Create: `test/mdl/RoundTrip.h`
- Create: `test/mdl/RoundTrip.cpp`
- Modify: `XMUtil.gyp` (append `ModelComparator.cpp` and `RoundTrip.cpp` to the `xmutil_test` target sources, if not already added in Task 3)

**Implementation:**

`ModelComparator` compares two `Model*` for structural/semantic equivalence and returns a `std::vector<std::string>` of human-readable differences (empty == equivalent). Phase 1 implements everything **except** deep expression-AST equality (added in Phase 3, which depends on the Phase 2 getters). The comparator must be deterministic despite hash-table iteration order in `Model::GetVariables` (`src/Model.cpp:636-649`) — index variables by name into maps before comparing.

Public interface (`ModelComparator.h`):
```cpp
#pragma once
#include <string>
#include <vector>
class Model;

class ModelComparator {
public:
  // Returns differences between a and b; empty means equivalent.
  static std::vector<std::string> Compare(Model *a, Model *b);
};
```

`ModelComparator.cpp` implements `Compare` by accumulating diffs from these helpers (all keyed by name, order-independent):

- **Variables:** Build `std::map<std::string, Variable*>` from `model->GetVariables(nullptr)` keyed by `var->GetName()`, skipping `var->Unwanted()` and `XMILE_Type_ARRAY_ELM`. Report names present in one but not the other. For each common variable compare:
  - `VariableType()` (`src/Symbol/Variable.h:297`).
  - units: `var->Units()` text (`UnitExpression::GetEquationString()`) falling back to `var->GetUnitsString()` (mirror `XMILEGenerator.cpp:558-567`).
  - comment: `var->Comment()` (`src/Symbol/Variable.h:236`).
  - subscript count: `var->SubscriptCount(...)`/`SubscriptCountVars(...)`.
  - equation expression: **Phase 1 placeholder** — compare nothing yet (a `// TODO(phase3): structural expression compare` comment). Phase 3 fills this in.
- **Dimensions:** For `XMILE_Type_ARRAY` variables, compare the element name lists (iterate the `ExpressionSymbolList::SymList()` of `var->GetEquation(0)->GetExpression()`, see `XMILEGenerator.cpp:254-263`).
- **Sim specs:** Compare `GetConstanValue("INITIAL TIME", initial_time())`, `"FINAL TIME"`, `"TIME STEP"`, `"SAVEPER"` (see `src/Model.cpp:604-616`) and `IntegrationType()`.
- **Groups:** Compare `Model::Groups()` by `ModelGroup::sName` and the set of member variable names (`group->vVariables`), order-independent.
- **Views:** Compare `Model::Views()`. For each `VensimView`, walk `Elements()` (`src/Vensim/VensimView.h:141`); compare element count, and per non-NULL element: `Type()`, `X()/Y()/Width()/Height()`, and for connectors `From()/To()/Polarity()`, and for variable elements the referenced `GetVariable()->GetName()` plus ghost/attached state. (Phase 5 strengthens this; Phase 1 implements the comparison so it is ready.)

`RoundTrip.h`/`.cpp` provide the harness helper later phases use:
```cpp
// RoundTrip.h
#pragma once
#include <string>
#include <vector>
class Model;
namespace roundtrip {
// Parse Vensim .mdl text in-memory and run the full post-parse pipeline
// (MarkVariableTypes main + macros, AdjustGroupNames, CheckGhostOwners) -- the
// same sequence convert_to_mdl uses (XMUtil.cpp:296-304).
Model *ParseVensim(const std::string &text);
// Parse Dynamo .dyn text in-memory and run the same post-parse pipeline.
Model *ParseDynamo(const std::string &text);
// Parse -> PrintMDL -> re-parse; returns the differences from ModelComparator.
// On any parse or generation failure, returns a single-element vector describing it.
std::vector<std::string> RoundTripDiffs(const std::string &mdlText);
}  // namespace roundtrip
```
`RoundTripDiffs` parses `mdlText` into `m0`, calls `m0->PrintMDL(errs)`, parses the result into `m1`, and returns `ModelComparator::Compare(m0, m1)` (or an error string on failure). This is the engine of every later round-trip test. **Both `ParseVensim` (and `ParseDynamo`) must run the identical post-parse pipeline `convert_to_mdl` uses, mirroring `XMUtil.cpp:296-304`: `MarkVariableTypes(nullptr)`, then `AdjustGroupNames()`, then per-macro `MarkVariableTypes(mf->NameSpace())`, then `CheckGhostOwners()`.** Applying `AdjustGroupNames()` to BOTH `m0` and `m1` is essential — it mutates `ModelGroup::sName` on collision, so omitting it on one side would make group comparisons (AC3.3) diverge. The smoke `ParseVensim` helper in Task 4 must be updated to the same full pipeline (or share this one).

**Testing (describe — task-implementor writes the actual test code in `MDLGeneratorTest.cpp` or a new `ModelComparatorTest.cpp`):**
- mdl-writer.AC3.5: parse a small fixture twice into two `Model`s; `Compare` returns empty (equivalent). Then build a second `Model` from a deliberately different source (e.g. rename a variable, change a constant, or change a variable's type) and confirm `Compare` returns a non-empty diff naming the change. Use a tiny inline `.mdl` string for the fixture (a stock + a flow + an aux + the `.Control` group). This proves the comparator is not vacuous.

**Verification:**
- `ninja -C out/Debug xmutil_test` builds.
- `out/Debug/xmutil_test` runs; the comparator tests pass.

**Commit:** `test(mdl): add ModelComparator and round-trip harness`
<!-- END_TASK_5 -->

<!-- START_TASK_6 -->
### Task 6: Phase-1 shell round-trip + AC3.5 guard wiring

**Verifies:** mdl-writer.AC1.1, mdl-writer.AC1.3, mdl-writer.AC3.5

**Files:**
- Modify: `test/mdl/MDLGeneratorTest.cpp` (add the guard + CLI-shell tests)

**Implementation / Testing (describe):**
- **AC1.1 / shell round-trip:** Parse a tiny `.mdl` into `m0`, call `PrintMDL`, re-parse into `m1`. For Phase 1 the writer only emits the `{UTF-8}` shell, so `m1` is empty; assert that the call sequence succeeds and `m1` is non-null (a full content round-trip is asserted starting in Phase 3). The point here is that the pipeline is wired end-to-end.
- **AC3.5 guard:** Already covered by Task 5's comparator test; add an explicit named test `ModelComparator_detects_mutation` that constructs two models differing only in one stock's initial constant and asserts a non-empty diff.
- **AC1.3 (CLI):** A lightweight check that the CLI path is wired: this is fully exercised by the end-to-end CLI test in Phase 7. In Phase 1, just confirm manually (see Task 2 verification) that `--to-mdl --stdio` emits a `{UTF-8}` shell. No automated CLI test in Phase 1.

**Verification:**
- `out/Debug/xmutil_test` passes all tests, exits 0.
- Manual: `out/Debug/XMUtil --to-mdl --stdio < test_models/"C-LEARN v77 for Vensim.mdl"` prints a `{UTF-8}`-prefixed shell.

**Commit:** `test(mdl): phase-1 shell round-trip and comparator guard`
<!-- END_TASK_6 -->
<!-- END_SUBCOMPONENT_B -->

## Phase 1 Done When
- The project builds (`XMUtil` and `xmutil_test`).
- `xmutil --to-mdl` produces a parseable shell `.mdl` (AC1.1, AC1.3 at shell level).
- `ModelComparator` reports equivalence for a model parsed twice and non-equivalence for two different models (AC3.5).
