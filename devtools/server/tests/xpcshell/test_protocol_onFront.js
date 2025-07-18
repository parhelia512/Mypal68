/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test Front.onFront method.
 */

const protocol = require("devtools/shared/protocol");
const { RetVal } = protocol;

const childSpec = protocol.generateActorSpec({
  typeName: "childActor",
});

const ChildActor = protocol.ActorClassWithSpec(childSpec, {
  initialize(conn, id) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.childID = id;
  },

  form: function() {
    return {
      actor: this.actorID,
      childID: this.childID,
      foo: "bar",
    };
  },
});

const rootSpec = protocol.generateActorSpec({
  typeName: "root",

  methods: {
    createChild: {
      request: {},
      response: { actor: RetVal("childActor") },
    },
  },
});

const RootActor = protocol.ActorClassWithSpec(rootSpec, {
  typeName: "root",

  initialize: function(conn) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this.actorID = "root";

    // Root actor owns itself.
    this.manage(this);

    this.sequence = 0;
  },

  sayHello() {
    return {
      from: "root",
      applicationType: "xpcshell-tests",
      traits: [],
    };
  },

  createChild() {
    return new ChildActor(this.conn, this.sequence++);
  },
});

class ChildFront extends protocol.FrontClassWithSpec(childSpec) {
  form(form) {
    this.childID = form.childID;
    this.foo = form.foo;
  }
}
protocol.registerFront(ChildFront);

class RootFront extends protocol.FrontClassWithSpec(rootSpec) {
  constructor(client) {
    super(client);
    this.actorID = "root";
    // Root owns itself.
    this.manage(this);
  }
}

add_task(async function run_test() {
  DevToolsServer.createRootActor = RootActor;
  DevToolsServer.init();

  const trace = connectPipeTracing();
  const client = new DevToolsClient(trace);
  await client.connect();

  const rootFront = new RootFront(client);

  const fronts = [];
  rootFront.onFront("childActor", front => {
    equal(
      front.foo,
      "bar",
      "Front's form is set before onFront listeners are called"
    );
    fronts.push(front);
  });

  const firstChild = await rootFront.createChild();
  ok(
    firstChild instanceof ChildFront,
    "createChild returns a ChildFront instance"
  );
  equal(firstChild.childID, 0, "First child has ID=0");

  equal(
    fronts.length,
    1,
    "onFront fires the callback, even if the front is created in the future"
  );
  equal(
    fronts[0],
    firstChild,
    "onFront fires the callback with the right front instance"
  );

  const onFrontAfter = await new Promise(resolve => {
    rootFront.onFront("childActor", resolve);
  });
  equal(
    onFrontAfter,
    firstChild,
    "onFront fires the callback, even if the front is already created, " +
      " with the same front instance"
  );

  equal(
    fronts.length,
    1,
    "There is still only one front reported from the first listener"
  );

  const secondChild = await rootFront.createChild();

  equal(
    fronts.length,
    2,
    "After a second call to createChild, two fronts are reported"
  );
  equal(fronts[1], secondChild, "And the new front is the right instance");

  trace.close();
  await client.close();
});
