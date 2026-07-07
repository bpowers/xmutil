# Vensim mdl Writer Implementation Plan — Phase 7: Corpus round-trip, CLI end-to-end, and Dynamo

**Goal:** Validate breadth (a corpus of real models round-trips through the comparator), the CLI path end-to-end (file in -> `.mdl` out -> re-parse), the C-entry contract (owned string / NULL on error), and Dynamo->Vensim (`.dyn` -> valid `.mdl` re-parsing to an equivalent `Model`, no sketch).

**Architecture:** New tests over the existing in-memory round-trip harness (`roundtrip::RoundTripDiffs`) plus a small shell script that drives the built `XMUtil` binary for the CLI file-output check. No production code beyond what Phases 1-6 added (the CLI flag and C entry already exist from Phase 1).

**Tech Stack:** C++17, gyp, the minimal custom test harness. Corpus fixtures from `third_party/simlin/test/...`.

**Scope:** Phase 7 of 7.

**Codebase verified:** 2026-05-22. Key facts:
- `test_models/` contains only the large (~1.4 MB) `C-LEARN v77 for Vensim.mdl` (has a sketch; macro-heavy), its tiny `.xmile` stub, and `Ref.vdf`. **No small fixtures live here** — small models come from `third_party/simlin/test/`.
- simlin corpus (267 `.mdl` files): small single-feature models under `third_party/simlin/test/test-models/tests/` and `third_party/simlin/test/sdeverywhere/models/`; sketched examples `third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl` and `.../simlin-serve/tests/fixtures/SIR.mdl`; the `macro_*` set under `test/test-models/tests/`.
- **No `.dyn` files exist anywhere in-tree** — a Dynamo fixture must be authored. Dynamo parsing: `convert_to_mdl` branches to `DynamoParse` on a `.dyn`/`.DYN` filename extension (`src/XMUtil.cpp:268-276`); `DynamoParse` creates **no views** (`Model::Views()` stays empty) and sets `set_from_dynamo(true)`.
- In-memory re-parse: `VensimParse::ProcessFile(filename, contents, len)` (`src/Vensim/VensimParse.h:26`) and `DynamoParse::ProcessFile(...)` (`src/Dynamo/DynamoParse.h:26`) parse a buffer; `filename` is used only for error messages — no disk read.
- CLI: `convert_to_mdl` C entry + `--to-mdl` flag (added Phase 1) write `<base>.mdl` (or `.regen.mdl` if that would clobber the input). Built binary: `out/Debug/XMUtil` (gyp/ninja default `Debug`; verify at build time). `convert_to_mdl` returns a `strdup`'d caller-owned string (caller `free`s) or `NULL` on parse/generation error (mirrors `convert_mdl_to_xmile`, `src/XMUtil.cpp:254-327`).

---

## Acceptance Criteria Coverage

This phase implements and tests:

### mdl-writer.AC1: Model serializes to Vensim mdl via a stable API
- **mdl-writer.AC1.2 Success:** The C entry (e.g. `convert_to_mdl(source, len, fileName, ...)`) returns a caller-owned `.mdl` string, mirroring `convert_mdl_to_xmile`.
- **mdl-writer.AC1.4 Failure:** On generation error, messages are collected in `errs`; `PrintMDL` returns an empty string and the C entry returns NULL.

### mdl-writer.AC3: Round-trip preserves the model (breadth)
- **mdl-writer.AC3.1-AC3.4** validated across the corpus (variables/types/ASTs, subscripts/units/comments, sim specs/groups, sketch geometry).

### mdl-writer.AC5: Dynamo to Vensim
- **mdl-writer.AC5.1 Success:** A `.dyn` model converts to valid `.mdl` that re-parses to an equivalent `Model` (no sketch comparison).

---

<!-- START_TASK_1 -->
### Task 1: Corpus round-trip test

**Verifies:** mdl-writer.AC3.1, AC3.2, AC3.3, AC3.4 (breadth)

**Files:**
- Create: `test/mdl/CorpusRoundTripTest.cpp` (added to `xmutil_test`)
- Modify: `XMUtil.gyp` (define a `XMUTIL_SRC_ROOT` preprocessor macro on the `xmutil_test` target so the test can locate fixtures regardless of CWD, e.g. `'defines': ['XMUTIL_SRC_ROOT="<(cwd)"']` — verify the gyp variable that expands to the repo root; alternatively run the test with CWD = repo root and use relative paths)

**Implementation / Testing (describe):**

The test reads each fixture file from disk into a string and calls `roundtrip::RoundTripDiffs(text)`, asserting an empty diff list. Build the absolute path as `std::string(XMUTIL_SRC_ROOT) + "/" + relativePath`.

Curate a list of small, xmutil-parseable Vensim models (start with ~10-20, expand as they pass). Seed list:
- `third_party/simlin/test/sdeverywhere/models/delay/delay.mdl`
- `third_party/simlin/test/sdeverywhere/models/trend/trend.mdl`
- `third_party/simlin/test/sdeverywhere/models/pulsetrain/pulsetrain.mdl`
- `third_party/simlin/test/sdeverywhere/models/elmcount/elmcount.mdl`
- `third_party/simlin/test/sdeverywhere/models/subalias/subalias.mdl`
- `third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl` (sketch)
- `third_party/simlin/src/simlin-serve/tests/fixtures/SIR.mdl` (sketch, polarity)
- a selection of `third_party/simlin/test/test-models/tests/*/` single-feature models.

**Filtering rule (important):** A model that xmutil's *parser* cannot read (parse failure) is **out of scope** — it indicates a parser gap, not a writer bug. The harness must distinguish: if the initial parse (`M0`) fails, **skip** the model (log it as skipped), not fail the test. Only a non-empty diff on a successfully-parsed model is a failure. Implement `RoundTripDiffs` to return a distinguished "skipped: initial parse failed" marker the test treats as a skip. Keep a maintained allow-list of models expected to round-trip cleanly; expanding it is how the corpus grows.

**v1 scope filter — `:EXCEPT:`:** v1 does not emit/compare the `:EXCEPT:` subscript-exception clause (see Phase 3). Exclude models whose source contains `:EXCEPT:` from the corpus allow-list (a simple substring check on the raw text is sufficient for curation), and record them as known-deferred. This keeps the round-trip bar honest: a model using `:EXCEPT:` would otherwise pass vacuously (both emit and comparator ignore the clause).

Optionally include the large `test_models/C-LEARN v77 for Vensim.mdl` behind a slow/opt-in flag (it is macro-heavy and exercises Phase 6); if it reveals gaps, add the specific small fixture that reproduces them rather than blocking on the 1.4 MB model.

**Verification:** `out/Debug/xmutil_test` passes; the corpus list round-trips with empty diffs (skips logged).

**Commit:** `test(mdl): corpus round-trip over simlin fixtures`
<!-- END_TASK_1 -->

<!-- START_TASK_2 -->
### Task 2: C-entry contract + CLI end-to-end

**Verifies:** mdl-writer.AC1.2, mdl-writer.AC1.4, mdl-writer.AC1.3

**Files:**
- Create: `test/mdl/CEntryTest.cpp` (added to `xmutil_test`)
- Create: `test/cli_roundtrip.sh` (CLI file-output check)

**Implementation / Testing (describe):**

`CEntryTest.cpp` (AC1.2, AC1.4) calls the C entry directly:
- **AC1.2:** `char *r = convert_to_mdl(text.c_str(), text.size(), "m.mdl", 0);` for a valid small model -> `r != nullptr`, `r` starts with `{UTF-8}`, and the returned string re-parses (`roundtrip::ParseVensim(r)` non-null). `free(r)` (caller owns it).
- **AC1.4:** `convert_to_mdl` on clearly invalid input (e.g. `"@#$ not a model %^&"`) returns `nullptr`. Also unit-test `Model::PrintMDL(errs)` returns an empty string and appends to `errs` on a forced generation error if one is reachable; if no generation error is reachable for a parsed model, assert the parse-failure path (`convert_to_mdl` returns NULL when `ProcessFile` fails) and document that `PrintMDL` itself does not currently fail for a well-formed `Model` (the error channel exists for forward-compatibility, mirroring `convert_mdl_to_xmile`'s `errs` handling at `src/XMUtil.cpp:315-321`).

`test/cli_roundtrip.sh` (AC1.3) — a small script (the project has no test runner; this is the CLI-level check):
```bash
#!/bin/bash
set -euo pipefail
BIN=${1:-out/Debug/XMUtil}
SRC="third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl"
TMP=$(mktemp -d)
cp "$SRC" "$TMP/teacup.mdl"
"$BIN" --to-mdl "$TMP/teacup.mdl"            # writes $TMP/teacup.regen.mdl (avoids clobber)
test -s "$TMP/teacup.regen.mdl"              # non-empty output file exists
head -1 "$TMP/teacup.regen.mdl" | grep -q '{UTF-8}'
"$BIN" --to-mdl --stdio < "$TMP/teacup.regen.mdl" | head -1 | grep -q '{UTF-8}'  # re-emits (re-parses)
rm -rf "$TMP"
echo "CLI round-trip OK"
```
Document in the script header that it requires a built `out/Debug/XMUtil` (or pass the path as `$1`). This is the AC1.3 evidence; the structural round-trip is already covered by Task 1.

**Verification:**
- `out/Debug/xmutil_test` passes the C-entry tests.
- `bash test/cli_roundtrip.sh out/Debug/XMUtil` prints `CLI round-trip OK`.

**Commit:** `test(mdl): C-entry contract and CLI end-to-end checks`
<!-- END_TASK_2 -->

<!-- START_TASK_3 -->
### Task 3: Dynamo -> Vensim

**Verifies:** mdl-writer.AC5.1

**Files:**
- Create: `test/mdl/fixtures/minimal.dyn` (hand-authored Dynamo model)
- Create: `test/mdl/DynamoToMdlTest.cpp` (added to `xmutil_test`)
- Modify: `test/mdl/RoundTrip.{h,cpp}` (add `DynamoToMdlDiffs` if not already present)

**Implementation / Testing (describe):**

Author a minimal `.dyn` fixture (a stock, a flow, an aux, and control specs) using Dynamo syntax that `DynamoParse` accepts. **Verify accepted syntax** by reading `src/Dynamo/DynamoParse.cpp` and `src/Dynamo/DynamoLex.*` (and the existing XMILE conversion of a Dynamo model if any sample can be found); a classic Dynamo level/rate/aux model is the target. Keep it tiny.

`DynamoToMdlDiffs(dynText)`:
1. Parse `dynText` with `DynamoParse` into `M0`, run `MarkVariableTypes(nullptr)` (+ per macro). `M0` has no views.
2. `M0->PrintMDL(errs)` -> `mdl'`.
3. Parse `mdl'` with `VensimParse` into `M1`, run `MarkVariableTypes`.
4. Return `ModelComparator::Compare(M0, M1)` **with sketch comparison disabled** (Dynamo has no views; the Vensim re-parse may have an empty view from the minimal frame — the comparator already ignores empty views, so this is automatic, but assert no view-related diffs).

Assertions (AC5.1): the diff list is empty — same variables, equation ASTs, types, sim specs; no sketch comparison. Also assert `mdl'` is non-empty and begins with `{UTF-8}`, and that step 3's parse succeeds (the emitted `.mdl` is valid).

**Verification:** `out/Debug/xmutil_test` passes the Dynamo->Vensim test.

**Commit:** `test(mdl): Dynamo-to-Vensim conversion round-trip`
<!-- END_TASK_3 -->

## Phase 7 Done When
- The curated corpus round-trips through the comparator (parse failures skipped, not failed), the CLI check passes, and a `.dyn` fixture converts and re-parses equivalently (AC1.2, AC1.4, AC5.1, and the breadth of AC3).

## Final integration note
After Phase 7, the full pipeline is: `source (.mdl/.dyn) -> parse -> MarkVariableTypes -> Model::PrintMDL/convert_to_mdl/--to-mdl -> .mdl text`, validated by `parse -> emit -> re-parse -> ModelComparator` over equations, sim specs, groups, units, macros, and (for Vensim input) sketch geometry. No view geometry is ever synthesized.
