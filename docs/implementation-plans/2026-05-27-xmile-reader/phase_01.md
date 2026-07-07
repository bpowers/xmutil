# XMILE Reader Implementation Plan — Phase 1

**Goal:** Stand up a buildable `XmileReader` skeleton wired end-to-end, with the `test/xmile/` harness in place, so subsequent phases have a working envelope to fill in.

**Architecture:** A new `XmileReader` class (parallel to `VensimParse`) walks the XMILE DOM via tinyxml2. A new `Model::ParseXMILE` convenience wrapper constructs it. Extern-C entries (`convert_xmile_to_xmile`, `convert_xmile_to_mdl`) mirror the existing `convert_mdl_to_xmile` / `convert_to_mdl` shape. CLI dispatch in `Main.cpp` selects the XMILE driver for `.xmile` / `.stmx` inputs. A new `test/xmile/` directory mirrors `test/mdl/`.

**Tech Stack:** C++17, vendored tinyxml2 (`third_party/tinyxml2/`), gyp + ninja, existing TEST/CHECK harness (`test/TestHarness.h`).

**Scope:** Phase 1 of 8 from `docs/design-plans/2026-05-27-xmile-reader.md`. Subsequent phases (2 — equation grammar, 3-7 — model population, 8 — corpus + WASM) fill in the envelope this phase establishes.

**Codebase verified:** 2026-05-28

---

## Acceptance Criteria Coverage

This phase implements and tests:

### xmile-reader.AC1: Model populates from XMILE via a stable API
- **xmile-reader.AC1.1 Success:** `Model::ParseXMILE(filename, contents, len, errs)` populates a `Model` that subsequently runs through `MarkVariableTypes` / `AdjustGroupNames` / `CheckGhostOwners` without error for a well-formed corpus XMILE input.
- **xmile-reader.AC1.4 Success:** The CLI dispatches `.xmile` and `.stmx` inputs (case-insensitive) to the XMILE reader; without `--to-mdl` it emits XMILE; with `--to-mdl` it emits MDL.
- **xmile-reader.AC1.6 Edge:** XMILE -> XMILE on a file named `model.xmile` writes `model.regen.xmile`; the input is not overwritten.

Phase 1 covers these ACs at the envelope level — an empty `<xmile>` document is a "well-formed corpus XMILE input" for AC1.1 purposes (no variables, but the pipeline still runs without error). Variable population is added in Phases 3-7; the comparator-driven structural checks behind AC1.1 are deferred to those phases. AC1.2, AC1.3, AC1.5 (extern-C return contracts under failure) are wired in this phase but their negative-path tests live in Phase 8.

---

## Codebase verification findings

- ✓ `src/Vensim/VensimParse.h:21-100` defines `VensimParse` with constructor `VensimParse(Model *)` and entry `bool ProcessFile(const std::string &filename, const char *contents, size_t contentsLen)`. The reader template the design references is real.
- ✗ Design's pipeline citation `src/XMUtil.cpp:278-304` is off; the actual `MarkVariableTypes` -> `AdjustGroupNames` -> per-macro `MarkVariableTypes` -> `CheckGhostOwners` block is at lines **296-304** in `convert_mdl_to_xmile` and **359-366** in `convert_to_mdl`. `test/mdl/RoundTrip.cpp:16-22` already factors this into a private `PreparePipeline(Model*)` helper — we will follow that pattern.
- ✗ Design says `ProcessFile(filename, contents, len, errs)` but the real `VensimParse::ProcessFile` has no `errs` parameter. **The XMILE reader will introduce `errs` as a new convention** (it's the design's intent and matches AC1.5). Errors today go through `VensimParseSyntaxError` and `log()`; the new reader will additionally push descriptive strings into the caller's `std::vector<std::string>&`.
- ✗ Existing MDL-out extern-C is named `convert_to_mdl`, not `convert_mdl_to_mdl` (`src/XMUtil.h:77`, `src/XMUtil.cpp:331`). New entries: `convert_xmile_to_xmile` and `convert_xmile_to_mdl`.
- ✗ Existing `convert_mdl_to_xmile` takes an `isCompact` flag (`src/XMUtil.h:71`); `convert_to_mdl` does not. To match the AC1.2 signature in the design (`source, len, fileName, longNames, sectors`), the new `convert_xmile_to_xmile` does **not** take `isCompact` — we will hardcode `isCompact = false` internally (same as the CLI does at `src/Main.cpp:107`).
- ✗ CLI extension dispatch lives **inside** `convert_mdl_to_xmile` / `convert_to_mdl` (`XMUtil.cpp:262-263, 268`), not in `Main.cpp`. It uses a raw 3-char suffix compare (`ext = fileName + strlen(fileName) - 3`) and is case-sensitive (`ext == "dyn" || ext == "DYN"`). The new XMILE path needs **case-insensitive** extension matching in `Main.cpp` so it can choose between MDL and XMILE input drivers before calling the extern-C entry; the extern-C entries themselves no longer need to sniff.
- ✗ The `.regen.mdl` rename is platform-split between Apple (`src/Main.cpp:115-138`, uses raw string manipulation) and non-Apple (`src/Main.cpp:139-155`, uses `std::filesystem::path`). Both branches need an analog `.regen.xmile` case.
- ✓ `XMUtil.gyp:232-313` declares a single flat `common_sources` variable consumed by `XMUtil`, `xmutil_test`, and `XMUtil_wasm`. Adding files there picks them up everywhere automatically.
- ✓ `test/TestHarness.h` exposes `TEST(name)`, `CHECK(cond)`, `CHECK_EQ_STR(a,b)`; self-registering at static-init time.
- ✓ `test/mdl/ModelComparator.h:6-10` exposes a single static `Compare(Model*, Model*) -> std::vector<std::string>`. Format-agnostic; reusable from XMILE tests.
- ✓ `test/mdl/RoundTrip.{h,cpp}` is the Vensim-side wrapper. We will mirror its shape into `test/xmile/RoundTrip.{h,cpp}`.
- ✓ `Model` has no existing `ParseMDL` or similar convenience wrapper (`src/Model.h:19-143`). `ParseXMILE` is a brand-new addition — note in the implementation that it's the **first** parser convenience on `Model`, not parallel to anything.
- ✓ `Model::Views()` is a `std::vector<View*>`; `View` is an abstract base (`src/Model.h:9-18`). `VensimView` is the concrete subclass added via `Model::AddView(View*)` (`src/Model.h:51-56`). Phase 7's `XmileView` will populate `VensimView`.
- ✓ `format.sh` uses `clang-format -i src/**/*.cpp src/**/*.h` — zsh `**` covers new `src/Xmile/` files automatically.
- ✗ `format.sh` does **not** cover `test/**` — test files must be formatted manually or the script extended.
- ✓ Corpus XMILE fixtures live at `third_party/simlin/default_projects/{fishbanks,logistic-growth,reliability}/model.xmile` — Phase 1 references them as readable paths but doesn't yet structurally compare them.

---

<!-- START_SUBCOMPONENT_A (tasks 1-2) -->

<!-- START_TASK_1 -->
### Task 1: Skeleton — XmileReader class + Model::ParseXMILE wrapper

**Verifies:** None (infrastructure).

**Files:**
- Create: `src/Xmile/XmileReader.h`
- Create: `src/Xmile/XmileReader.cpp`
- Modify: `src/Model.h` (add `ParseXMILE` declaration)
- Modify: `src/Model.cpp` (add `ParseXMILE` definition; include `Xmile/XmileReader.h`)
- Modify: `XMUtil.gyp` (`common_sources`: add the two new entries)

**Implementation notes:**

1. `XmileReader` follows the `VensimParse` template (`src/Vensim/VensimParse.h:21-100`):
   - Constructor `XmileReader(Model *model)`: captures `model`, `model->GetNameSpace()`, asserts the process-global singleton is null, sets it. No `ReadyFunctions()` call here — Phase 2 introduces a separate process-global for the equation parser; Phase 1 doesn't need any function lookup yet.
   - Destructor: clears the process-global.
   - Entry: `bool ProcessFile(const std::string &filename, const char *contents, size_t len, std::vector<std::string> &errs);`. The `errs` parameter is new compared to `VensimParse` — the design treats this as the XMILE reader's stable error channel (AC1.5).
   - Stub body for Phase 1: parse `contents` as XML via `tinyxml2::XMLDocument::Parse`. If parse fails or the root element is not `<xmile>` (or `xmile` in some namespace — tinyxml2 returns the local name verbatim including any prefix; for Phase 1 accept exact local name `xmile`), push a descriptive error into `errs` and return `false`. Otherwise return `true` without populating the model. Future phases replace the body; the signature stays.
   - Provide an `extern XmileReader *XPObject;` declaration in the header and the corresponding definition + assertion in the cpp. This is the process-global that Phase 2's equation parser uses; declaring it now keeps the header stable.

2. `Model::ParseXMILE` is a thin convenience wrapper (a new shape — no `ParseMDL` analog exists):
   ```cpp
   // in Model.h, public section, near MarkVariableTypes:
   bool ParseXMILE(const std::string &filename, const char *contents, size_t len,
                   std::vector<std::string> &errs);
   ```
   ```cpp
   // in Model.cpp:
   bool Model::ParseXMILE(const std::string &filename, const char *contents, size_t len,
                          std::vector<std::string> &errs) {
     XmileReader reader{this};
     return reader.ProcessFile(filename, contents, len, errs);
   }
   ```
   The pipeline (`MarkVariableTypes`, etc.) is **not** run inside `ParseXMILE` — that mirrors how `VensimParse::ProcessFile` works: the parser populates the model, the caller runs the pipeline. The extern-C entries in Task 2 are the canonical pipeline drivers.

3. `XMUtil.gyp` change:
   - In the `'variables': { 'common_sources': [...] }` block (lines 232-313), under the existing `'./src/Xmile/XMILEGenerator.cpp'` entries (lines 245-246), add:
     ```
     './src/Xmile/XmileReader.h',
     './src/Xmile/XmileReader.cpp',
     ```

**Header sketch (`src/Xmile/XmileReader.h`):**
```cpp
#ifndef _XMUTIL_XMILE_XMILEREADER_H
#define _XMUTIL_XMILE_XMILEREADER_H
#include <string>
#include <vector>
class Model;
class SymbolNameSpace;

class XmileReader {
public:
  explicit XmileReader(Model *model);
  ~XmileReader();
  bool ProcessFile(const std::string &filename, const char *contents, size_t len,
                   std::vector<std::string> &errs);

  SymbolNameSpace *GetSymbolNameSpace() { return pSymbolNameSpace; }
  Model *GetModel() { return _model; }

private:
  Model *_model;
  SymbolNameSpace *pSymbolNameSpace;
};

extern XmileReader *XPObject;
#endif
```

**Implementation sketch (`src/Xmile/XmileReader.cpp`):**
```cpp
#include "XmileReader.h"
#include <cassert>
#include "../../third_party/tinyxml2/tinyxml2.h"
#include "../Model.h"

XmileReader *XPObject = nullptr;

XmileReader::XmileReader(Model *model) : _model(model), pSymbolNameSpace(model->GetNameSpace()) {
  assert(!XPObject);
  XPObject = this;
}

XmileReader::~XmileReader() { XPObject = nullptr; }

bool XmileReader::ProcessFile(const std::string &filename, const char *contents, size_t len,
                              std::vector<std::string> &errs) {
  tinyxml2::XMLDocument doc;
  // tinyxml2::XMLDocument::Parse honors a length argument, so we can hand it a
  // non-NUL-terminated buffer (the extern-C path passes uint32_t lengths).
  tinyxml2::XMLError parseErr = doc.Parse(contents, len);
  if (parseErr != tinyxml2::XML_SUCCESS) {
    errs.push_back(filename + ": XML parse error: " + doc.ErrorStr());
    return false;
  }
  tinyxml2::XMLElement *root = doc.RootElement();
  if (!root) {
    errs.push_back(filename + ": empty XML document");
    return false;
  }
  // The XMILE namespace prefix may decorate the local name (e.g. "isee:xmile" is
  // *not* the root we want — that would be vendor-namespaced). We accept the
  // bare "xmile" local name for Phase 1; Phase 3+ refines namespace handling.
  const char *name = root->Name();
  if (!name || std::string(name) != "xmile") {
    errs.push_back(filename + ": root element is not <xmile> (got <" +
                   (name ? name : "(null)") + ">)");
    return false;
  }
  // Phase 1 stops here. Phases 3-7 walk <header>, <sim_specs>, <model>, <views>.
  return true;
}
```

**Verification:**
- `./configure.sh` succeeds (regenerates ninja from the modified gyp).
- `ninja -C out/Debug XMUtil` succeeds.
- `ninja -C out/Debug xmutil_test` succeeds.
- `out/Debug/xmutil_test` runs (no new tests yet — exit 0 with the existing MDL test count unchanged).

**Commit:** `feat(xmile): skeleton XmileReader + Model::ParseXMILE`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Extern-C entries — convert_xmile_to_xmile and convert_xmile_to_mdl

**Verifies:** None directly (the entries' positive-path output is exercised by Task 4's smoke test; their negative paths land in Phase 8).

**Files:**
- Modify: `src/XMUtil.h` (declare two new extern-C entries)
- Modify: `src/XMUtil.cpp` (implement them; include `Xmile/XmileReader.h` is unnecessary because they go through `Model::ParseXMILE`)

**Implementation notes:**

Add the declarations next to the existing extern-C entries (`src/XMUtil.h:69-78`):

```cpp
extern "C" {
// returns NULL on error or a string containing XMILE that the caller now owns
XMUTIL_EXPORT char *convert_xmile_to_xmile(const char *source, uint32_t len, const char *fileName,
                                           int isLongName, bool isAsSectors);
}

extern "C" {
// returns NULL on error or a string containing Vensim .mdl that the caller now owns
XMUTIL_EXPORT char *convert_xmile_to_mdl(const char *source, uint32_t len, const char *fileName, int isLongName);
}
```

Signature notes:
- `isLongName` is `int` (not `bool`) for consistency with the existing pair (`-1`/`0`/`1` tri-state — see `src/Main.cpp:47`).
- `isAsSectors` matches `convert_mdl_to_xmile`.
- No `isCompact` here — internal callers pass `false` (compact mode is unused outside the old CLI path).

In `src/XMUtil.cpp`, after the existing `convert_to_mdl` (line 375), add the two implementations. Both follow the same shape as the existing entries and call `Model::ParseXMILE`. The pipeline block is identical to `convert_to_mdl`'s — copy it verbatim. Example:

```cpp
extern "C" {
char *convert_xmile_to_xmile(const char *source, uint32_t len, const char *fileName,
                             int isLongName, bool isAsSectors) {
  Model m{};
  if (fileName == nullptr) {
    fileName = "<in memory>";
  }
  std::vector<std::string> errs;
  m.SetAsSectors(isAsSectors);
  if (!m.ParseXMILE(fileName, source, len, errs)) {
    for (const std::string &e : errs) {
      log("%s\n", e.c_str());
    }
    return nullptr;
  }
  // Same post-parse pipeline as convert_mdl_to_xmile / convert_to_mdl.
  m.MarkVariableTypes(nullptr);
  m.AdjustGroupNames();
  for (MacroFunction *mf : m.MacroFunctions()) {
    m.MarkVariableTypes(mf->NameSpace());
  }
  m.CheckGhostOwners();

  // longNames maps to PrintXMILE's identifier strategy; isLongName == 1 means
  // "use long names" (mirrors convert_mdl_to_xmile, which threads it through
  // VensimParse::SetLongName). PrintXMILE itself does not currently consume
  // isLongName as a parameter — it reads it from the Model's symbol table state
  // populated during parsing. For XMILE input we follow the same path.
  (void)isLongName;  // v1: accepted for API symmetry with convert_mdl_to_xmile but not consumed.

  double xscale = 1.0;
  double yscale = 1.0;
  std::string xmile = m.PrintXMILE(/*isCompact=*/false, errs, xscale, yscale);
  if (!errs.empty()) {
    for (const std::string &e : errs) log("%s\n", e.c_str());
    return nullptr;
  }
  return strdup(xmile.c_str());
}
}  // extern "C"

extern "C" {
char *convert_xmile_to_mdl(const char *source, uint32_t len, const char *fileName, int isLongName) {
  Model m{};
  if (fileName == nullptr) {
    fileName = "<in memory>";
  }
  std::vector<std::string> errs;
  m.SetAsSectors(false);
  if (!m.ParseXMILE(fileName, source, len, errs)) {
    for (const std::string &e : errs) log("%s\n", e.c_str());
    return nullptr;
  }
  m.MarkVariableTypes(nullptr);
  m.AdjustGroupNames();
  for (MacroFunction *mf : m.MacroFunctions()) {
    m.MarkVariableTypes(mf->NameSpace());
  }
  m.CheckGhostOwners();
  (void)isLongName;  // see note above

  std::string mdl = m.PrintMDL(errs);
  if (!errs.empty()) {
    for (const std::string &e : errs) log("%s\n", e.c_str());
    return nullptr;
  }
  return strdup(mdl.c_str());
}
}  // extern "C"
```

The `(void)isLongName` lines mark v1's deliberate non-consumption. simlin's XMILE writer follows the same pattern — XMILE has no long-vs-short-name distinction at the envelope level, and `PrintXMILE` reads the relevant state from the Model's symbol table. The flag is in the signature for API symmetry with `convert_mdl_to_xmile` so embedders can use the same call shape across both directions. If a future need arises (e.g., XMILE input with quoted-name variables that should be normalized), wiring is a small follow-up task; the v1 corpus does not exercise it.

**Verification:**
- `ninja -C out/Debug XMUtil` succeeds.
- `ninja -C out/Debug xmutil_test` succeeds.
- Manually: `out/Debug/XMUtil --stdio < /dev/null 2>&1 | head -1` exits non-zero (no input) — confirms no regression to the existing path.

**Commit:** `feat(xmile): extern-C entries convert_xmile_to_xmile and convert_xmile_to_mdl`
<!-- END_TASK_2 -->

<!-- END_SUBCOMPONENT_A -->

<!-- START_SUBCOMPONENT_B (tasks 3-3) -->

<!-- START_TASK_3 -->
### Task 3: CLI dispatch — extension-based driver selection and .regen.xmile rename

**Verifies:** xmile-reader.AC1.4, xmile-reader.AC1.6.

**Files:**
- Modify: `src/Main.cpp` (extension dispatch, output-filename rename)

**Implementation notes:**

The existing CLI (`src/Main.cpp:104-108`) dispatches on the `--to-mdl` flag only:
```cpp
if (toMdl) {
  output = convert_to_mdl(...);
} else {
  output = convert_mdl_to_xmile(..., false /*isCompact*/, ...);
}
```

Phase 1 adds input-extension dispatch on top. The selection matrix is:

| input ext | `--to-mdl` | entry called |
|-----------|------------|--------------|
| `.mdl`, `.dyn`, none | false | `convert_mdl_to_xmile` |
| `.mdl`, `.dyn`, none | true  | `convert_to_mdl` |
| `.xmile`, `.stmx`    | false | `convert_xmile_to_xmile` |
| `.xmile`, `.stmx`    | true  | `convert_xmile_to_mdl` |

Extension matching is **case-insensitive** (AC1.4 explicit). Add a small helper above `cliMain`:

```cpp
static bool extensionMatchesInsensitive(const char *path, const char *ext) {
  if (!path) return false;
  const size_t plen = strlen(path);
  const size_t elen = strlen(ext);
  if (plen < elen) return false;
  const char *suffix = path + plen - elen;
  for (size_t i = 0; i < elen; ++i) {
    char a = suffix[i], b = ext[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
    if (a != b) return false;
  }
  return true;
}
```

Below the existing argument parsing in `cliMain`, replace the dispatch block (`src/Main.cpp:103-108`) with:

```cpp
char *output;
const bool xmileInput =
    extensionMatchesInsensitive(path, ".xmile") || extensionMatchesInsensitive(path, ".stmx");
if (xmileInput) {
  if (toMdl) {
    output = convert_xmile_to_mdl(contents.c_str(), contents.size(), path, longNames);
  } else {
    output = convert_xmile_to_xmile(contents.c_str(), contents.size(), path, longNames, sectors);
  }
} else {
  if (toMdl) {
    output = convert_to_mdl(contents.c_str(), contents.size(), path, longNames);
  } else {
    output = convert_mdl_to_xmile(contents.c_str(), contents.size(), path, false, longNames, sectors);
  }
}
```

Note: stdio mode sets `path = "STDIN"` (`src/Main.cpp:81`), which won't match either XMILE extension; stdio defaults to MDL input. That preserves current behavior and is consistent with the design (extension-driven input selection is for file inputs).

For the **`.regen.xmile` clobber guard** (AC1.6), both the Apple branch (`src/Main.cpp:115-138`) and the non-Apple branch (`src/Main.cpp:139-155`) need a new case mirroring the existing `.regen.mdl` logic. The rule is: when the chosen output extension equals the input extension, append `.regen` before the extension.

For the non-Apple branch, replace lines 140-149 with:

```cpp
std::filesystem::path p(path);
const bool inputIsXmile = extensionMatchesInsensitive(path, ".xmile") ||
                          extensionMatchesInsensitive(path, ".stmx");
if (toMdl) {
  p.replace_extension(".mdl");
  if (p == std::filesystem::path(path)) {
    p.replace_extension(".regen.mdl");
  }
} else {
  p.replace_extension(".xmile");
  if (inputIsXmile && p == std::filesystem::path(path)) {
    p.replace_extension(".regen.xmile");
  }
}
```

The `inputIsXmile` guard makes the `.regen.xmile` rename apply only when the input was XMILE — an MDL input being converted to XMILE produces a `.xmile` file alongside the `.mdl`, no collision. For a `.stmx` input being converted to XMILE, the output filename becomes `<base>.xmile` (different extension) — no collision either, so no rename needed.

For the Apple branch (`src/Main.cpp:117-133`), apply the same logic with string operations:

```cpp
std::string p(path);
for (std::string::iterator it = p.end(); it-- > p.begin();) {
  if (*it == '.') {
    p = p.substr(0, it - p.begin());
    break;
  } else if (*it == '/' || *it == '\\' || *it == ':')
    break;
}
const bool inputIsXmile = extensionMatchesInsensitive(path, ".xmile") ||
                          extensionMatchesInsensitive(path, ".stmx");
if (toMdl) {
  p += ".mdl";
  if (p == path) {
    p = p.substr(0, p.size() - 4) + ".regen.mdl";
  }
} else {
  p += ".xmile";
  if (inputIsXmile && p == path) {
    p = p.substr(0, p.size() - 6) + ".regen.xmile";
  }
}
```

Also update `cliUsage` (`src/Main.cpp:30-40`) so the help text reflects the broader input support:

```cpp
log("Usage: %s [OPTION...] PATH\n"
    "Convert Vensim .mdl, Dynamo .dyn, or XMILE .xmile/.stmx files to XMILE or .mdl.\n\n"
    "Options:\n"
    "  --help:\tshow this message\n"
    "  --stdio:\tread from stdin, write to stdout (assumes Vensim input)\n"
    "  --to-mdl:\twrite Vensim MDL instead of XMILE\n",
    argv0);
```

**Verification:**
- `ninja -C out/Debug XMUtil` succeeds.
- `cp third_party/simlin/default_projects/logistic-growth/model.xmile /tmp/model.xmile && out/Debug/XMUtil /tmp/model.xmile` — exits successfully; `/tmp/model.regen.xmile` exists (even if structurally empty XMILE for Phase 1).
- `cp third_party/simlin/default_projects/logistic-growth/model.xmile /tmp/foo.STMX && out/Debug/XMUtil /tmp/foo.STMX` — exits successfully (verifies case-insensitive .stmx); produces `/tmp/foo.xmile` (no collision, no `.regen`).
- `cp third_party/simlin/default_projects/logistic-growth/model.xmile /tmp/m.xmile && out/Debug/XMUtil --to-mdl /tmp/m.xmile` — produces `/tmp/m.mdl`.
- Existing CLI path: `test/cli_roundtrip.sh` still passes (no regression on MDL dispatch).

**Commit:** `feat(xmile): CLI dispatch for .xmile/.stmx inputs + .regen.xmile guard`
<!-- END_TASK_3 -->

<!-- END_SUBCOMPONENT_B -->

<!-- START_SUBCOMPONENT_C (tasks 4-4) -->

<!-- START_TASK_4 -->
### Task 4: test/xmile harness — RoundTrip helpers and BasicSmokeTest

**Verifies:** xmile-reader.AC1.1 (envelope-level), xmile-reader.AC1.4 (in-process), xmile-reader.AC1.6 (in-process via filename-rename helper if added; primary check is the manual CLI step above).

**Files:**
- Create: `test/xmile/RoundTrip.h`
- Create: `test/xmile/RoundTrip.cpp`
- Create: `test/xmile/BasicSmokeTest.cpp`
- Modify: `XMUtil.gyp` (`xmutil_test` `sources` list: add three new entries)

**Implementation notes:**

`test/xmile/RoundTrip.{h,cpp}` is the XMILE-flavored analog of `test/mdl/RoundTrip.{h,cpp}`. It exposes:

```cpp
// test/xmile/RoundTrip.h
#pragma once
#include <string>
#include <vector>
class Model;

namespace xmileroundtrip {
// Parse XMILE text in-memory and run the same post-parse pipeline the extern-C
// drivers use (MarkVariableTypes main + macros, AdjustGroupNames,
// CheckGhostOwners). Returns a heap Model the caller owns, or nullptr on
// parse failure; failure messages are pushed into errs.
Model *ParseXMILE(const std::string &text, std::vector<std::string> &errs);

// Parse -> PrintXMILE -> re-parse; returns ModelComparator differences.
// On any failure, returns a single-element vector describing it.
std::vector<std::string> RoundTripDiffs(const std::string &xmileText);

// Parse XMILE -> PrintMDL -> Vensim re-parse; returns ModelComparator
// differences.
std::vector<std::string> XmileToMdlDiffs(const std::string &xmileText);
}  // namespace xmileroundtrip
```

The implementation (`test/xmile/RoundTrip.cpp`) mirrors `test/mdl/RoundTrip.cpp:8-97`. The local `PreparePipeline` helper is copied verbatim. `XmileToMdlDiffs` re-parses the emitted MDL with `VensimParse` (just like the MDL-side `DynamoToMdlDiffs` does).

**Note for Phase 1:** the round-trip helpers compile and link but the structural comparator runs over an *empty* model on both sides until Phase 3 populates anything. That is fine — the Phase 1 smoke test only checks the envelope contract.

`test/xmile/BasicSmokeTest.cpp` exercises three things:

```cpp
#include <string>
#include <vector>
#include "../../src/Model.h"
#include "../TestHarness.h"
#include "RoundTrip.h"

namespace {
const char *kEmptyEnvelope =
    "<xmile version=\"1.0\" xmlns=\"http://docs.oasis-open.org/xmile/ns/XMILE/v1.0\">\n"
    "  <header><name>empty</name></header>\n"
    "  <sim_specs/>\n"
    "  <model/>\n"
    "</xmile>\n";

const char *kMissingRoot = "not even xml";

const char *kWrongRoot =
    "<not-xmile/>\n";
}  // namespace

TEST(xmile_envelope_parses) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kEmptyEnvelope, errs);
  CHECK(m != nullptr);
  CHECK(errs.empty());
  delete m;
}

TEST(xmile_non_xml_input_errors) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kMissingRoot, errs);
  CHECK(m == nullptr);
  CHECK(!errs.empty());
}

TEST(xmile_wrong_root_errors) {
  std::vector<std::string> errs;
  Model *m = xmileroundtrip::ParseXMILE(kWrongRoot, errs);
  CHECK(m == nullptr);
  CHECK(!errs.empty());
}

TEST(xmile_empty_envelope_round_trip) {
  // Parse -> PrintXMILE -> re-parse. With an empty model both sides have no
  // variables, so the comparator returns an empty diff list.
  std::vector<std::string> diffs = xmileroundtrip::RoundTripDiffs(kEmptyEnvelope);
  CHECK(diffs.empty());
}
```

`xmutil_test` `sources` addition (`XMUtil.gyp:128-145`): after the existing `'./test/mdl/DynamoToMdlTest.cpp',` (line 144) add:
```
'./test/xmile/RoundTrip.h',
'./test/xmile/RoundTrip.cpp',
'./test/xmile/BasicSmokeTest.cpp',
```

(gyp lists headers explicitly for source dependency tracking on some generators; we follow the existing `test/mdl/RoundTrip.h`-not-listed convention there. Concretely: just the `.cpp` files need to be in the `sources` list. The header is included from the cpp and gets picked up via the include path. So the actual addition is just `RoundTrip.cpp` and `BasicSmokeTest.cpp`.)

**Verification:**
- `./configure.sh` succeeds.
- `ninja -C out/Debug xmutil_test` succeeds.
- `out/Debug/xmutil_test` runs; all four new XMILE tests pass; existing MDL test count + 4 = new total, zero failures.
- `out/Debug/xmutil_test 2>&1 | grep -E "xmile_(envelope_parses|non_xml_input_errors|wrong_root_errors|empty_envelope_round_trip)"` shows each test present.

**Commit:** `test(xmile): basic envelope smoke + RoundTrip harness`
<!-- END_TASK_4 -->

<!-- END_SUBCOMPONENT_C -->

---

## Phase 1 done when

- The project builds (`ninja -C out/Debug XMUtil` and `ninja -C out/Debug xmutil_test`).
- `xmutil model.xmile` and `xmutil --to-mdl model.xmile` produce output files (structurally empty XMILE / `.mdl` is acceptable for now — Phase 3+ fill in content).
- The four new tests in `test/xmile/BasicSmokeTest.cpp` pass.
- The existing MDL test suite continues to pass with no regressions.
- `bash test/cli_roundtrip.sh` continues to pass (MDL dispatch unchanged).
- `./format.sh` clean on the new `src/Xmile/*.{h,cpp}` files.

## Out of scope for Phase 1

- Equation parsing (Phase 2).
- Any `<aux>`, `<stock>`, `<flow>`, `<sim_specs>`, `<dimensions>`, `<gf>`, `<view>` handling (Phases 3-7).
- Test fixtures structurally comparing against the corpus (Phases 4 + 8).
- WASM build verification (Phase 8).
- Wiring the `isLongName` flag through to `XmileReader` — v1 deliberately accepts and ignores it for API symmetry with `convert_mdl_to_xmile`. The corpus tests don't exercise this distinction, and simlin's reference XMILE writer follows the same non-consumption pattern.
