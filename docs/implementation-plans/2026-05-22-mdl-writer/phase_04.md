# Vensim mdl Writer Implementation Plan — Phase 4: Control and settings sections

**Goal:** Emit sim specs (the `.Control` group with `INITIAL TIME`/`FINAL TIME`/`TIME STEP`/`SAVEPER`), group banners interleaved with their variables, and the trailing `:L...` settings section (`15:` integration method, `22:` unit equivalences, plus Vensim default cosmetic lines).

**Architecture:** `MDLGenerator` gains group-aware equation emission, a `.Control` group emitted last, and a settings emitter. Because xmutil's parser only reads the settings section after the sketch terminator `///---\\\`, this phase also introduces the sketch framing (`\\\---///` ... `///---\\\`) as a **minimal/empty frame** so settings re-parse; Phase 5 fills the frame with real view records. Output is assembled with `\n` and converted to CRLF once at the end of `Print` (matching simlin `writer.rs:2760`).

**Tech Stack:** C++17, gyp. Reference: simlin `writer.rs` — `write_sim_specs` (2821-2871), the `.Control` header + group banners (2919-2994), `write_settings_section` (3217-3282); integration map in `mdl/settings.rs:130-134`.

**Scope:** Phase 4 of 7.

**Codebase verified:** 2026-05-22. Key facts:
- Control vars `INITIAL TIME`/`FINAL TIME`/`TIME STEP`/`SAVEPER` are **ordinary `Variable`s** in `mSymbolNameSpace` (not Model fields). `Model::GetConstanValue(name, default)` (`src/Model.cpp:604-616`) reads a variable's numeric equation, falling back to `default` when the equation isn't a bare number (e.g. `SAVEPER = TIME STEP`). The `Model::_initial_time/_final_time/_dt` fields are only defaults. **Do NOT reuse the XMILE `SetUnwanted` suppression** — for `.mdl` we EMIT these as `.Control` equations.
- Integration: `enum Integration_Type { Integration_Type_EULER, Integration_Type_RK2, Integration_Type_RK4 }` (`src/Model.h:8`); `Model::IntegrationType()` (`Model.h:63-68`). Vensim `15:` code: parser reads the 4th field (`VensimParse.cpp:328-349`), mapping 0/2->Euler, 1/5->RK4, 3/4->RK2. **Emit:** Euler->0, RK4->1, RK2->3.
- Groups: `ModelGroup` (`src/Symbol/Symbol.h:17-32`) has `sName` and `std::vector<Variable*> vVariables`; `Model::Groups()` (`Model.h:69-71`); `Variable::GetGroup()` (`Variable.h:214`). Parser builds groups from `***...***` banners (`VensimLex.cpp:161-186`, `VensimParse.cpp:235-251`) and assigns each variable to the most-recent banner at `AddFullEq` (`VensimParse.cpp:173-179`).
- Unit equivalences: `Model::UnitEquivs()` returns `std::vector<std::string>&` of the **raw `22:`-line payloads** (`Model.h:45-47`; populated at `VensimParse.cpp:350-353`).
- Settings parsing: only `15:` and `22:` are consumed; the marker is `:L\x7F<%^E!@` with a literal **DEL byte (0x7F)** (`VensimParse.cpp:320-358`). The `///---\\\` sketch terminator must precede it.

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC3: Round-trip preserves the model
- **mdl-writer.AC3.3 Success:** Sim specs (initial/final/dt/saveper, integration method) and groups are equivalent across the round-trip.

---

<!-- START_SUBCOMPONENT_A (tasks 1-3) -->
<!-- START_TASK_1 -->
### Task 1: Group-aware equation emission + `.Control` group

**Verifies:** mdl-writer.AC3.3

**Files:**
- Modify: `src/Mdl/MDLGenerator.{h,cpp}` (restructure `GenerateEquations`; add `GenerateControl`)

**Implementation:**

The four control variable names are special. Define:
```cpp
static bool IsControlVar(const std::string &name);  // case-insensitive match of the 4 names
```
matching `INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER`.

Restructure equation emission to be group-aware (port simlin `writer.rs:2919-2994`):
1. Build the set of non-control variables, partitioned by `GetGroup()`.
2. For each `ModelGroup` in `Model::Groups()` (skip a `.Control`/`Control` group — emitted in step 4): emit the **banner**, then its `vVariables` (non-control, skipping `Unwanted()` and `ARRAY_ELM`) via `GenerateVariableEntry` (Phase 3).
3. Emit ungrouped non-control variables (those with `GetGroup()==nullptr`) after the last group.
4. `GenerateControl`: emit the `.Control` banner, then `INITIAL TIME`, `FINAL TIME`, `TIME STEP`, `SAVEPER` — each emitted from its actual `Variable` (looked up in the namespace via `mSymbolNameSpace.Find(name)`) using `GenerateVariableEntry`, so a `SAVEPER = TIME STEP` equation and the variables' real units/comments round-trip. If a control variable is absent from the namespace, synthesize a numeric entry from `GetConstanValue(name, fallback)` (fallbacks: `initial_time()`, `final_time()`, `dt()`, and `dt()` for SAVEPER).

Banner helper (port simlin `writer.rs:2934-2941`): a line of 56 `*`, then `\t<name>`, then 56 `*` immediately followed by `~`, then `\t\t<doc>`, then `\t|`:
```cpp
void EmitGroupBanner(std::string &out, const std::string &name, const std::string &doc) {
  const std::string bar(56, '*');
  out += "\n" + bar + "\n\t" + name + "\n" + bar + "~\n\t\t" + doc + "\n\t|\n";
}
```
- For non-control groups, `name` is the group's `sName` emitted **verbatim** so re-parse stores the identical `sName` (round-trips). **Verify** by parsing a grouped model whether `ModelGroup::sName` retains a leading `.`; emit exactly what was stored. For the `.Control` group, use `name = ".Control"` and `doc = "Simulation Control Parameters"` (matches simlin).
- **`AdjustGroupNames` parity (see Phase 1):** `sName` is the value *after* `Model::AdjustGroupNames()` ran (it disambiguates colliding names by appending `" 1"`, `Model.cpp:443-467`). Because the round-trip harness runs `AdjustGroupNames()` on both `M0` and `M1` (Phase 1 Task 5), emitting the post-adjustment `sName` and re-parsing it is idempotent — `M1`'s adjusted `sName` equals `M0`'s. Do **not** try to reverse the adjustment.

`Print` order becomes: `{UTF-8}` header, [macros — Phase 6], grouped + ungrouped non-control variables, `GenerateControl`, [sketch — Task 3 / Phase 5], [settings — Task 2].

**Skip control vars in the main loop:** `GenerateVariableEntry`/the group loops must skip any variable where `IsControlVar(v->GetName())` (they are emitted only in `GenerateControl`). Note: control vars are NOT `Unwanted()` on the `.mdl` path (we never call the XMILE `generateSimSpecs`), so the `Unwanted()` skip alone is insufficient — add the explicit `IsControlVar` skip.

**Testing:** Covered by Task 4 round-trip tests.

**Commit:** `feat(mdl): group banners and .Control sim-spec section`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: Settings section (`:L...`, `15:`, `22:`)

**Verifies:** mdl-writer.AC3.3

**Files:**
- Modify: `src/Mdl/MDLGenerator.{h,cpp}` (add `GenerateSettings`; CRLF conversion in `Print`)

**Implementation:**

`Print` assembles with `\n` and converts to CRLF once at the end:
```cpp
// at the end of Print, before returning:
std::string crlf;
crlf.reserve(out.size() + out.size() / 16);
for (char c : out) { if (c == '\n') crlf += "\r\n"; else crlf += c; }
return crlf;
```
Refactor the Phase 1 shell to emit `"{UTF-8}\n"` (the final conversion produces the CRLF).

`GenerateSettings` ports `write_settings_section` (`writer.rs:3217-3282`). It runs **after** the sketch terminator `///---\\\` (emitted by `GenerateSketch`, Task 3). Emit (all lines `\n`-terminated; the final pass adds CR):
```cpp
out += ":L";
out += '\x7F';                 // DEL byte 0x7F -- required by VensimParse.cpp:321
out += "<%^E!@\n";
// 22: unit equivalences (raw payloads, already in comma form)
for (const std::string &eq : _model->UnitEquivs()) {
  out += "22:" + eq + "\n";
}
// 15: integration method
int code = 0;                  // Euler
if (_model->IntegrationType() == Integration_Type_RK4) code = 1;
else if (_model->IntegrationType() == Integration_Type_RK2) code = 3;
out += "15:0,0,0," + std::to_string(code) + ",0,0\n";
// Vensim default cosmetic lines (ignored by xmutil's parser, aid real Vensim)
out += "19:100,0\n27:0,\n34:0,\n4:Time\n35:Date\n36:YYYY-MM-DD\n";
out += "37:2000\n38:1\n39:1\n40:2\n41:0\n42:0\n";
// 24/25/26: display time range (start, stop, stop)
double start = _model->GetConstanValue("INITIAL TIME", _model->initial_time());
double stop = _model->GetConstanValue("FINAL TIME", _model->final_time());
out += "24:" + mdl::FormatMDLNumber(start) + "\n";
out += "25:" + mdl::FormatMDLNumber(stop) + "\n";
out += "26:" + mdl::FormatMDLNumber(stop) + "\n";
```

**Testing:** Task 4 (integration type + unit equivs round-trip).

**Commit:** `feat(mdl): emit :L settings section (integration, unit equivs)`
<!-- END_TASK_2 -->

<!-- START_TASK_3 -->
### Task 3: Minimal sketch frame (placeholder for Phase 5)

**Verifies:** mdl-writer.AC3.3 (lets settings re-parse)

**Files:**
- Modify: `src/Mdl/MDLGenerator.{h,cpp}` (add `GenerateSketch` stub)

**Implementation:**

`GenerateSketch` for Phase 4 emits a valid empty sketch frame so the `///---\\\` terminator (and thus the settings section) re-parses. Phase 5 replaces the body with real view records.
```cpp
void MDLGenerator::GenerateSketch(std::string &out) {
  // \\\---/// opener, version line, one empty view, ///---\\\ terminator.
  out += "\\\\\\---///\n";
  out += "V300  Do not put anything below this section - it will be ignored\n";
  out += "*View 1\n";
  out += "$192-192-192,0,Helvetica|10|B|0-0-0|0-0-0|-1--1--1|-1--1--1|96,96,100,0\n";
  out += "///---\\\\\\\n";
}
```
(Note the C++ escaping: `"\\\\\\---///"` is the 9-char marker `\\\---///`; `"///---\\\\\\"` is `///---\\\`.)

Call order in `Print`: ... `GenerateControl(out)` then `GenerateSketch(out)` then `GenerateSettings(out)`.

**Comparator update — ignore empty views:** Because this frame creates one element-less `*View` for models that had no sketch, update `ModelComparator`'s view comparison to **skip views with zero elements** (an empty view carries no geometry, so "0 views" and "1 empty view" are equivalent). This satisfies AC4.2/AC4.3 and keeps Phase 4 round-trips clean for sketch-less models.

**Testing:** Task 4.

**Commit:** `feat(mdl): minimal sketch frame so settings re-parse`
<!-- END_TASK_3 -->
<!-- END_SUBCOMPONENT_A -->

<!-- START_TASK_4 -->
### Task 4: Control/settings/groups round-trip tests

**Verifies:** mdl-writer.AC3.3

**Files:**
- Create: `test/mdl/ControlRoundTripTest.cpp` (added to `xmutil_test`)
- Modify: `test/mdl/ModelComparator.cpp` (add unit-equiv comparison; exclude control vars from variable/group comparison)

**Implementation:**

Strengthen `ModelComparator`:
- **Variable comparison:** skip variables where `IsControlVar(GetName())` (compared via sim specs instead).
- **Group comparison:** compare groups by `sName` + non-control member set; ignore a group whose non-control membership is empty (handles `.Control` and any banner that only held control vars).
- **Sim specs:** already compared in Phase 1 (`GetConstanValue` for the 4 + `IntegrationType()`); confirm present.
- **Unit equivs:** compare `Model::UnitEquivs()` as a multiset of strings.
- **Views:** ignore zero-element views (Task 3).

**Testing (describe):** `roundtrip::RoundTripDiffs` over fixtures (assert empty diffs):
- **Sim specs + integration** (AC3.3): a model with explicit `INITIAL TIME = 0`, `FINAL TIME = 100`, `TIME STEP = 0.25`, `SAVEPER = TIME STEP`, and a `15:` line selecting RK4 — confirm `IntegrationType()` and all four values match after round-trip. Include a separate fixture selecting RK2 and one defaulting to Euler.
- **Groups** (AC3.3): a model with two `***...***` group banners each owning a couple of variables — confirm group names + memberships match.
- **Unit equivalences** (AC3.3): a model whose settings include `22:Hour,Hours` and `22:$,Dollar,Dollars` — confirm `UnitEquivs()` matches.
- **`SAVEPER = TIME STEP`** specifically: confirm the equation round-trips (not just the numeric fallback) — i.e. M1's SAVEPER equation is still `TIME STEP`, not a literal number.

**Verification:** `out/Debug/xmutil_test` passes all control/settings/group round-trip tests.

**Commit:** `test(mdl): control, settings, and group round-trip tests`
<!-- END_TASK_4 -->

## Phase 4 Done When
- Round-trip tests confirm equivalent sim specs, integration method, groups, and unit equivalences (AC3.3).
