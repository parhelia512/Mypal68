// |reftest| error:SyntaxError
// This file was procedurally generated from the following sources:
// - src/assignment-target-type/equalityexpression-abstract-equal-relationalexpression-0.case
// - src/assignment-target-type/invalid/direct.template
/*---
description: Static Semantics AssignmentTargetType, Return invalid. (Direct assignment)
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Direct assignment

    EqualityExpression == RelationalExpression
    Static Semantics AssignmentTargetType, Return invalid.

---*/

$DONOTEVALUATE();

function _() {
  x == y = 1;
}
