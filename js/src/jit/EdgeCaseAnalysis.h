/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_EdgeCaseAnalysis_h
#define jit_EdgeCaseAnalysis_h

namespace js {
namespace jit {

class MIRGenerator;
class MIRGraph;

class EdgeCaseAnalysis {
  MIRGenerator* mir;
  MIRGraph& graph;

 public:
  EdgeCaseAnalysis(MIRGenerator* mir, MIRGraph& graph);
  [[nodiscard]] bool analyzeLate();
};

}  // namespace jit
}  // namespace js

#endif /* jit_EdgeCaseAnalysis_h */
