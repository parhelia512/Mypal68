// |reftest| error:SyntaxError
// This file was procedurally generated from the following sources:
// - src/assignment-target-type/primaryexpression-literal-numeric.case
// - src/assignment-target-type/invalid/direct.template
/*---
description: PrimaryExpression Literal NumericLiteral; Return invalid. (Direct assignment)
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Direct assignment
---*/

$DONOTEVALUATE();

function _() {
  0 = 1;
}
