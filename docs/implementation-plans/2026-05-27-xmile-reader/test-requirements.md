# XMILE Reader — Test Requirements

This document maps every acceptance criterion in the XMILE reader design
(`docs/design-plans/2026-05-27-xmile-reader.md`) to either an automated test or
a documented human verification procedure. It is the audit trail for the eight
implementation phases (`phase_01.md` ... `phase_08.md`): every AC must land
somewhere, and every "covered by tests" claim in a phase must be checkable
against this file.

Notes on conventions used below:

- Test files live under `test/xmile/` and are built into the `xmutil_test`
  gyp target (`XMUtil.gyp` `xmutil_test` `sources` list).
- Test names are the `TEST(name)` macro literal from the harness in
  `test/TestHarness.h`. Running `out/Debug/xmutil_test` exercises them all.
- "Round-trip" means XMILE -> Model -> serialize -> Model' -> compare via
  `ModelComparator` (the format-agnostic structural comparator at
  `test/mdl/ModelComparator.{h,cpp}`).
- "Unit" means a small in-process test that exercises a specific function or
  class without driving the full pipeline.
- "Integration" means a test that drives multiple components together but
  stops short of a full file-format round-trip.
- "E2E / CLI" means a shell test (`test/cli_xmile_roundtrip.sh`) that exercises
  the `XMUtil` binary end to end.
- Several ACs surfaced design corrections during planning (AC2.3, AC2.8,
  AC4.2, AC4.3). Those corrections are noted on the relevant rows and the
  test description names the actual implemented mechanism.

---

## xmile-reader.AC1: Model populates from XMILE via a stable API

### AC1.1
- **Literal text:** "`Model::ParseXMILE(filename, contents, len, errs)` populates a `Model` that subsequently runs through `MarkVariableTypes` / `AdjustGroupNames` / `CheckGhostOwners` without error for a well-formed corpus XMILE input."
- **Phase:** 1 (envelope-level), then progressively tightened in 3 (auxes),
  4 (stocks + flows), 5 (arrays), 6 (lookups), 7 (views), 8 (full corpus).
- **Coverage type:** Automated.
- **Test file + names:**
  - `test/xmile/BasicSmokeTest.cpp` -- `xmile_envelope_parses`,
    `xmile_empty_envelope_round_trip` (Phase 1 envelope guarantee).
  - `test/xmile/AuxRoundTripTest.cpp` -- `AuxRoundTrip_auxes_only_round_trip`
    (Phase 3 scalar slice).
  - `test/xmile/StockFlowRoundTripTest.cpp` --
    `StockFlow_corpus_logistic_growth_round_trip`,
    `StockFlow_stock_classified_as_STOCK_after_pipeline` (Phase 4 stock slice).
  - `test/xmile/CorpusRoundTripTest.cpp` -- `Corpus_fishbanks_xmile_to_xmile`,
    `Corpus_logistic_growth_xmile_to_xmile`,
    `Corpus_reliability_xmile_to_xmile` (Phase 8 full corpus).
- **Notes:** The round-trip helpers (`xmileroundtrip::ParseXMILE`,
  `xmileroundtrip::RoundTripDiffs`, `xmileroundtrip::XmileToMdlDiffs` in
  `test/xmile/RoundTrip.{h,cpp}`) run `MarkVariableTypes`, `AdjustGroupNames`,
  per-macro `MarkVariableTypes`, and `CheckGhostOwners` after every parse.
  Any pipeline failure surfaces as either a non-empty errs vector or a
  diff entry, so the AC1.1 contract is checked by every round-trip test.

### AC1.2
- **Literal text:** "The extern-C entry `convert_xmile_to_xmile(source, len, fileName, longNames, sectors)` returns a caller-owned XMILE string for a well-formed input, with the same ownership contract as `convert_mdl_to_xmile`."
- **Phase:** 8.
- **Coverage type:** Automated.
- **Test file + names:** `test/xmile/CEntryTest.cpp` --
  `CEntry_xmile_to_xmile_returns_owned_buffer`,
  `CEntry_null_filename_is_accepted`.
- **Notes:** The test asserts a non-NULL return, that the buffer is `free()`-able,
  and that the output begins with `<?xml`. `convert_xmile_to_xmile` is declared
  alongside `convert_mdl_to_xmile` in `src/XMUtil.h` and its signature
  intentionally drops the `isCompact` flag (hardcoded to `false`) to match
  the AC1.2 shape; see Phase 1 Task 2's signature note.

### AC1.3
- **Literal text:** "The extern-C entry `convert_xmile_to_mdl(source, len, fileName, longNames)` returns a caller-owned MDL string for a well-formed input."
- **Phase:** 8.
- **Coverage type:** Automated.
- **Test file + names:** `test/xmile/CEntryTest.cpp` --
  `CEntry_xmile_to_mdl_returns_owned_buffer`.
- **Notes:** Asserts the buffer begins with `{UTF-8}` (the canonical Vensim
  MDL header) and is `free()`-able.

### AC1.4
- **Literal text:** "The CLI dispatches `.xmile` and `.stmx` inputs (case-insensitive) to the XMILE reader; without `--to-mdl` it emits XMILE; with `--to-mdl` it emits MDL."
- **Phase:** 1 (extension dispatch), then end-to-end-validated in 8.
- **Coverage type:** Automated.
- **Test file + names:**
  - In-process: `test/xmile/BasicSmokeTest.cpp` --
    `xmile_envelope_parses` (smoke for the reader dispatch path).
  - CLI shell: `test/cli_xmile_roundtrip.sh` (Phase 8). Drives
    `out/Debug/XMUtil` against `model.xmile` (no `--to-mdl`) and confirms
    `model.regen.xmile` exists; drives it with `--to-mdl` and confirms
    `model.mdl` exists with the `{UTF-8}` marker; copies the same fixture to
    `m2.STMX` (uppercase) and confirms case-insensitive `.stmx` dispatch.
- **Notes:** The case-insensitivity contract is covered by the explicit
  `m2.STMX` step in the shell test. `extensionMatchesInsensitive` in
  `src/Main.cpp` is the implementation; Phase 1 Task 3 introduces it.

### AC1.5
- **Literal text:** "On a parse / conversion error, messages are appended to `errs` and the extern-C entry returns NULL; `Model::ParseXMILE` returns false."
- **Phase:** 1 (wired in extern-C path), 8 (rejection-path tests).
- **Coverage type:** Automated.
- **Test file + names:**
  - `test/xmile/BasicSmokeTest.cpp` -- `xmile_non_xml_input_errors`,
    `xmile_wrong_root_errors` (envelope-level rejection).
  - `test/xmile/ErrorRejectTest.cpp` (Phase 8) --
    `ErrorReject_convert_xmile_to_xmile_returns_null_on_error`,
    `ErrorReject_convert_xmile_to_mdl_returns_null_on_error`.
- **Notes:** Each test asserts both `m == nullptr` (from `ParseXMILE`) or
  `out == nullptr` (from the extern-C entries) and `!errs.empty()`.

### AC1.6
- **Literal text:** "XMILE -> XMILE on a file named `model.xmile` writes `model.regen.xmile`; the input is not overwritten."
- **Phase:** 1 (rename logic), 8 (end-to-end smoke).
- **Coverage type:** Automated.
- **Test file + names:** `test/cli_xmile_roundtrip.sh` (Phase 8). Copies the
  corpus XMILE to a temp file `m.xmile`, runs `out/Debug/XMUtil m.xmile`, and
  asserts both files exist (`m.xmile` unchanged, `m.regen.xmile` non-empty
  XML).
- **Notes:** The rename logic lives in `src/Main.cpp` with two branches
  (Apple / non-Apple). Both branches are exercised on the platform that runs
  CI; the in-process tests cannot exercise the filename logic because the
  extern-C path doesn't touch filenames.

---

## xmile-reader.AC2: XMILE equations parse to xmutil Expression trees

### AC2.1
- **Literal text:** "XMILE arithmetic expressions parse to the same `ExpressionAdd` / `Subtract` / `Multiply` / `Divide` / `Power` shapes the Vensim parser produces; operator precedence matches the lalrpop grammar's lattice."
- **Phase:** 2.
- **Coverage type:** Automated (unit).
- **Test file + names:** `test/xmile/EquationParseTest.cpp` --
  `XmileParse_addition_builds_ExpressionAdd`,
  `XmileParse_subtraction_builds_ExpressionSubtract`,
  `XmileParse_multiplication_division_power`,
  `XmileParse_precedence_a_plus_b_times_c`.
- **Notes:** Tests `dynamic_cast` the result of `XmileReader::ParseEquation`
  to the expected node type and walk `GetArg(0)` / `GetArg(1)` to verify the
  precedence-defined tree shape. The precedence ladder lives in
  `src/Xmile/XmileEqYacc.y` and follows the simlin reference parser's C-like
  hierarchy (logical < comparison < additive < multiplicative < unary < `^`).

### AC2.2
- **Literal text:** "XMILE function names map to Vensim `Function*` via the un-rename table: `smth1` -> `SMOOTH`, `smth3` -> `SMOOTH3`, `delay` -> `DELAY FIXED`, `delay1` -> `DELAY1`, `delay3` -> `DELAY3`, `int` -> `INTEGER`, `safediv` -> `ZIDZ` (2-arg) or `XIDZ` (3-arg), `init` -> `ACTIVE INITIAL`, plus the generic `underbar_to_space().to_uppercase()` fallback for unmapped Vensim builtins."
- **Phase:** 2.
- **Coverage type:** Automated (unit at the lookup table; integration via the
  grammar).
- **Test file + names:**
  - Unit: `test/xmile/XmileFunctionsTest.cpp` --
    `XmileFunctions_function_smth1_maps_to_SMOOTH`,
    `XmileFunctions_function_safediv_2arg_is_ZIDZ`,
    `XmileFunctions_function_safediv_3arg_is_XIDZ`,
    `XmileFunctions_function_int_maps_to_INTEGER`,
    `XmileFunctions_function_fallback_uppercase_underscores`,
    `XmileFunctions_function_unknown_returns_null`.
  - Grammar integration: `test/xmile/EquationParseTest.cpp` --
    `XmileParse_function_smth1_resolves_to_SMOOTH`.
- **Notes:** The fallback rule (`underbar_to_space().to_uppercase()` then
  `Find`) is verified by `XmileFunctions_function_fallback_uppercase_underscores`,
  which uses `if_then_else` (no entry in the un-rename map) and expects
  `IF THEN ELSE`. `delay`/`delay1`/`delay3`/`smth3`/`init` arity coverage
  is implicitly delivered by the round-trip corpus tests in Phases 4-8 (the
  fishbanks / logistic-growth / reliability models exercise the relevant
  Vensim builtins).

### AC2.3
- **Literal text:** "Bare zero-argument keywords `time` / `dt` / `initial_time` / `final_time` resolve to `Time` / `TIME STEP` / `INITIAL TIME` / `FINAL TIME` as zero-arg `ExpressionFunction`s."
- **Phase:** 2.
- **Coverage type:** Automated (unit).
- **Test file + names:**
  - Lookup table: `test/xmile/XmileFunctionsTest.cpp` --
    `XmileFunctions_bare_keyword_time_creates_Time_variable`,
    `XmileFunctions_bare_keyword_dt_maps_to_TIME_STEP`,
    `XmileFunctions_bare_keyword_initial_time_maps_to_INITIAL_TIME`,
    `XmileFunctions_bare_keyword_unknown_returns_null`.
  - Grammar integration: `test/xmile/EquationParseTest.cpp` --
    `XmileParse_bare_time_resolves_to_Time_Variable`,
    `XmileParse_bare_dt_resolves_to_TIME_STEP_Variable`.
- **Notes / design correction:** The AC says "zero-arg `ExpressionFunction`s"
  but the codebase reality is that `Time`, `TIME STEP`, `INITIAL TIME`, and
  `FINAL TIME` are registered as ordinary `Variable*` objects in the
  namespace (the XMILE writer at `XMILEGenerator.cpp` and the MDL writer both
  expect Variables, not Functions). The implementation accordingly resolves
  these XMILE keywords to a `Variable*` (creating it if absent) and wraps it
  in `ExpressionVariable`, not `ExpressionFunction`. Phase 2 Task 1 and
  Task 5 use the corrected mechanism. The AC's intent -- a stable, single
  encoding for each bare keyword -- is preserved.

### AC2.4
- **Literal text:** "`if cond then a else b` parses to `ExpressionFunction("IF THEN ELSE", [cond, a, b])`."
- **Phase:** 2.
- **Coverage type:** Automated (unit).
- **Test file + names:** `test/xmile/EquationParseTest.cpp` --
  `XmileParse_if_then_else_builds_IF_THEN_ELSE`.
- **Notes:** Asserts the result `dynamic_cast`s to `ExpressionFunction`, then
  the function name is `IF THEN ELSE`. The grammar production `XPTT_if expr
  XPTT_then expr XPTT_else expr` lives in `src/Xmile/XmileEqYacc.y` and
  lowers via `xpyy_if` to `xpyy_call("IF THEN ELSE", args)`.

### AC2.5
- **Literal text:** "Subscripts (`x[a, b]`, `x[*]`, `x[*:Dim]`, `x[l:r]`, `x[@1]`) parse to `ExpressionVariable` with the corresponding subscript representation."
- **Phase:** 2 (grammar), 5 (end-to-end with dimensions populated).
- **Coverage type:** Automated (unit + round-trip).
- **Test file + names:**
  - Grammar: `test/xmile/EquationParseTest.cpp` --
    `XmileParse_subscripted_variable_x_a_b`.
  - End-to-end: `test/xmile/ArrayRoundTripTest.cpp` (Phase 5) --
    `Array_apply_to_all_aux_round_trip`,
    `Array_per_element_aux_round_trip`,
    `Array_apply_to_all_stock_round_trip`,
    `Array_per_element_stock_round_trip`,
    `Array_indexed_dim_expands_to_numeric_elements`.
- **Notes:** The grammar productions in `src/Xmile/XmileEqYacc.y` cover all
  five shapes (`name`, `*`, `*:Dim`, `l:r`, `@N`). The round-trip tests
  exercise the apply-to-all (`x[*]`-equivalent via `<dimensions>`) and
  per-element (`x[a]`-equivalent via `<element subscript="a">`) shapes; the
  bare named (`x[a, b]`) shape is covered by the grammar test.

### AC2.6
- **Literal text:** "Both keyword (`AND`, `OR`, `NOT`, `MOD`) and C-style (`&&`, `||`, `!`, `%`) operator spellings produce the same `ExpressionLogical` / operator nodes."
- **Phase:** 2.
- **Coverage type:** Automated (unit).
- **Test file + names:** `test/xmile/EquationParseTest.cpp` --
  `XmileParse_keyword_and_matches_C_style_amp_amp`,
  `XmileParse_keyword_or_matches_C_style_pipe_pipe`,
  `XmileParse_keyword_not_matches_bang`.
- **Notes:** Each test parses two equation strings (one keyword-spelled, one
  C-style-spelled) and asserts `LogicalOp(e1) == LogicalOp(e2)`. The lexer
  in `src/Xmile/XmileEqLex.cpp` returns the same `XPTT_*` token for both
  spellings; the grammar consumes the token type, not the original spelling,
  so the resulting `ExpressionLogical` carries identical `VPTT_*` operator
  codes. `MOD` vs `%` follows the same pattern through the `XPTT_mod` token
  and `xpyy_function("MODULO", ...)` lowering.

### AC2.7
- **Literal text:** "`//` (safediv) parses to `ExpressionFunction("ZIDZ", [l, r])` (matching the lalrpop's `App("safediv", [l, r])` shape)."
- **Phase:** 2.
- **Coverage type:** Automated (unit).
- **Test file + names:** `test/xmile/EquationParseTest.cpp` --
  `XmileParse_safediv_two_args_uses_ZIDZ`,
  `XmileParse_safediv_three_args_uses_XIDZ`.
- **Notes:** The two-arg test uses the `//` operator form (`a // b`); the
  three-arg test uses the explicit `safediv(a, b, 0)` call form to validate
  the arity-dispatched branch (2 -> ZIDZ, 3 -> XIDZ). Implementation lives
  in `xmile::LookupFunction` (`src/Xmile/XmileFunctions.cpp`).

### AC2.8
- **Literal text:** "A function name with no Vensim mapping in the un-rename table is reported as an error naming the unknown function; the parse fails."
- **Phase:** 2.
- **Coverage type:** Automated (unit).
- **Test file + names:** `test/xmile/EquationParseTest.cpp` --
  `XmileParse_unknown_function_errors`.
- **Notes / design correction:** The simlin reference writer falls through
  to the generic `underbar_to_space().to_uppercase()` transform for any name
  not in the un-rename table; the AC fires only when *that* fallback also
  fails to resolve to a registered Vensim `Function*`. The implementation
  (`xmile::LookupFunction` in `src/Xmile/XmileFunctions.cpp`) follows the
  two-step rule, and the test uses `completely_unknown_function` which has
  no map entry and no namespace match after the fallback transform.

### AC2.9
- **Literal text:** "A postfix transpose `'` in an equation is reported as a parse error."
- **Phase:** 2 (equation-grammar level), 8 (document-level regression).
- **Coverage type:** Automated (unit + integration).
- **Test file + names:**
  - Grammar: `test/xmile/EquationParseTest.cpp` --
    `XmileParse_postfix_apostrophe_errors`.
  - Document level: `test/xmile/ErrorRejectTest.cpp` (Phase 8) --
    `ErrorReject_postfix_apostrophe_in_equation`.
- **Notes:** The grammar production `expr XPTT_apostrophe` calls `xpyyerror`
  + `YYABORT`. The Phase 8 document-level test guards against a future
  refactor that bypasses the grammar's rejection.

---

## xmile-reader.AC3: Round-trip preserves the model (primary bar)

### AC3.1
- **Literal text:** "For each corpus model (`fishbanks`, `logistic-growth`, `reliability`), `XMILE -> Model -> XMILE -> Model'` yields a `Model` equivalent to the first per `ModelComparator`: same variables, equivalent equation ASTs, same variable types."
- **Phase:** Built up incrementally in 3 (auxes), 4 (stocks+flows), 5 (arrays),
  6 (lookups), 7 (views with the comparator already structural), 8 (full
  corpus matrix).
- **Coverage type:** Automated (round-trip).
- **Test file + names:**
  - `test/xmile/AuxRoundTripTest.cpp` -- `AuxRoundTrip_auxes_only_round_trip`,
    `AuxRoundTrip_units_round_trip`, `AuxRoundTrip_group_round_trip` (scalar
    slice).
  - `test/xmile/StockFlowRoundTripTest.cpp` --
    `StockFlow_corpus_logistic_growth_round_trip`,
    `StockFlow_single_inflow_no_outflow_round_trip`,
    `StockFlow_multiple_in_and_out_round_trip`,
    `StockFlow_forward_reference_to_flow_ok` (stock+flow slice).
  - `test/xmile/ArrayRoundTripTest.cpp` --
    `Array_apply_to_all_aux_round_trip`,
    `Array_per_element_aux_round_trip`,
    `Array_apply_to_all_stock_round_trip`,
    `Array_per_element_stock_round_trip` (array slice).
  - `test/xmile/LookupRoundTripTest.cpp` --
    `Lookup_with_lookup_explicit_xpts_round_trip`,
    `Lookup_with_lookup_xscale_only_round_trip`,
    `Lookup_standalone_gf_round_trip`,
    `Lookup_extrapolate_flag_round_trip` (lookup slice).
  - `test/xmile/CorpusRoundTripTest.cpp` -- `Corpus_fishbanks_xmile_to_xmile`,
    `Corpus_logistic_growth_xmile_to_xmile`,
    `Corpus_reliability_xmile_to_xmile` (full corpus, Phase 8).
- **Notes:** Each round-trip test calls `xmileroundtrip::RoundTripDiffs`,
  which parses -> writes via `PrintXMILE` -> re-parses -> runs
  `ModelComparator::Compare` and CHECKs the diff list is empty.

### AC3.2
- **Literal text:** "Dimensions, unit definitions, groups, and sim specs (start, stop, dt, save_step, integration method) are equivalent across the XMILE round-trip."
- **Phase:** 3 (units, sim specs, groups), 5 (dimensions).
- **Coverage type:** Automated (round-trip + spot checks).
- **Test file + names:**
  - Sim specs: `test/xmile/AuxRoundTripTest.cpp` --
    `AuxRoundTrip_auxes_only_round_trip`,
    `AuxRoundTrip_dt_reciprocal_resolves` (verifies
    `dt reciprocal="true"` resolution and integration-type RK4 round-trip).
  - Units: `test/xmile/AuxRoundTripTest.cpp` --
    `AuxRoundTrip_units_round_trip` (`<model_units>` -> `Model::UnitEquivs`
    -> writer emission -> re-parse comparator equivalence).
  - Groups: `test/xmile/AuxRoundTripTest.cpp` --
    `AuxRoundTrip_group_round_trip`.
  - Dimensions: `test/xmile/ArrayRoundTripTest.cpp` --
    `Array_apply_to_all_aux_round_trip`,
    `Array_dim_classified_as_ARRAY_after_pipeline`,
    `Array_indexed_dim_expands_to_numeric_elements`.
- **Notes:** The `dt reciprocal="true"` resolution is documented as a
  one-way lossy mapping in the design glossary; the round-trip comparator
  compares resolved doubles (so `<dt reciprocal="true">4</dt>` -> `dt=0.25`
  -> writer emits `0.25` -> re-parse sees `0.25` -> match).

### AC3.3
- **Literal text:** "For each corpus model, `XMILE -> Model -> MDL -> Model'` yields a `Model` equivalent to the first."
- **Phase:** Built up incrementally in 4 (logistic-growth), 7 (fishbanks +
  reliability), and tied off in 8 (full corpus matrix).
- **Coverage type:** Automated (round-trip via MDL writer + Vensim re-parser).
- **Test file + names:**
  - `test/xmile/StockFlowRoundTripTest.cpp` (Phase 4) --
    `StockFlow_corpus_logistic_growth_to_mdl`.
  - `test/xmile/AuxRoundTripTest.cpp` (Phase 3) -- `AuxRoundTrip_xmile_to_mdl`.
  - `test/xmile/ArrayRoundTripTest.cpp` (Phase 5) -- `Array_apply_to_all_to_mdl`.
  - `test/xmile/ViewRoundTripTest.cpp` (Phase 7) --
    `View_corpus_fishbanks_to_mdl`, `View_corpus_reliability_to_mdl`.
  - `test/xmile/CorpusRoundTripTest.cpp` (Phase 8) --
    `Corpus_fishbanks_xmile_to_mdl`,
    `Corpus_logistic_growth_xmile_to_mdl`,
    `Corpus_reliability_xmile_to_mdl`.
- **Notes:** Each test calls `xmileroundtrip::XmileToMdlDiffs`, which parses
  the XMILE input, writes MDL via `Model::PrintMDL`, re-parses the MDL via
  `VensimParse`, and runs `ModelComparator::Compare` between the two Models.
  The fishbanks and reliability runs depend on Phase 7's view-geometry
  synthesis being in place (the MDL writer requires sketch data).

### AC3.4
- **Literal text:** "The `ModelComparator` (reused from `test/mdl/`) detects a deliberately introduced non-equivalence on the XMILE round-trip; the round-trip test cannot pass vacuously."
- **Phase:** 7.
- **Coverage type:** Automated (negative-control / guard test).
- **Test file + names:** `test/xmile/ViewRoundTripTest.cpp` --
  `View_comparator_guard_detects_corruption`.
- **Notes:** The guard test parses an XMILE input into Model A, regenerates
  XMILE from A and parses Model B, then hand-corrupts B (renames the
  variable `a` to `z`) and asserts `ModelComparator::Compare(A, B)` returns
  a non-empty diff list. If the comparator ever became a no-op, this test
  would fail loudly, defending every other round-trip test against
  vacuous-pass regression.

---

## xmile-reader.AC4: Sketch synthesis from XMILE views

### AC4.1
- **Literal text:** "XMILE `<view>` elements with stocks, flows, auxes, connectors, and aliases produce `VensimView` geometry that the MDL writer's sketch emitter consumes without error; the resulting `.mdl` re-parses with equivalent geometry."
- **Phase:** 7.
- **Coverage type:** Automated (round-trip).
- **Test file + names:** `test/xmile/ViewRoundTripTest.cpp` --
  `View_corpus_fishbanks_to_mdl`, `View_corpus_reliability_to_mdl`. The
  former covers stocks + flows + auxes + connectors; the reliability fixture
  additionally covers `<alias>` elements with `<of>name</of>` children and
  per-alias `uid` attributes.
- **Notes:** The round-trip helper `xmileroundtrip::XmileToMdlDiffs` re-parses
  the generated MDL via `VensimParse`, which exercises the Vensim sketch
  parser; if the MDL is malformed (the writer's emitter rejected the
  XmileView-built geometry), the test surfaces a comparator failure or a
  re-parse error.

### AC4.2
- **Literal text:** "Connector polarity (`positive` -> `+`, `negative` -> `-`, absent -> none) round-trips XMILE -> MDL -> re-parse correctly."
- **Phase:** 7.
- **Coverage type:** Automated (round-trip).
- **Test file + names:** `test/xmile/ViewRoundTripTest.cpp` --
  `View_connector_polarity_plus_round_trips`.
- **Notes / design correction:** The AC's "`positive` -> `+`, `negative` ->
  `-`" wording reflects how the user described the spec, but the actual
  XMILE 1.0 spec attribute value is the literal char `+` or `-`. The
  implementation in `XmileView::ResolveConnectors` reads `polarity="+"` /
  `polarity="-"` and stores the char directly on
  `VensimConnectorElement::_polarity`. As a robustness measure (some Stella
  exports use `"positive"` / `"negative"`), the reader accepts the
  first-char-or-prefix shape via `strncmp(p, "pos", 3)` /
  `strncmp(p, "neg", 3)` -- see Phase 7 Task 3 implementation note. The
  round-trip test uses the canonical `polarity="+"` form.

### AC4.3
- **Literal text:** "A flow whose source or sink is a cloud (implicit, via missing `<from>` / `<to>` stock attribute) sets the connected Variable's `_hasUpstream` / `_hasDownstream` flags so the MDL writer emits the correct flow structure."
- **Phase:** 7.
- **Coverage type:** Automated (round-trip + direct shape check).
- **Test file + names:** `test/xmile/ViewRoundTripTest.cpp` --
  `View_clouds_produce_VensimCommentElement`.
- **Notes / design correction:** The AC's "`_hasUpstream` / `_hasDownstream`
  flags on the connected Variable" mechanism was incorrect per Phase 7's
  codebase investigation. Those flags are set by `is_all_plus_minus` during
  equation-level stock-flow decomposition (`src/Symbol/Expression.cpp`) and
  have no role in sketch geometry. The MDL writer represents clouds as
  `VensimCommentElement` (type 12) nodes in the view's element vector, and
  the XMILE writer recognizes `ElementTypeCOMMENT` at a pipe endpoint as a
  cloud. **The implementation uses `VensimCommentElement`, not the
  upstream/downstream flags.** The intent of AC4.3 -- clouds round-trip
  correctly via the sketch path -- is preserved. The test asserts the view
  contains the expected number of `VensimCommentElement` nodes (one per
  cloud endpoint that didn't match a stock/aux position).

### AC4.4
- **Literal text:** "A model with multiple `<view>` elements takes the first; subsequent views are noted via `errs` as a soft warning and skipped."
- **Phase:** 7.
- **Coverage type:** Automated (soft-warning assertion).
- **Test file + names:** `test/xmile/ViewRoundTripTest.cpp` --
  `View_multi_view_takes_first_warns_second`.
- **Notes:** The test parses a hand-written XMILE with two `<view>` blocks
  and asserts: (a) the resulting Model has exactly one view, (b) `errs`
  contains a string with the substring "multi-view", and (c) parsing
  succeeded overall (the warning does not fail the load). The Phase 7
  `ProcessViews` driver pushes the warning string and increments past
  subsequent views without instantiating an `XmileView` for them.

### AC4.5
- **Literal text:** "XMILE `<group>` elements become `ModelGroup`s; their members are emitted under group banners on the MDL output path."
- **Phase:** 3 (the `ProcessGroup` helper + minimal `ProcessViews` stub that
  walks `<group>` children), 7 (full view walk with view-level `<group>` and
  `<item uid="N"/>` children).
- **Coverage type:** Automated (round-trip).
- **Test file + names:**
  - `test/xmile/AuxRoundTripTest.cpp` -- `AuxRoundTrip_group_round_trip` (the
    `<view><group><var>name</var></group></view>` form via `generateSectorViews`).
  - `test/xmile/ViewRoundTripTest.cpp` -- `View_corpus_reliability_to_mdl`
    covers the corpus's group structure via the full view walk; specifically
    asserts via `ModelComparator` that group membership matches.
- **Notes:** The Phase 7 plan flags a latent risk that Phase 3's
  `AuxRoundTrip_group_round_trip` may surface a comparator diff once Phase 7
  populates the view (because the writer takes a different group-emission
  branch). If that fires during Phase 7 implementation, the fix is either
  fixture-level (add explicit `<view>` positions) or a writer bug log; the
  AC4.5 contract still binds.

---

## xmile-reader.AC5: Out-of-scope features hard-error or are silently skipped

### AC5.1
- **Literal text:** "A `<module>` element inside `<variables>` is rejected with a clear error citing 'modules are not supported' and the offending element."
- **Phase:** 3 (first impl in `ProcessModel`), 8 (negative-path
  rejection test with stricter assertion).
- **Coverage type:** Automated.
- **Test file + names:**
  - `test/xmile/AuxRoundTripTest.cpp` -- `AuxRoundTrip_module_is_rejected`
    (Phase 3).
  - `test/xmile/ErrorRejectTest.cpp` -- `ErrorReject_module_with_clear_message`
    (Phase 8).
- **Notes:** Both tests assert (a) the parse fails (`Model* == nullptr`),
  (b) `errs` is non-empty, and (c) at least one error contains the
  substring `"modules are not supported"`. The Phase 8 version uses the
  shared `CheckRejected` helper so the expected-substring check is uniform
  across the four rejection ACs.

### AC5.2
- **Literal text:** "A `<macro>` sibling element of `<model>` is rejected with a clear error."
- **Phase:** 3 (minimal envelope-level check in `ProcessFile`), 8
  (negative-path test).
- **Coverage type:** Automated.
- **Test file + names:** `test/xmile/ErrorRejectTest.cpp` --
  `ErrorReject_macro_with_clear_message`.
- **Notes:** Asserts the parse fails and `errs` contains the substring
  `"macro"`. Phase 3 sets up the envelope-level rejection so that Phase 4-7
  test fixtures that accidentally include `<macro>` would fail loudly;
  Phase 8 codifies it as a positive test fixture (`kWithMacro`).

### AC5.3
- **Literal text:** "Multiple `<model>` elements (other than `<macro>`-classified ones) is rejected with a clear error."
- **Phase:** 8 (envelope-level check added to `ProcessFile`).
- **Coverage type:** Automated.
- **Test file + names:** `test/xmile/ErrorRejectTest.cpp` --
  `ErrorReject_multiple_models`.
- **Notes:** The implementation counts `<model>` sibling elements (skipping
  foreign-namespace ones) during the root walk in `XmileReader::ProcessFile`
  and rejects when the count exceeds 1 with the message `"multiple <model>
  elements are not supported"`. The test asserts the parse fails and the
  error contains the substring `"multiple <model>"`.

### AC5.4
- **Literal text:** "Unknown-namespace elements (`isee:*`, `simlin:*`) and Stella UI widgets in the XMILE namespace (`<button>`, `<knob>`, `<slider>`, `<graph>`, `<numeric_input>`, ...) are silently skipped; the rest of the model loads."
- **Phase:** 3.
- **Coverage type:** Automated.
- **Test file + names:** `test/xmile/AuxRoundTripTest.cpp` --
  `AuxRoundTrip_foreign_namespace_silently_skipped`.
- **Notes:** The test fixture includes both an `<isee:prefs>` envelope-level
  element and an `<isee:loop_indicator>` inside `<variables>`. The test
  asserts (a) parse succeeds with empty `errs`, (b) the aux survives the
  skip (`m->GetNameSpace()->Find("x") != nullptr`). The Stella UI widget
  filter lives in `XmileReader::IsStellaUIWidget`; the foreign-namespace
  filter lives in `XmileReader::IsForeignNamespace`. Phase 7 extends the
  filter to the view-level walk so widgets embedded in `<view>` are also
  dropped silently.

---

## xmile-reader.AC6: No regression

### AC6.1
- **Literal text:** "The existing `xmutil_test` MDL test suite continues to pass."
- **Phase:** Implicit across all phases; explicitly re-verified in 8.
- **Coverage type:** Automated (the test suite itself).
- **Test file + names:** All pre-existing tests under `test/mdl/`,
  collectively. The pass condition is `out/Debug/xmutil_test` exiting 0 with
  the pre-existing MDL test count intact and zero new failures introduced
  in those files. Phase 8 Task 4 calls this out as a verification step.
- **Notes:** Each phase's "Done when" lists "all Phase 1-N tests still
  pass," which collectively includes the pre-existing MDL tests because the
  same `xmutil_test` binary runs both. The Phase 4 function-registration
  refactor (extracting `RegisterXmutilFunctions` from `VensimParse`) is the
  highest-risk change for AC6.1; the existing MDL tests catch a regression
  there immediately.

### AC6.2
- **Literal text:** "XMILE output produced by the existing `convert_mdl_to_xmile` path is byte-identical to the pre-change output for the existing MDL corpus."
- **Phase:** 8.
- **Coverage type:** Automated (golden file).
- **Test file + names:** `test/xmile/MdlXmileByteIdentityTest.cpp` --
  `MdlXmileByteIdentity_teacup`. Asset:
  `test/xmile/fixtures/teacup.golden.xmile`.
- **Notes:** The golden file is generated once at the start of Phase 8 by
  running the *pre-Phase-8* (or pre-XMILE-reader-work) binary against
  `third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl`, capturing
  the output, and checking it in under `test/xmile/fixtures/`. The test then
  re-runs `convert_mdl_to_xmile` on the same MDL input and `CHECK_EQ_STR`s
  the result against the committed golden. A failing test indicates the
  XMILE reader work unintentionally changed `convert_mdl_to_xmile` (most
  likely via the Phase 4 function-registration refactor); the fix is to
  inspect the diff and either correct the regression or, if the change is
  intentional, update the golden as part of that change's commit.

### AC6.3
- **Literal text:** "The WASM build (configured via `./configure.sh` with the emscripten profile) succeeds with the new sources included."
- **Phase:** 8.
- **Coverage type:** Human verification (operational / CI).
- **Verification procedure:**
  1. Verify emscripten is on PATH (`emcc --version` succeeds).
  2. Run `./configure.sh --use-wasm` from the repo root.
  3. Run `ninja -C out/Debug XMUtil_wasm`.
  4. Verify the output artifacts exist: `ls out/Debug/xmutil.js
     out/Debug/xmutil.wasm`.
  5. Re-run `./configure.sh` to restore the native build profile so further
     development / testing works.
- **Notes / why not automated:** The WASM toolchain (emscripten) is not a
  dependency of the in-process test suite -- the dev machine may not have
  it installed, and `xmutil_test` itself does not build for WASM. The AC
  validates that the gyp `XMUtil_wasm` target consumes the new
  `src/Xmile/*.{cpp}` files (which it does automatically via `<@(common_sources)`
  in `XMUtil.gyp`) and that the WASM linker accepts them. This is a CI /
  operational gate, not an in-process check. The Phase 8 plan documents the
  fallback if emscripten is unavailable on the dev machine: the structural
  participation in `common_sources` is the gyp-level guarantee, and the
  actual link run is delegated to CI.

---

## Coverage summary

| AC      | Phase | Coverage           | Test file / verification                                                                       |
|---------|-------|--------------------|------------------------------------------------------------------------------------------------|
| AC1.1   | 1,3,4,5,6,7,8 | Automated  | `BasicSmokeTest.cpp`, `AuxRoundTripTest.cpp`, `StockFlowRoundTripTest.cpp`, `CorpusRoundTripTest.cpp` |
| AC1.2   | 8     | Automated          | `CEntryTest.cpp`                                                                               |
| AC1.3   | 8     | Automated          | `CEntryTest.cpp`                                                                               |
| AC1.4   | 1,8   | Automated          | `BasicSmokeTest.cpp` + `test/cli_xmile_roundtrip.sh`                                           |
| AC1.5   | 1,8   | Automated          | `BasicSmokeTest.cpp`, `ErrorRejectTest.cpp`                                                    |
| AC1.6   | 1,8   | Automated (E2E)    | `test/cli_xmile_roundtrip.sh`                                                                  |
| AC2.1   | 2     | Automated (unit)   | `EquationParseTest.cpp`                                                                        |
| AC2.2   | 2     | Automated (unit)   | `XmileFunctionsTest.cpp`, `EquationParseTest.cpp`                                              |
| AC2.3   | 2     | Automated (unit)   | `XmileFunctionsTest.cpp`, `EquationParseTest.cpp` -- corrected: `ExpressionVariable` (not Function) |
| AC2.4   | 2     | Automated (unit)   | `EquationParseTest.cpp`                                                                        |
| AC2.5   | 2,5   | Automated          | `EquationParseTest.cpp`, `ArrayRoundTripTest.cpp`                                              |
| AC2.6   | 2     | Automated (unit)   | `EquationParseTest.cpp`                                                                        |
| AC2.7   | 2     | Automated (unit)   | `EquationParseTest.cpp`                                                                        |
| AC2.8   | 2     | Automated (unit)   | `EquationParseTest.cpp` -- two-step rule (table miss + fallback miss)                          |
| AC2.9   | 2,8   | Automated          | `EquationParseTest.cpp`, `ErrorRejectTest.cpp`                                                 |
| AC3.1   | 3,4,5,6,7,8 | Automated    | `AuxRoundTripTest.cpp`, `StockFlowRoundTripTest.cpp`, `ArrayRoundTripTest.cpp`, `LookupRoundTripTest.cpp`, `CorpusRoundTripTest.cpp` |
| AC3.2   | 3,5   | Automated          | `AuxRoundTripTest.cpp`, `ArrayRoundTripTest.cpp`                                               |
| AC3.3   | 3,4,5,7,8 | Automated      | `AuxRoundTripTest.cpp`, `StockFlowRoundTripTest.cpp`, `ArrayRoundTripTest.cpp`, `ViewRoundTripTest.cpp`, `CorpusRoundTripTest.cpp` |
| AC3.4   | 7     | Automated (guard)  | `ViewRoundTripTest.cpp` -- `View_comparator_guard_detects_corruption`                          |
| AC4.1   | 7     | Automated          | `ViewRoundTripTest.cpp`                                                                        |
| AC4.2   | 7     | Automated          | `ViewRoundTripTest.cpp` -- polarity is single char (corrected)                                 |
| AC4.3   | 7     | Automated          | `ViewRoundTripTest.cpp` -- clouds are `VensimCommentElement` (corrected mechanism)             |
| AC4.4   | 7     | Automated (warn)   | `ViewRoundTripTest.cpp` -- `View_multi_view_takes_first_warns_second` asserts errs contains "multi-view" |
| AC4.5   | 3,7   | Automated          | `AuxRoundTripTest.cpp`, `ViewRoundTripTest.cpp`                                                |
| AC5.1   | 3,8   | Automated          | `AuxRoundTripTest.cpp`, `ErrorRejectTest.cpp`                                                  |
| AC5.2   | 3,8   | Automated          | `ErrorRejectTest.cpp`                                                                          |
| AC5.3   | 8     | Automated          | `ErrorRejectTest.cpp`                                                                          |
| AC5.4   | 3     | Automated          | `AuxRoundTripTest.cpp` -- `AuxRoundTrip_foreign_namespace_silently_skipped`                    |
| AC6.1   | 8     | Automated          | All pre-existing `test/mdl/` tests, run via `xmutil_test`                                      |
| AC6.2   | 8     | Automated (golden) | `MdlXmileByteIdentityTest.cpp` + `test/xmile/fixtures/teacup.golden.xmile`                     |
| AC6.3   | 8     | Human (CI/op)      | `./configure.sh --use-wasm && ninja -C out/Debug XMUtil_wasm` -- no in-process test            |

Every acceptance criterion from `xmile-reader.AC1.1` through
`xmile-reader.AC6.3` is mapped above. AC6.3 is the only criterion that is
not covered by an in-process automated test; its verification is delegated to
CI / a documented manual procedure for the reason stated in its row.
