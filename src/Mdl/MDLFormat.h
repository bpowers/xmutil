#pragma once
#ifndef __MDLFORMAT_H
#define __MDLFORMAT_H

#include <string>
#include <vector>

class ExpressionTable;

// Free functions for formatting identifiers and numbers as Vensim .mdl text.
// Ported from the simlin MDL writer (src/simlin-engine/src/mdl/writer.rs).
namespace mdl {

// True if the (already-dequoted) name must be wrapped in double quotes for
// Vensim. Interior spaces are allowed unquoted; quoting is required for an empty
// name, a leading or trailing space, a leading character outside [A-Za-z_], or
// any interior character outside [A-Za-z0-9_ ].
bool NeedsMDLQuoting(const std::string &name);

// Escape interior quotes, backslashes, and newlines for a quoted Vensim
// identifier. A real newline becomes the two-character escape \n; an existing
// \n escape (backslash followed by 'n') is preserved rather than double-escaped.
std::string EscapeMDLQuotedIdent(const std::string &name);

// Strip an optional single pair of surrounding quotes, then quote and escape iff
// needed. The single entry point the walker uses for every emitted identifier;
// idempotent on already-quoted input.
std::string FormatMDLIdent(const std::string &rawName);

// Vensim number text: ":NA:" for the -1e38 sentinel, an integer form for whole
// values, otherwise the shortest decimal that round-trips back to the same
// double.
std::string FormatMDLNumber(double v);

// Split an .mdl equation string into tokens at natural line-break boundaries:
// after a comma (the trailing space stays with the comma token), around each
// bracket/paren, and before a binary +-*/^ operator. A leading or post-operator
// +/- is treated as a unary sign and kept attached. Quoted '...' literals and
// "..." identifiers are atomic. Concatenating the returned tokens reproduces the
// input exactly.
std::vector<std::string> TokenizeForWrapping(const std::string &eqn);

// Wrap a long equation with Vensim "\<newline><two tabs>" continuations so no
// physical line exceeds maxLineLen (call with 80). An equation already within
// the budget is returned unchanged. Breaks happen between tokens, trimming a
// trailing space before the break.
std::string WrapEquation(const std::string &eqn, size_t maxLineLen);

// Render a lookup/graphical-function table as the Vensim body
// "[(xmin,ymin)-(xmax,ymax)],(x0,y0),(x1,y1),...". The range box is computed
// from the min/max of the stored x/y vectors because xmutil's parsed range
// (ExpressionTable::AddRange) is unreliable. With no points the box collapses to
// "[(0,0)-(0,0)]". The extrapolation flag is intentionally not emitted (v1
// fidelity gap; the comparator ignores it).
std::string WriteLookupBody(ExpressionTable *table);

}  // namespace mdl

#endif  // __MDLFORMAT_H
