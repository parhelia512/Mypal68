// |reftest| skip-if(!this.hasOwnProperty('AsyncIterator'))

class TestError extends Error {}
class TestIterator extends AsyncIterator {
  next() {
    return Promise.resolve({
      done: false,
      get value() {
        throw new TestError();
      }
    });
  }

  closed = false;
  return() {
    closed = true;
  }
}

const iterator = new TestIterator();
assertEq(iterator.closed, false, 'iterator starts unclosed');
iterator.some(x => x).then(() => assertEq(true, false, 'expected error'), err => {
  assertEq(err instanceof TestError, true);
  assertEq(iterator.closed, false, 'iterator remains unclosed');
});

if (typeof reportCompare === 'function')
  reportCompare(0, 0);
