if (!wasmIsSupported())
    quit();

load(libdir + "asserts.js");

// On 64-bit systems with explicit bounds checking, ion and baseline can handle
// 65536 pages but cranelift can handle only 65534 pages; thus the presence of
// cranelift forces the max for the system as a whole to 65534.  We will
// probably fix this eventually.

var PageSizeInBytes = 65536;
var MaxBytesIn32BitMemory = 0;
if (largeArrayBufferEnabled()) {
    if (wasmCompilersPresent().indexOf("cranelift") != -1) {
        MaxBytesIn32BitMemory = 65534*PageSizeInBytes;
    } else {
        MaxBytesIn32BitMemory = 65536*PageSizeInBytes;
    }
} else {
    // This is an overestimate twice: first, the max byte value is divisible by
    // the page size; second, it must be a valid bounds checking immediate.  But
    // INT32_MAX is fine for testing.
    MaxBytesIn32BitMemory = 0x7FFF_FFFF;
}
var MaxPagesIn32BitMemory = Math.floor(MaxBytesIn32BitMemory / PageSizeInBytes);

// "options" is an extension to facilitate the SIMD wormhole

function wasmEvalText(str, imports, options) {
    let binary = wasmTextToBinary(str);
    let valid = WebAssembly.validate(binary, options);

    let m;
    try {
        m = new WebAssembly.Module(binary, options);
        assertEq(valid, true);
    } catch(e) {
        if (!e.toString().match(/out of memory/))
            assertEq(valid, false);
        throw e;
    }

    return new WebAssembly.Instance(m, imports);
}

function wasmValidateText(str) {
    let binary = wasmTextToBinary(str);
    let valid = WebAssembly.validate(binary);
    if (!valid) {
        new WebAssembly.Module(binary);
    }
    assertEq(valid, true);
}

function wasmFailValidateText(str, pattern) {
    let binary = wasmTextToBinary(str);
    assertEq(WebAssembly.validate(binary), false);
    assertErrorMessage(() => new WebAssembly.Module(binary), WebAssembly.CompileError, pattern);
}

// Expected compilation failure can happen in a couple of ways:
//
// - The compiler can be available but not capable of recognizing some opcodes:
//   Compilation will start, but will fail with a CompileError.  This is what
//   happens without --wasm-gc if opcodes enabled by --wasm-gc are used.
//
// - The compiler can be unavailable: Compilation will not start at all but will
//   throw an Error.  This is what happens with "--wasm-gc --wasm-compiler=X" if
//   X does not support the features enabled by --wasm-gc.

function wasmCompilationShouldFail(bin, compile_error_regex) {
    try {
        new WebAssembly.Module(bin);
    } catch (e) {
        if (e instanceof WebAssembly.CompileError) {
            assertEq(compile_error_regex.test(e), true);
        } else if (e instanceof Error) {
            assertEq(/can't use wasm debug\/gc without baseline/.test(e), true);
        } else {
            throw new Error("Unexpected exception value:\n" + e);
        }
    }
}

function mismatchError(actual, expect) {
    var str = `type mismatch: expression has type ${actual} but expected ${expect}`;
    return RegExp(str);
}

const emptyStackError = /from empty stack/;
const unusedValuesError = /unused values not explicitly dropped by end of block/;

function jsify(wasmVal) {
    if (wasmVal === 'nan')
        return NaN;
    if (wasmVal === 'inf')
        return Infinity;
    if (wasmVal === '-inf')
        return Infinity;
    if (wasmVal === '-0')
        return -0;
    return wasmVal;
}

function _augmentSrc(src, assertions) {
    let i = 0;
    let newSrc = src.substr(0, src.lastIndexOf(')'));
    for (let { func, args, expected, type } of assertions) {
        newSrc += `
        (func (export "assert_${i++}") (result i32)
         ${ args ? args.join('\n') : '' }
         call ${func}`;

        if (typeof expected !== 'undefined') {
            switch (type) {
                case 'f32':
                    newSrc += `
         i32.reinterpret/f32
         i32.const ${expected}
         i32.eq`;
                    break;
                case 'f64':
                    newSrc += `
         i64.reinterpret/f64
         i64.const ${expected}
         i64.eq`;
                    break;
                case 'i32':
                    newSrc += `
         i32.const ${expected}
         i32.eq`;
                    break;
                case 'i64':
                    newSrc += `
         i64.const ${expected}
         i64.eq`;
                    break;
                case 'v128':
                    newSrc += `
         v128.const ${expected}
         i8x16.eq
         i8x16.all_true`;
                    break;
                default:
                    throw new Error("unexpected usage of wasmAssert");
            }
        } else {
            // Always true when there's no expected return value.
            newSrc += "\ni32.const 1";
        }

        newSrc += ')\n';
    }
    newSrc += ')';
    return newSrc;
}

function wasmAssert(src, assertions, maybeImports = {}, exportBox = null) {
    let { exports } = wasmEvalText(_augmentSrc(src, assertions), maybeImports);
    if (exportBox !== null)
        exportBox.exports = exports;
    for (let i = 0; i < assertions.length; i++) {
        let { func, expected, params } = assertions[i];
        let paramText = params ? params.join(', ') : '';
        assertEq(exports[`assert_${i}`](), 1,
                 `Unexpected value when running ${func}(${paramText}), expecting ${expected}.`);
    }
}

// Fully test a module:
// - ensure it validates.
// - ensure it compiles and produces the expected result.
// - ensure textToBinary(binaryToText(binary)) = binary
// Preconditions:
// - the binary module must export a function called "run".
function wasmFullPass(text, expected, maybeImports, ...args) {
    let binary = wasmTextToBinary(text);
    assertEq(WebAssembly.validate(binary), true, "Must validate.");

    let module = new WebAssembly.Module(binary);
    let instance = new WebAssembly.Instance(module, maybeImports);
    assertEq(typeof instance.exports.run, 'function', "A 'run' function must be exported.");
    assertEq(instance.exports.run(...args), expected, "Initial module must return the expected result.");
}

// Ditto, but expects a function named '$run' instead of exported with this name.
function wasmFullPassI64(text, expected, maybeImports, ...args) {
    let binary = wasmTextToBinary(text);
    assertEq(WebAssembly.validate(binary), true, "Must validate.");

    let augmentedSrc = _augmentSrc(text, [ { type: 'i64', func: '$run', args, expected } ]);
    let augmentedBinary = wasmTextToBinary(augmentedSrc);
    new WebAssembly.Instance(new WebAssembly.Module(augmentedBinary), maybeImports).exports.assert_0();
}

function wasmRunWithDebugger(wast, lib, init, done) {
    let g = newGlobal({newCompartment: true});
    let dbg = new Debugger(g);

    g.eval(`
var wasm = wasmTextToBinary('${wast}');
var lib = ${lib || 'undefined'};
var m = new WebAssembly.Instance(new WebAssembly.Module(wasm), lib);`);

    var wasmScript = dbg.findScripts().filter(s => s.format == 'wasm')[0];

    init({dbg, wasmScript, g,});
    let result = undefined, error = undefined;
    try {
        result = g.eval("m.exports.test()");
    } catch (ex) {
        error = ex;
    }
    done({dbg, result, error, wasmScript, g,});
}

const WasmHelpers = {};

(function() {
    let enabled = false;
    try {
        enableSingleStepProfiling();
        disableSingleStepProfiling();
        enabled = true;
    } catch (e) {}
    WasmHelpers.isSingleStepProfilingEnabled = enabled;
})();

WasmHelpers._normalizeStack = (stack, preciseStacks) => {
    var wasmFrameTypes = [
        {re:/^jit call to int64 wasm function$/,                          sub:"i64>"},
        {re:/^out-of-line coercion for jit entry arguments \(in wasm\)$/, sub:"ool>"},
        {re:/^wasm-function\[(\d+)\] \(.*\)$/,                            sub:"$1"},
        {re:/^(fast|slow) exit trampoline (to native )?\(in wasm\)$/,     sub:"<"},
        {re:/^call to[ asm.js]? native (.*) \(in wasm\)$/,                sub:"$1"},
        {re:/ \(in wasm\)$/,                                              sub:""}
    ];

    let entryRegexps;
    if (preciseStacks) {
        entryRegexps = [
            {re:/^slow entry trampoline \(in wasm\)$/,                    sub:"!>"},
            {re:/^fast entry trampoline \(in wasm\)$/,                    sub:">"},
        ];
    } else {
        entryRegexps = [
            {re:/^(fast|slow) entry trampoline \(in wasm\)$/,             sub:">"}
        ];
    }
    wasmFrameTypes = entryRegexps.concat(wasmFrameTypes);

    var framesIn = stack.split(',');
    var framesOut = [];
    for (let frame of framesIn) {
        for (let {re, sub} of wasmFrameTypes) {
            if (re.test(frame)) {
                framesOut.push(frame.replace(re, sub));
                break;
            }
        }
    }

    return framesOut.join(',');
};

WasmHelpers._removeAdjacentDuplicates = array => {
    if (array.length < 2)
        return;
    let i = 0;
    for (let j = 1; j < array.length; j++) {
        if (array[i] !== array[j])
            array[++i] = array[j];
    }
    array.length = i + 1;
}

WasmHelpers.normalizeStacks = (stacks, preciseStacks = false) => {
    let observed = [];
    for (let i = 0; i < stacks.length; i++)
        observed[i] = WasmHelpers._normalizeStack(stacks[i], preciseStacks);
    WasmHelpers._removeAdjacentDuplicates(observed);
    return observed;
};

WasmHelpers._compareStacks = (got, expect) => {
    if (got.length != expect.length) {
        return false;
    }
    for (let i = 0; i < got.length; i++) {
        if (got[i] !== expect[i])
            return false;
    }
    return true;
}

WasmHelpers.assertEqImpreciseStacks = (got, expect) => {
    let observed = WasmHelpers.normalizeStacks(got, /* precise */ false);
    let same = WasmHelpers._compareStacks(observed, expect);
    if (!same) {
        if (observed.length != expect.length) {
            print(`Got:\n${observed.toSource()}\nExpect:\n${expect.toSource()}`);
            assertEq(observed.length, expect.length);
        }
        for (let i = 0; i < observed.length; i++) {
            if (observed[i] !== expect[i]) {
                print(`On stack ${i}, Got:\n${observed[i]}\nExpect:\n${expect[i]}`);
                assertEq(observed[i], expect[i]);
            }
        }
    }
}

WasmHelpers.extractStackFrameFunction = (frameString) => {
    var [_, name, filename, line, column] = frameString.match(/^(.*)@(.*):(.*):(.*)$/);
    if (name)
        return name;
    if (/wasm-function/.test(line))
        return line;
    return "";
};

WasmHelpers.assertStackTrace = (exception, expected) => {
    let callsites = exception.stack.trim().split('\n').map(WasmHelpers.extractStackFrameFunction);
    assertEq(callsites.length, expected.length);
    for (let i = 0; i < callsites.length; i++) {
        assertEq(callsites[i], expected[i]);
    }
};

WasmHelpers.nextLineNumber = (n=1) => {
    return +(new Error().stack).split('\n')[1].split(':')[1] + n;
}

WasmHelpers.startProfiling = () => {
    if (!WasmHelpers.isSingleStepProfilingEnabled)
        return;
    enableSingleStepProfiling();
}

WasmHelpers.endProfiling = () => {
    if (!WasmHelpers.isSingleStepProfilingEnabled)
        return;
    return disableSingleStepProfiling();
}

WasmHelpers.assertEqPreciseStacks = (observed, expectedStacks) => {
    if (!WasmHelpers.isSingleStepProfilingEnabled)
        return null;

    observed = WasmHelpers.normalizeStacks(observed, /* precise */ true);

    for (let i = 0; i < expectedStacks.length; i++) {
        if (WasmHelpers._compareStacks(observed, expectedStacks[i]))
            return i;
    }

    throw new Error(`no plausible stacks found, observed: ${observed.join('/')}
Expected one of:
${expectedStacks.map(stacks => stacks.join("/")).join('\n')}`);
}

function fuzzingSafe() {
    return typeof getErrorNotes == 'undefined';
}

// Common instantiations of wasm values for dynamic type check testing

let WasmFuncrefValues = [
    wasmEvalText(`(module (func (export "")))`).exports[''],
];
let WasmNonNullEqrefValues = [];
let WasmEqrefValues = [];
if (wasmGcEnabled()) {
    let { newStruct } = wasmEvalText(`
      (module
        (type $s (struct))
        (func (export "newStruct") (result eqref)
            rtt.canon $s
            struct.new_with_rtt $s)
      )`).exports;
    WasmNonNullEqrefValues.push(newStruct());
    WasmEqrefValues.push(null, ...WasmNonNullEqrefValues);
}
let WasmNonEqrefValues = [
    undefined,
    true,
    false,
    {x:1337},
    ["abracadabra"],
    1337,
    13.37,
    "hi",
    37n,
    new Number(42),
    new Boolean(true),
    Symbol("status"),
    () => 1337,
    ...WasmFuncrefValues,
];
let WasmNonNullExternrefValues = [
    ...WasmNonEqrefValues,
    ...WasmNonNullEqrefValues
];
let WasmExternrefValues = [null, ...WasmNonNullExternrefValues];
