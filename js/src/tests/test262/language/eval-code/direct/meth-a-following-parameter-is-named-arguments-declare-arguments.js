// This file was procedurally generated from the following sources:
// - src/direct-eval-code/declare-arguments.case
// - src/direct-eval-code/default/meth/a-following-parameter-is-named-arguments.template
/*---
description: Declare "arguments" and assign to it in direct eval code (Declare |arguments| when a following parameter is named |arguments|.)
esid: sec-evaldeclarationinstantiation
flags: [generated, noStrict]
---*/



assert.sameValue("arguments" in this, false, "No global 'arguments' binding");

let o = { f(p = eval("var arguments"), arguments) {

}};
assert.throws(SyntaxError, o.f);

assert.sameValue("arguments" in this, false, "No global 'arguments' binding");

reportCompare(0, 0);
