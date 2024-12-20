// DO NOT EDIT! This test has been generated by /offscreen-canvas/tools/gentest.py.
// OffscreenCanvas test in a worker:2d.path.arc.negative
// Description:arc() with negative radius throws INDEX_SIZE_ERR
// Note:

importScripts("/resources/testharness.js");
importScripts("/2dcontext/resources/canvas-tests.js");

var t = async_test("arc() with negative radius throws INDEX_SIZE_ERR");
var t_pass = t.done.bind(t);
var t_fail = t.step_func(function(reason) {
    throw reason;
});
t.step(function() {

var offscreenCanvas = new OffscreenCanvas(100, 50);
var ctx = offscreenCanvas.getContext('2d');

assert_throws("INDEX_SIZE_ERR", function() { ctx.arc(0, 0, -1, 0, 0, true); });
t.done();

});
done();
