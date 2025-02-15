/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_FoldConstants_h
#define frontend_FoldConstants_h

#include "frontend/SyntaxParseHandler.h"

namespace js {
namespace frontend {

class FullParseHandler;
template <class ParseHandler>
class PerHandlerParser;
class ParserAtomsTable;

// Perform constant folding on the given AST. For example, the program
// `print(2 + 2)` would become `print(4)`.
//
// pnp is the address of a pointer variable that points to the root node of the
// AST. On success, *pnp points to the root node of the new tree, which may be
// the same node (unchanged or modified in place) or a new node.
//
// Usage:
//    pn = parser->statement();
//    if (!pn) {
//        return false;
//    }
//    if (!FoldConstants(cx, parserAtoms, &pn, parser)) {
//        return false;
//    }
[[nodiscard]] extern bool FoldConstants(JSContext* cx,
                                        ParserAtomsTable& parserAtoms,
                                        ParseNode** pnp,
                                        FullParseHandler* handler);

[[nodiscard]] inline bool FoldConstants(JSContext* cx,
                                        ParserAtomsTable& parserAtoms,
                                        typename SyntaxParseHandler::Node* pnp,
                                        SyntaxParseHandler* handler) {
  return true;
}

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_FoldConstants_h */
