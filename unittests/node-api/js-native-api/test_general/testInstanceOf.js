'use strict';
const common = require('../../common');
const assert = require('assert');

// Addon is referenced through the eval expression in testFile
const addon = require(`./build/${common.buildType}/test_general`);

// The following assert functions are referenced by v8's unit tests
// See for instance deps/v8/test/mjsunit/instanceof.js
// eslint-disable-next-line no-unused-vars
function assertTrue(assertion) {
  return assert.strictEqual(assertion, true);
}

// eslint-disable-next-line no-unused-vars
function assertFalse(assertion) {
  assert.strictEqual(assertion, false);
}

// eslint-disable-next-line no-unused-vars
function assertEquals(leftHandSide, rightHandSide) {
  assert.strictEqual(leftHandSide, rightHandSide);
}

// eslint-disable-next-line no-unused-vars
function assertThrows(statement) {
  assert.throws(function() {
    addon.testNapiRun(statement);
  }, Error);
}

function testFile(contents) {
  try {
    // Function(contents)();
    addon.testNapiRun(contents);
  } catch (err) {
    // This test depends on V8 test files, which may not exist in downloaded
    // archives. Emit a warning if the tests cannot be found instead of failing.
    throw err;
  }
}
const FileContent = 
    `print("instanceof");
    //CHECK-LABEL: instanceof

    try {
        1 instanceof 2;
    } catch (e) {
        print("caught", e.name, e.message);
    }
    //CHECK-NEXT: caught TypeError right operand of 'instanceof' is not an object

    function foo () {}
    foo.prototype = 1;

    print(1 instanceof foo);
    //CHECK-NEXT: false

    try {
        ({}) instanceof foo;
    } catch (e) {
        print("caught", e.name, e.message);
    }
    //CHECK-NEXT: caught TypeError function's '.prototype' is not an object in 'instanceof'

    function BaseObj() {}

    print({} instanceof BaseObj);
    //CHECK-NEXT: false

    print( (new BaseObj()) instanceof BaseObj);
    //CHECK-NEXT: true

    function ChildObj() {}
    ChildObj.prototype = Object.create(BaseObj.prototype);

    print( (new ChildObj()) instanceof BaseObj);
    //CHECK-NEXT: true
    print( (new ChildObj()) instanceof ChildObj);
    //CHECK-NEXT: true

    var BoundBase = BaseObj.bind(1,2).bind(3,4);
    var BoundChild = ChildObj.bind(1,2).bind(3,4);

    print( (new ChildObj()) instanceof BoundBase);
    //CHECK-NEXT: true
    print( (new ChildObj()) instanceof BoundChild);
    //CHECK-NEXT: true

    var a = new Proxy({}, {});
    var b = Object.create(a);
    var c = Object.create(b);
    a.__proto__ = c;
    try {
        b instanceof Date;
    } catch (e) {
        print("caught", e.name, e.message);
    }
    //CHECK-NEXT: caught RangeError Maximum prototype chain length exceeded

    function A(){}
    Object.defineProperty(A, Symbol.hasInstance, {value: function(){return true;}})
    print(undefined instanceof A);
    //CHECK-NEXT: true`;

testFile(FileContent);

// We can only perform this test if we have a working Symbol.hasInstance
if (typeof Symbol !== 'undefined' && 'hasInstance' in Symbol &&
    typeof Symbol.hasInstance === 'symbol') {

  function compareToNative(theObject, theConstructor) {
    assert.strictEqual(
      addon.doInstanceOf(theObject, theConstructor),
      (theObject instanceof theConstructor)
    );
  }

  function MyClass() {}
  Object.defineProperty(MyClass, Symbol.hasInstance, {
    value: function(candidate) {
      return 'mark' in candidate;
    }
  });

  function MySubClass() {}
  MySubClass.prototype = new MyClass();

  let x = new MySubClass();
  let y = new MySubClass();
  x.mark = true;

  compareToNative(x, MySubClass);
  compareToNative(y, MySubClass);
  compareToNative(x, MyClass);
  compareToNative(y, MyClass);

  x = new MyClass();
  y = new MyClass();
  x.mark = true;

  compareToNative(x, MySubClass);
  compareToNative(y, MySubClass);
  compareToNative(x, MyClass);
  compareToNative(y, MyClass);
}
