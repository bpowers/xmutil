# Human Test Plan: Vensim .mdl Writer

This plan covers what the automated suite cannot. The automated bar uses xmutil's
own parser as the round-trip oracle and is structural/semantic, not byte-identical;
it verifies that a regenerated model re-parses to an equivalent `Model`. The human
plan covers the things that oracle cannot judge: that the emitted bytes load in
*real Vensim*, that the diagram renders correctly to a human eye, that cosmetic
sketch defaults (fonts/colors/line styles the comparator ignores) are acceptable,
and that large real-world models survive the round-trip.

## Prerequisites

- A build of the writer: `./configure.sh` then `ninja -C out/Debug XMUtil xmutil_test`.
  Binary at `out/Debug/XMUtil`.
- A green automated baseline (must hold before any manual check is meaningful):
  - `out/Debug/xmutil_test` -> `89 tests, 0 failed`, exit 0.
  - `bash test/cli_roundtrip.sh out/Debug/XMUtil` -> `CLI round-trip OK`, exit 0.
- A licensed copy of **Vensim** (any edition that opens `.mdl`: Model Reader, PLE,
  or DSS) on a machine you can copy files to. This is the only manual dependency
  and is unavailable to CI.
- Source fixtures present at
  `third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl` and
  `third_party/simlin/test/test-models/samples/SIR/SIR.mdl`.

## Phase 1: Generate the artifacts to inspect (run from repo root)

| Step | Action | Expected |
|------|--------|----------|
| 1 | `out/Debug/XMUtil --to-mdl third_party/simlin/src/pysimlin/tests/fixtures/teacup.mdl` | Exit 0; creates `teacup.regen.mdl` (clobber guard appends `.regen` because input already ends `.mdl`). |
| 2 | `cp third_party/simlin/test/test-models/samples/SIR/SIR.mdl /tmp/SIR.mdl && out/Debug/XMUtil --to-mdl /tmp/SIR.mdl` | Exit 0; creates `/tmp/SIR.regen.mdl`, first line `{UTF-8}`. |
| 3 | `head -1 .../teacup.regen.mdl` | Prints `{UTF-8}` (with a trailing CR -- file is CRLF). |
| 4 | Open both `*.regen.mdl` in a plain text editor | Human-readable Vensim equations, a `\\\---///` sketch block, and a trailing `:L...` settings section are present. |

## Phase 2: Real-Vensim load (the core human gate)

| Step | Action | Expected |
|------|--------|----------|
| 1 | Copy `teacup.regen.mdl` to the Vensim machine; File -> Open Model | Opens with **no error/warning dialog**; no "file is damaged"/recovery prompt. |
| 2 | Open `SIR.regen.mdl` likewise | Opens clean. |
| 3 | Run a simulation on each (Model -> Simulate) | Completes without an equation/units error; output finite (teacup temperature decays toward room temperature; SIR Infectious rises then falls). |
| 4 | Open the *original* and regenerated model side by side; compare the Equations view | Same variables, same equations semantically (parenthesization/whitespace may differ -- expected, out of scope). |

## Phase 3: Visual sketch fidelity (what the comparator cannot judge)

| Step | Action | Expected |
|------|--------|----------|
| 1 | View the teacup diagram in Vensim | One stock box (Teacup Temperature) with an attached flow valve and a cloud; auxiliaries positioned roughly as the original; connectors present. |
| 2 | View the SIR diagram | Stocks with attached flow valves and clouds; ghost/alias boxes render *as aliases* (greyed duplicates), not as new primary definitions. |
| 3 | Inspect causal-link polarity arrows on SIR | `+` and `-` polarities appear on the same links as the original (confirms POL fields also *display* correctly). |
| 4 | Eyeball cosmetic defaults (fonts, box colors, line styles) | Legible and non-broken. These are writer-emitted Vensim defaults the comparator intentionally ignores; confirm they don't produce invisible/overlapping/garbled elements. |

## End-to-End: XMILE/Dynamo -> Vensim cross-format (load in real Vensim)

Purpose: validate the writer on input that did **not** originate as Vensim, where
there is no original `.mdl` to diff against and the in-process oracle is weakest.

1. Pick an `.xmile` or a classic `.dyn` model (e.g. author one like
   `test/mdl/fixtures/minimal.dyn`, or any XMILE you have).
2. `out/Debug/XMUtil --to-mdl path/to/model.xmile` -> writes `path/to/model.mdl`
   (non-`.mdl` input, so the extension is replaced, no `.regen`).
3. Open the resulting `.mdl` in Vensim -> loads clean, simulates, and the
   stocks/flows/auxes match the source model's structure. A Dynamo source has no
   sketch; expect Vensim to auto-layout or show an empty diagram (acceptable --
   AC4.2/AC4.3: no fabricated geometry).

## Large / real-world model breadth (beyond the curated corpus)

Purpose: the automated corpus is curated single-feature models plus teacup/SIR;
exercise a big, messy, real model.

1. Take a large production `.mdl` (hundreds of variables, multiple sketch views,
   macros, lookups).
2. `out/Debug/XMUtil --to-mdl big.mdl` -> writes `big.regen.mdl`; confirm exit 0
   and `{UTF-8}` first line.
3. Open `big.regen.mdl` in Vensim -> loads without error, **all views** present
   and navigable, simulation completes.
4. Re-feed it:
   `out/Debug/XMUtil --to-mdl --stdio < big.regen.mdl > big.regen2.mdl` and compare
   -- `big.regen.mdl` vs `big.regen2.mdl` should be byte-identical (the writer is a
   fixpoint on its own output; `WriterBug_multiline_comment_emit_is_fixpoint`
   asserts this on a small case -- confirm it holds at scale).

## Human Verification Required (from test-requirements.md)

| Criterion | Why Manual | Steps |
|-----------|------------|-------|
| Real-Vensim load and render spot-check (once per release, not a CI gate) | Vensim is a proprietary GUI unavailable to CI; cosmetic sketch defaults are validated only by Vensim's own renderer, which the comparator deliberately does not check. | Phase 2 (load + simulate) and Phase 3 (visual sketch fidelity) on `teacup.regen.mdl` and `SIR.regen.mdl`. |

## Notes on documented deviations / v1 scope

- **AC1.4 (generation error -> NULL)** is unsatisfiable as written: xmutil's Vensim
  reader is recovery-oriented (`VensimParse::ProcessFile` never returns false) and
  `PrintMDL` never populates `errs`, so garbage input yields a non-null degenerate
  `{UTF-8}` shell rather than NULL. The C-entry tests pin the real contract (NULL iff
  parse-fails or `errs` populated) so a future change that begins reporting errors
  would visibly flip the behavior. No manual analogue.
- **v1 scope exclusions** (documented, not gaps): the `:EXCEPT:` subscript-exception
  clause is neither emitted nor compared (such models are excluded from the corpus);
  the lookup extrapolation flag is not emitted; multi-row `;` number tables are lost
  at parse time (pre-existing parser limitation).
