# XMILE Reader Test Plan

Test plan for the XMILE reader implementation (8 phases plus later corpus and
hardening waves). Generated from the test-analyst coverage validation and kept
current as the suite grew.

## Coverage Validation

- Automated criteria: 28
- Covered: 28
- Missing: 0
- Human-only criteria: 1 (AC6.3, WASM build)

Automated verification (current HEAD): `out/Debug/xmutil_test` -> `198 tests,
0 failed`; `bash test/cli_xmile_roundtrip.sh` -> `XMILE CLI round-trip OK`;
`bash test/cli_roundtrip.sh` -> `CLI round-trip OK`.

The suite is anchored by two on-disk corpora walked from `test/fixtures/`
(provenance in `test/fixtures/README.md`):

- The XMILE corpus (`test/xmile/XmileCorpusTest.cpp`): 118 allow-listed
  `.xmile`/`.stmx` models, each asserted to round-trip in both directions
  (XMILE -> XMILE and XMILE -> MDL -> VensimParse) with an empty
  `ModelComparator` diff. A deferred table names the specific unsupported
  feature for every non-round-tripping candidate, a rejection table asserts the
  by-design `<module>`/`<macro>` failures, and a coverage walk fails if any
  `.xmile`/`.stmx` on disk (171 at present) is in none of the tables.
- The MDL corpus (`test/mdl/CorpusRoundTripTest.cpp`): 64 allow-listed Vensim
  `.mdl` models round-tripped through the writer, plus a large-model stress
  test over the 1.4 MB `C-LEARN v77` export that asserts the writer is a
  byte-exact fixpoint.

## Prerequisites

- Clean checkout at HEAD of branch `mdl-writer` with `third_party/simlin/`
  populated.
- Native build toolchain (ninja, bison, flex) plus emscripten for the WASM
  step.
- Repo root: `/home/bpowers/src/xmutil`.

## Phase 1: Build verification (native)

| Step | Action | Expected |
|------|--------|----------|
| 1.1 | `./configure.sh` | Exit 0; `out/Debug/build.ninja` exists |
| 1.2 | `ninja -C out/Debug XMUtil` | Exit 0; `out/Debug/XMUtil` exists and is executable |
| 1.3 | `ninja -C out/Debug xmutil_test` | Exit 0; `out/Debug/xmutil_test` exists |
| 1.4 | `out/Debug/xmutil_test` | Final line `198 tests, 0 failed (0 total check failures)` |
| 1.5 | `bash test/cli_roundtrip.sh` | `CLI round-trip OK` (AC6.1 regression guard) |
| 1.6 | `bash test/cli_xmile_roundtrip.sh` | `XMILE CLI round-trip OK` (AC1.4, AC1.6) |

## Phase 2: CLI XMILE -> XMILE dispatch and clobber guard (AC1.4, AC1.6)

| Step | Action | Expected |
|------|--------|----------|
| 2.1 | `mkdir /tmp/xmile-test && cp third_party/simlin/default_projects/logistic-growth/model.xmile /tmp/xmile-test/m.xmile` | `/tmp/xmile-test/m.xmile` exists |
| 2.2 | `out/Debug/XMUtil /tmp/xmile-test/m.xmile` | Exit 0; both `m.xmile` (unchanged) and `m.regen.xmile` exist; the latter starts with `<xmile` |
| 2.3 | `cmp /tmp/xmile-test/m.xmile third_party/simlin/default_projects/logistic-growth/model.xmile` | No output (input untouched) |
| 2.4 | `out/Debug/XMUtil --to-mdl /tmp/xmile-test/m.xmile` | Exit 0; `/tmp/xmile-test/m.mdl` exists; `head -1` shows `{UTF-8}` |
| 2.5 | `cp third_party/simlin/default_projects/logistic-growth/model.xmile /tmp/xmile-test/m2.STMX && out/Debug/XMUtil /tmp/xmile-test/m2.STMX` | Exit 0; `/tmp/xmile-test/m2.xmile` exists with `<xmile` first line (case-insensitive `.STMX` dispatch) |

## Phase 3: Corpus structural check (AC3.1, AC3.3)

| Step | Action | Expected |
|------|--------|----------|
| 3.1 | `out/Debug/XMUtil third_party/simlin/default_projects/fishbanks/model.xmile` | Exit 0; `.regen.xmile` written beside the input |
| 3.2 | `out/Debug/XMUtil --to-mdl third_party/simlin/default_projects/fishbanks/model.xmile` | Exit 0; `model.mdl` produced; first line `{UTF-8}` |
| 3.3 | Open the generated `.mdl` in Vensim or another Vensim-compatible viewer | Model opens without error; stock-flow diagram renders with correct connector polarity and aliases |
| 3.4 | Repeat 3.1-3.3 for `logistic-growth` and `reliability` corpora | All three corpora load and render |

## Phase 4: WASM build verification (AC6.3) - REQUIRED MANUAL

| Step | Action | Expected |
|------|--------|----------|
| 4.1 | `emcc --version` | Reports a version (emscripten on PATH) |
| 4.2 | `./configure.sh --use-wasm` | Exit 0; ninja regenerated for the wasm profile |
| 4.3 | `ninja -C out/Debug XMUtil_wasm` | Exit 0; linker accepts the new `src/Xmile/*.cpp` sources |
| 4.4 | `ls out/Debug/xmutil.js out/Debug/xmutil.wasm` | Both files exist and are non-empty |
| 4.5 | `./configure.sh` | Restores native build profile |

If emscripten is unavailable locally, defer 4.1-4.4 to CI and record the WASM
build status from the CI job that ran against this HEAD as evidence.

## End-to-End: Corpus model fidelity across both writers

Validates that the new XMILE reader composed with the existing XMILE writer
and the existing MDL writer preserves model semantics for a representative
model with stocks, flows, lookups, and clouds (AC3.1, AC3.3, AC4.1-AC4.3).

1. `out/Debug/XMUtil --to-mdl third_party/simlin/default_projects/fishbanks/model.xmile`
   produces `model.mdl` beginning with `{UTF-8}`.
2. Open `third_party/simlin/default_projects/fishbanks/model.xmile` in Stella
   (or another XMILE-aware tool) and visually note: stock/flow topology,
   group/sector membership, and at least two connector polarities (one `+`,
   one `-` if present).
3. Open the generated `model.mdl` in Vensim. Confirm the same stock/flow
   topology, the same group membership, and the same connector polarities are
   visible.
4. Re-run `out/Debug/XMUtil model.mdl` (Vensim -> XMILE) and diff against the
   `.regen.xmile` from `out/Debug/XMUtil model.xmile`. Differences should be
   limited to documented lossy mappings (extrapolate flag emission, dt
   reciprocal resolution).

## End-to-End: Negative-path rejection messaging surfaces to the CLI

Verifies the rejection ACs (AC5.1-AC5.3, AC2.9) produce useful CLI feedback,
not silent failures.

1. Create `/tmp/xmile-test/with-module.xmile` containing the `kWithModule`
   fixture text from `test/xmile/ErrorRejectTest.cpp`.
2. `out/Debug/XMUtil /tmp/xmile-test/with-module.xmile` -> non-zero exit;
   message contains "modules are not supported" attributable to the
   `<module name="m1">` element.
3. Repeat with the `kWithMacro` fixture; message mentions "macro".
4. Repeat with the `kMultiModel` fixture; message mentions "multiple <model>".
5. Repeat with the `kPostfixApostrophe` fixture; a parse error points at the
   apostrophe.

## Human Verification Required

| Criterion | Why Manual | Steps |
|-----------|------------|-------|
| AC6.3 (WASM build) | Emscripten is not a test-suite dependency and `xmutil_test` does not build for WASM; the gyp target's source participation is the structural guarantee, but the linker run is the operational gate. | Phase 4 above. If emscripten is unavailable locally, attach CI build logs for this HEAD as evidence. |

## Traceability

| AC | Automated Test | Manual Step |
|----|----------------|-------------|
| AC1.1 | BasicSmokeTest + every round-trip test | Phase 1.4 |
| AC1.2 | CEntry_xmile_to_xmile_returns_owned_buffer, CEntry_null_filename_is_accepted | Phase 1.4 |
| AC1.3 | CEntry_xmile_to_mdl_returns_owned_buffer | Phase 1.4 |
| AC1.4 | xmile_envelope_parses + cli_xmile_roundtrip.sh | Phase 1.6, Phase 2.2/2.4/2.5 |
| AC1.5 | xmile_non_xml_input_errors, xmile_wrong_root_errors, ErrorReject_convert_xmile_to_*_returns_null_on_error | Phase 1.4, End-to-End negative-path |
| AC1.6 | cli_xmile_roundtrip.sh | Phase 2.2/2.3 |
| AC2.1 | XmileParse_addition_builds_ExpressionAdd, ..._precedence_a_plus_b_times_c | Phase 1.4 |
| AC2.2 | XmileFunctions_function_* + XmileParse_function_smth1_resolves_to_SMOOTH | Phase 1.4 |
| AC2.3 | XmileFunctions_bare_keyword_* + XmileParse_bare_* | Phase 1.4 |
| AC2.4 | XmileParse_if_then_else_builds_IF_THEN_ELSE | Phase 1.4 |
| AC2.5 | XmileParse_subscripted_variable_x_a_b + Array_*_round_trip | Phase 1.4, Phase 3 |
| AC2.6 | XmileParse_keyword_and_matches_C_style_amp_amp (+ or/not/mod variants) | Phase 1.4 |
| AC2.7 | XmileParse_safediv_two_args_uses_ZIDZ, XmileParse_safediv_three_args_uses_XIDZ | Phase 1.4 |
| AC2.8 | XmileParse_unknown_function_errors | Phase 1.4 |
| AC2.9 | XmileParse_postfix_apostrophe_errors, ErrorReject_postfix_apostrophe_in_equation | Phase 1.4, End-to-End negative-path step 5 |
| AC3.1 | Corpus_*_xmile_to_xmile + slice round-trips | Phase 3.1/3.4 |
| AC3.2 | AuxRoundTrip_units_round_trip, AuxRoundTrip_dt_reciprocal_resolves, Array_dim_classified_as_ARRAY_after_pipeline | Phase 1.4 |
| AC3.3 | Corpus_*_xmile_to_mdl, View_corpus_*_to_mdl | Phase 3.2-3.4 + End-to-End corpus fidelity |
| AC3.4 | View_comparator_guard_detects_corruption | Phase 1.4 |
| AC4.1 | View_corpus_fishbanks_to_mdl, View_corpus_reliability_to_mdl | End-to-End corpus fidelity steps 2-3 |
| AC4.2 | View_connector_polarity_plus_round_trips | End-to-End corpus fidelity step 2 |
| AC4.3 | View_clouds_produce_VensimCommentElement | Phase 3.3 (cloud rendering in Vensim) |
| AC4.4 | View_multi_view_takes_first_warns_second | Phase 1.4 |
| AC4.5 | AuxRoundTrip_group_round_trip, View_corpus_reliability_to_mdl | End-to-End corpus fidelity (group membership) |
| AC5.1 | AuxRoundTrip_module_is_rejected, ErrorReject_module_with_clear_message | End-to-End negative-path step 2 |
| AC5.2 | ErrorReject_macro_with_clear_message | End-to-End negative-path step 3 |
| AC5.3 | ErrorReject_multiple_models | End-to-End negative-path step 4 |
| AC5.4 | AuxRoundTrip_foreign_namespace_silently_skipped | Phase 1.4 |
| AC6.1 | All `test/mdl/` tests via xmutil_test | Phase 1.4-1.5 |
| AC6.2 | MdlXmileByteIdentity_teacup | Phase 1.4 |
| AC6.3 | (none) | Phase 4 (REQUIRED MANUAL) |
