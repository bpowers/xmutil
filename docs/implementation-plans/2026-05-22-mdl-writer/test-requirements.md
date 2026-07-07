# Vensim mdl Writer — Test Requirements

This document maps every acceptance criterion of the Vensim mdl-writer feature (from
`docs/design-plans/2026-05-22-mdl-writer.md`, "Acceptance Criteria") to a specific test, and
records the small set of criteria that fall to human verification.

## Test approach

There is no third-party test framework in this repo. Tests use a **minimal custom C++ harness**
(`test/TestHarness.{h,cpp}`) exposing `TEST(name)` registration and `CHECK`/`CHECK_EQ_STR`
assertion macros, compiled into a dedicated gyp executable target `xmutil_test` (built alongside
`XMUtil`). Test sources live under `test/` and `test/mdl/`.

The core verification mechanism is the **round-trip comparator**:
`roundtrip::RoundTripDiffs(mdlText)` parses the input, runs the full post-parse pipeline
(`MarkVariableTypes(nullptr)` -> `AdjustGroupNames()` -> per-macro `MarkVariableTypes(mf->NameSpace())`
-> `CheckGhostOwners()` — the same sequence `convert_to_mdl` uses), calls `Model::PrintMDL`,
re-parses the emitted text through the identical pipeline, and runs
`ModelComparator::Compare(M0, M1)`. A test passes when the returned diff list is empty. The
comparator is keyed by name (order-independent) and compares variable presence/type, structural
paren-insensitive equation ASTs, stock inflow/outflow sets, subscripts/dimensions, units,
comments, sim specs, integration method, groups, unit equivalences, `VensimView` geometry, and
macros. Equivalence is **structural/semantic, not byte-identical** — byte-for-byte reproduction of
Vensim's output is explicitly out of scope.

Three other test types supplement the round-trip comparator:
- **Golden tests** assert exact Vensim text for the expression walker (operator precedence and
  parenthesization, builtins emitted verbatim, logical operators, subscripted references, quoting,
  number/`:NA:` formatting), plus a render -> parse -> render idempotence guard.
- **Unit tests** cover pure formatting helpers (`FormatMDLNumber`, `FormatMDLIdent`,
  `WrapEquation`) directly.
- A **CLI shell check** (`test/cli_roundtrip.sh`) drives the built `XMUtil` binary for file
  in -> `.mdl` out -> re-emit, and a **C-entry contract test** exercises `convert_to_mdl`'s
  owned-string / NULL-on-error contract.

No part of the automated bar requires Vensim itself; correctness is proven entirely by
`parse -> emit -> re-parse` equivalence on a corpus of real models. The AC3.5 guard ensures the
comparator is not vacuous (it detects a deliberately introduced non-equivalence), so a passing
round-trip is meaningful rather than a no-op.

## Acceptance criteria coverage

| AC | AC text (from design) | Test type | Test file | Phase / Task | What the test asserts |
|----|-----------------------|-----------|-----------|--------------|-----------------------|
| mdl-writer.AC1.1 | `Model::PrintMDL(errs)` returns Vensim `.mdl` text for a parsed, `MarkVariableTypes`-processed `Model`. | unit / round-trip | `test/mdl/MDLGeneratorTest.cpp` | Phase 1, Tasks 4 & 6 | `PrintMDL` returns non-empty text starting with `{UTF-8}` with empty `errs`; the shell re-parses to a non-null `Model` end-to-end. |
| mdl-writer.AC1.2 | The C entry (`convert_to_mdl(source, len, fileName, ...)`) returns a caller-owned `.mdl` string, mirroring `convert_mdl_to_xmile`. | integration | `test/mdl/CEntryTest.cpp` | Phase 7, Task 2 | `convert_to_mdl` on a valid model returns a non-null `strdup`'d string starting with `{UTF-8}` that re-parses; caller `free`s it. |
| mdl-writer.AC1.3 | The CLI emits a `.mdl` file (e.g. `xmutil --to-mdl model.mdl`, or selected by output extension). | cli | `test/cli_roundtrip.sh` | Phase 7, Task 2 (wired Phase 1, Task 2) | `XMUtil --to-mdl <file>` writes a non-empty `.regen.mdl` whose first line is `{UTF-8}`, and `--to-mdl --stdio` re-emits a `{UTF-8}` shell (proves re-parse). |
| mdl-writer.AC1.4 | On generation error, messages are collected in `errs`; `PrintMDL` returns an empty string and the C entry returns NULL. | integration | `test/mdl/CEntryTest.cpp` | Phase 7, Task 2 | `convert_to_mdl` on clearly invalid input returns `nullptr`; the `errs`/empty-string error channel on `PrintMDL` is asserted on a forced/parse-failure path (documented that a well-formed `Model` does not currently fail). |
| mdl-writer.AC2.1 | Aux/flow variables emit as `name = <expr> ~ units ~ comment \|`, with Vensim function names via `Function::GetName()` and no XMILE expansion (`PULSE`, `IF THEN ELSE`, `DELAY FIXED` round-trip unchanged). | golden + round-trip | `test/mdl/MDLGeneratorTest.cpp` (walker golden); `test/mdl/EquationRoundTripTest.cpp` (full entry) | Phase 2, Tasks 5-6; Phase 3, Tasks 2 & 7 | Walker emits builtins verbatim (`IF THEN ELSE(...)`, `PULSE(...)`, `DELAY FIXED(...)`) and logical operators; the full `name = <expr> ~ units ~ comment \|` entry round-trips to an equivalent AST. |
| mdl-writer.AC2.2 | Stocks emit as `name = INTEG(<net_flow>, <init>)`, with `<net_flow>` from `Inflows()`/`Outflows()` and `<init>` from the stock's initial value. | round-trip | `test/mdl/EquationRoundTripTest.cpp` | Phase 3, Tasks 3 & 7 | A stock + flows fixture (`s = INTEG(inflow - outflow, 100)`) round-trips with empty diffs; comparator checks stock inflow/outflow name sets and the init expression. |
| mdl-writer.AC2.3 | Lookups/graphical functions emit Vensim syntax `name([(xmin,ymin)-(xmax,ymax)],(x1,y1),...)` (standalone) and `WITH LOOKUP(input, (...))` (embedded). | round-trip | `test/mdl/EquationRoundTripTest.cpp` | Phase 3, Tasks 4 & 7 | Standalone graphical-function, embedded `WITH LOOKUP`, and lookup-call fixtures round-trip; comparator checks table x/y values within tolerance. |
| mdl-writer.AC2.4 | Dimension definitions emit `Dim: e1, e2, e3 ~~\|`; subscripted variables emit per-element entries `name[elem] = ...` via stored equations. | round-trip | `test/mdl/EquationRoundTripTest.cpp` | Phase 3, Tasks 5 & 7 | A dimension definition plus subscripted variables (`v[Dim] = 1, 2, 3`, `w[Dim] = v[Dim] * 2`) round-trip; comparator checks element lists and per-equation subscript signatures. |
| mdl-writer.AC2.5 | Identifiers requiring quoting are quoted; numbers format correctly, including the `:NA:` sentinel for `-1e38`. | unit + golden | `test/mdl/MDLFormatTest.cpp`; `test/mdl/MDLGeneratorTest.cpp` (walker golden) | Phase 2, Tasks 4-5 | `FormatMDLNumber(-1e38) == ":NA:"`, integer/decimal round-trip; `FormatMDLIdent` quotes names with special chars and does not double-quote already-quoted input; walker emits `:NA:` and quoted refs. |
| mdl-writer.AC2.6 | Equations longer than Vensim's line budget wrap with `\` continuations and re-parse to the same expression. | unit + round-trip | `test/mdl/MDLFormatTest.cpp` (`WrapEquation`); `test/mdl/EquationRoundTripTest.cpp` (long fixture) | Phase 3, Tasks 1 & 7 | `WrapEquation` on a >80-char string inserts `\`-continuations and reconstructs the original token stream; a long-RHS aux fixture round-trips with empty diffs. |
| mdl-writer.AC3.1 | `parse -> MarkVariableTypes -> M0`, `emit -> mdl'`, `parse -> MarkVariableTypes -> M1` yields the same variables with equivalent equation ASTs and variable types. | round-trip | `test/mdl/EquationRoundTripTest.cpp`; `test/mdl/CorpusRoundTripTest.cpp` | Phase 3, Tasks 6-7; Phase 7, Task 1 | Per-fixture and corpus-wide empty diffs; comparator matches variable sets, `VariableType()`, and structural (paren-insensitive) equation ASTs. |
| mdl-writer.AC3.2 | Subscripts/dimensions, units, and comments are equivalent across the round-trip. | round-trip | `test/mdl/EquationRoundTripTest.cpp`; `test/mdl/CorpusRoundTripTest.cpp` | Phase 3, Tasks 5-7; Phase 7, Task 1 | Comparator reports zero diffs for subscript counts, dimension element lists, `Units()` text, and `Comment()`. |
| mdl-writer.AC3.3 | Sim specs (initial/final/dt/saveper, integration method) and groups are equivalent across the round-trip. | round-trip | `test/mdl/ControlRoundTripTest.cpp` | Phase 4, Task 4 | Fixtures with explicit `INITIAL/FINAL TIME`, `TIME STEP`, `SAVEPER = TIME STEP`, Euler/RK2/RK4, group banners, and `22:` unit equivs round-trip with empty diffs. |
| mdl-writer.AC3.4 | `VensimView` geometry (variables, valves, clouds, connectors, positions, polarity) is equivalent across the round-trip. | round-trip | `test/mdl/SketchRoundTripTest.cpp` | Phase 5, Tasks 2-3 | Sketched fixtures (teacup, SIR) round-trip; comparator matches element types, x/y/w/h, connector from/to, attached/ghost, and `+`/`-` polarity by UID index. |
| mdl-writer.AC3.5 | The `Model` comparator detects and reports a deliberately introduced non-equivalence (round-trip tests cannot pass vacuously). | unit | `test/mdl/MDLGeneratorTest.cpp` (`ModelComparator_detects_mutation`) | Phase 1, Tasks 5-6; strengthened Phase 3, Task 6 | `Compare` returns empty for a model parsed twice and a non-empty diff naming the change for two models differing in one constant/type; `ExpressionsEqual` distinguishes `a + b` from `a - b`. |
| mdl-writer.AC4.1 | Vensim input with a sketch produces a sketch section that re-parses to an equivalent `VensimView`. | round-trip | `test/mdl/SketchRoundTripTest.cpp` | Phase 5, Tasks 2-3 | teacup/SIR fixtures round-trip with empty view diffs; a focused assertion confirms a `+` and a `-` connector polarity survive. |
| mdl-writer.AC4.2 | Input without a `VensimView` (Dynamo or external consumer) produces no/empty sketch section and still parses. | round-trip | `test/mdl/SketchRoundTripTest.cpp`; `test/mdl/DynamoToMdlTest.cpp` | Phase 5, Task 3; Phase 7, Task 3 | A sketch-less inline `.mdl` round-trips (empty frame re-parses; comparator ignores zero-element views); the Dynamo path emits a valid `.mdl` that re-parses. |
| mdl-writer.AC4.3 | No diagram geometry is fabricated: variables absent from the original sketch are not assigned synthetic coordinates. | round-trip | `test/mdl/SketchRoundTripTest.cpp` | Phase 5, Task 3 | For a model whose sketch references only some variables, the round-tripped view has the same element count (no invented variable elements). |
| mdl-writer.AC5.1 | A `.dyn` model converts to valid `.mdl` that re-parses to an equivalent `Model` (no sketch comparison). | integration / round-trip | `test/mdl/DynamoToMdlTest.cpp` (fixture `test/mdl/fixtures/minimal.dyn`) | Phase 7, Task 3 | `DynamoToMdlDiffs` parses `.dyn` -> emits `.mdl` (begins with `{UTF-8}`) -> re-parses with Vensim; empty diffs over variables/ASTs/types/sim specs, no view-related diffs. |
| mdl-writer.AC6.1 | A model with a Vensim macro emits a `:MACRO: ... :END OF MACRO:` block that re-parses to an equivalent macro. | round-trip | `test/mdl/MacroRoundTripTest.cpp` | Phase 6, Tasks 1-2 | `macro_expression` and `macro_stock` fixtures round-trip; comparator matches macro name, parameter list, and body equation ASTs (including a macro-body `INTEG`). |

## Human verification

The automated bar deliberately does **not** require Vensim, and byte-identical output is out of
scope. The remaining manual check is narrow and justified:

- **Real-Vensim load and render (spot-check, not a CI gate).** Open a writer-produced `.mdl`
  (e.g. the `.regen.mdl` from `XMUtil --to-mdl` on `teacup.mdl` or `SIR.mdl`) in actual Vensim to
  confirm the file loads without error and the diagram renders with variables, flows, clouds, and
  connectors in the expected positions. This cannot be automated in CI because Vensim is a
  proprietary GUI application not available to the harness, and because the cosmetic sketch fields
  the writer emits as Vensim defaults (fonts, colors, line styles) are validated only by Vensim's
  own renderer — the automated comparator intentionally checks only xmutil-retained geometry
  (positions, types, links, polarity, attached/ghost). The structural correctness that CI *can*
  prove — that the emitted text re-parses to an equivalent `Model` — is fully covered by the
  round-trip tests above; the human check adds only the visual/renderer confirmation those tests
  cannot make.

This is the sole human-verified item; it is a one-time-per-release spot-check, not a per-commit
requirement.

## Coverage note

Every one of the 20 acceptance criteria (AC1.1, AC1.2, AC1.3, AC1.4, AC2.1, AC2.2, AC2.3, AC2.4,
AC2.5, AC2.6, AC3.1, AC3.2, AC3.3, AC3.4, AC3.5, AC4.1, AC4.2, AC4.3, AC5.1, AC6.1) maps to at
least one automated test (unit, golden, round-trip, integration, or CLI) as listed above. No AC
relies on human verification for its pass/fail determination. The single human-verification item
(opening a writer-produced `.mdl` in real Vensim to confirm load and render) is an additive
spot-check on top of fully automated structural coverage — explicitly justified by Vensim's
unavailability to CI and by byte-identical output being out of v1 scope.
