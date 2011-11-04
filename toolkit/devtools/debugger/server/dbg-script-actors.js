/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Panos Astithas <past@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";
/**
 * JSD2 actors.
 */
/**
 * Creates a ThreadActor.
 *
 * ThreadActors manage a JSInspector object and manage execution/inspection
 * of debuggees.
 */
function ThreadActor(aHooks)
{
  this._state = "detached";
  this._frameActors = [];
  this._hooks = aHooks ? aHooks : {};
}

ThreadActor.prototype = {
  actorPrefix: "context",

  get state() { return this._state; },

  get dbg() { return this._dbg; },

  get threadLifetimePool() {
    if (!this._threadLifetimePool) {
      this._threadLifetimePool = new ActorPool(this.conn);
      this.conn.addActorPool(this._threadLifetimePool);
    }
    return this._threadLifetimePool;
  },

  _breakpointPool: null,
  get breakpointActorPool() {
    if (!this._breakpointPool) {
      this._breakpointPool = new ActorPool(this.conn);
      this.conn.addActorPool(this._breakpointPool);
    }
    return this._breakpointPool;
  },

  _scripts: {},

  /**
   * Add a debuggee global to the JSInspector.
   */
  addDebuggee: function TA_addDebuggee(aGlobal) {
    // Use the inspector xpcom component to turn on debugging
    // for aGlobal's compartment.  Ideally this won't be necessary
    // medium- to long-term, and will be managed by the engine
    // instead.

    if (!this._dbg) {
      this._dbg = new Debugger();
    }

    this.dbg.addDebuggee(aGlobal);
    this.dbg.uncaughtExceptionHook = this.uncaughtExceptionHook.bind(this);
    this.dbg.onDebuggerStatement = this.onDebuggerStatement.bind(this);
    this.dbg.onNewScript = this.onNewScript.bind(this);
    // Keep the debugger disabled until a client attaches.
    this.dbg.enabled = false;
  },

  /**
   * Remove a debuggee global from the JSInspector.
   */
  removeDebugee: function TA_removeDebuggee(aGlobal) {
    try {
      this.dbg.removeDebuggee(aGlobal);
    } catch(ex) {
      // XXX: This debuggee has code currently executing on the stack,
      // we need to save this for later.
    }
  },

  disconnect: function TA_disconnect() {
    this._state = "exited";
    if (this.dbg) {
      this.dbg.enabled = false;
      this._dbg = null;
    }
    this.conn.removeActorPool(this._threadLifetimePool || undefined);
    this._threadLifetimePool = null;
    this.conn.removeActorPool(this._breakpointPool);
    this._breakpointPool = null;
    for (let url in this._scripts) {
      delete this._scripts[url];
    }
    this._scripts = {};
  },

  /**
   * Disconnect the debugger and put the actor in the exited state.
   */
  exit: function TA_exit() {
    this.disconnect();
  },

  // Request handlers
  onAttach: function TA_onAttach(aRequest) {
    if (this.state === "exited") {
      return { type: "exited" };
    }

    if (this.state !== "detached") {
      return { error: "wrongState" };
    }

    this._state = "attached";

    this.dbg.enabled = true;
    try {
      // Put ourselves in the paused state.
      // XXX: We need to put the debuggee in a paused state too.
      let packet = this._paused();
      if (!packet) {
        return { error: "notAttached" };
      }
      packet.why = { type: "attached" };

      // Send the response to the attach request now (rather than
      // returning it), because we're going to start a nested event loop
      // here.
      this.conn.send(packet);

      // Start a nested event loop.
      this._nest();

      // We already sent a response to this request, don't send one
      // now.
      return null;
    } catch(e) {
      dumpn(e);
      return { error: "notAttached", message: e.toString() };
    }
  },

  onDetach: function TA_onDetach(aRequest) {
    this.disconnect();
    return { type: "detached" };
  },

  onResume: function TA_onResume(aRequest) {
    let packet = this._resumed();
    DebuggerServer.xpcInspector.exitNestedEventLoop();
    return packet;
  },

  onClientEvaluate: function TA_onClientEvaluate(aRequest) {
    if (this.state !== "paused") {
      return { type: "wrongState",
               message: "Debuggee must be paused to evaluate code." };
    };

    let frame = this._requestFrame(aRequest.frame);
    if (!frame) {
      // XXXspec
      return { type: "unknownFrame",
               message: "Evaluation frame not found" };
    }


    // We'll clobber the youngest frame if the eval causes a pause, so
    // save our frame now to be restored after eval returns.
    // XXX: or we could just start using dbg.getNewestFrame() now that it
    // works as expected.
    let youngest = this._youngestFrame;

    // Put ourselves back in the running state and inform the client.
    let resumedPacket = this._resumed();
    this.conn.send(resumedPacket);

    // Run the expression.
    // XXX: test syntax errors
    let completion = frame.eval(aRequest.expression);

    // Put ourselves back in the pause state.
    let packet = this._paused(youngest);
    packet.why = { type: "clientEvaluated" };
    if ("return" in completion) {
      packet.why.value = this.valueGrip(completion["return"]);
    } else if ("throw" in completion) {
      packet.why.exception = this.valueGrip(completion["throw"]);
    } else {
      // XXXspec
      packet.why.terminated = true;
    }

    // Return back to our previous pause's event loop.

    return packet;
  },

  onFrames: function TA_onFrames(aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Stack frames are only available while the debuggee is paused."};
    }

    let start = aRequest.start ? aRequest.start : 0;
    let count = aRequest.count;

    // Find the starting frame...
    let frame = this._youngestFrame;
    let i = 0;
    while (frame && (i < start)) {
      frame = frame.older;
      i++;
    }

    // Return request.count frames, or all remaining
    // frames if count is not defined.
    let frames = [];
    for (; frame && (!count || i < (start + count)); i++) {
      let grip = this._frameActor(frame).grip();
      grip.depth = i;
      frames.push(grip);
      frame = frame.older;
    }

    return { frames: frames };
  },

  onReleaseMany: function TA_onReleaseMany(aRequest) {
    for each (let actorID in aRequest.actors) {
      let actor = this.threadLifetimePool.get(actorID);
      this.threadLifetimePool.objectActors.delete(actor.obj);
      this.threadLifetimePool.removeActor(actorID);
    }
    return {};
  },

  /**
   * Handle a request to set a breakpoint.
   */
  onSetBreakpoint: function TA_onSetBreakpoint(aRequest) {
    if (this.state !== "paused") {
      return { error: "wrongState",
               message: "Breakpoints can only be set while the debuggee is paused."};
    }

    let location = aRequest.location;
    // TODO: deal with actualLocation being different from the provided location
    if (!this._scripts[location.url] || location.line < 0) {
      return { from: this.actorID,
               error: "noScript" };
    }
    // Fetch the list of scripts in that url.
    let scripts = this._scripts[location.url];
    // Fetch the specified script in that list.
    let script = null;
    for (let i = location.line; i >= 0; i--) {
      // Stop when the first script that contains this location is found.
      if (scripts[i]) {
        // If that first script does not contain the line specified, it's no
        // good.
        if (i + scripts[i].lineCount < location.line) {
          break;
        }
        script = scripts[i];
        break;
      }
    }
    if (!script) {
      return { from: this.actorID,
               error: "noScript" };
    }
    let bpActor = new BreakpointActor(this, script);
    this.breakpointActorPool.addActor(bpActor);
    var offsets = script.getLineOffsets(location.line);
    for (var i = 0; i < offsets.length; i++) {
      script.setBreakpoint(offsets[i], bpActor);
    }
    let packet = { from: this.actorID,
                   actor: bpActor.actorID };
    return packet;
  },

  /**
   * Handle a request to return the list of loaded scripts.
   */
  onScripts: function TA_onScripts(aRequest) {
    let scripts = [];
    for (let url in this._scripts) {
      for (let i = 0; i < this._scripts[url].length; i++) {
        if (!this._scripts[url][i]) {
          continue;
        }
        let script = {
          url: url,
          startLine: i,
          lineCount: this._scripts[url][i].lineCount
        };
        scripts.push(script);
      }
    }

    let packet = { from: this.actorID,
                   scripts: scripts };
    return packet;
  },

  /**
   * Return the Debug.Frame for a frame mentioned by the protocol.
   */
  _requestFrame: function TA_requestFrame(aFrameID) {
    // XXXspec: doesn't actually specify how frames are named.  By
    // depth?  By actor?  Both?
    if (!aFrameID) {
      return this._youngestFrame;
    }

    if (this._framePool.has(aFrameID)) {
      return this._framePool.get(aFrameID).frame;
    }

    return undefined;
  },

  _paused: function TA_paused(aFrame) {
    // XXX: We don't handle nested pauses correctly.  Don't try - if we're
    // paused, just continue running whatever code triggered the pause.

    // We don't want to actually have nested pauses (although we will
    // have nested event loops).  If code runs in the debuggee during
    // a pause, it should cause the actor to resume (dropping
    // pause-lifetime actors etc) and then repause when complete.

    if (this.state === "paused") {
      return undefined;
    }

    this._state = "paused";

    // Save the pause frame (if any) as the youngest frame for
    // stack viewing.
    this._youngestFrame = aFrame;

    // Create the actor pool that will hold the pause actor and its
    // children.
    dbg_assert(!this._pausePool);
    this._pausePool = new ActorPool(this.conn);
    this.conn.addActorPool(this._pausePool);

    // Give children of the pause pool a quick link back to the
    // thread...
    this._pausePool.threadActor = this;

    // Create the pause actor itself...
    dbg_assert(!this._pauseActor);
    this._pauseActor = new PauseActor(this._pausePool);
    this._pausePool.addActor(this._pauseActor);

    // Update the list of frames.
    let poppedFrames = this._updateFrames();

    // Send off the paused packet and spin an event loop.
    let packet = { from: this.actorID,
                   type: "paused",
                   actor: this._pauseActor.actorID };
    if (aFrame) {
      packet.frame = this._frameActor(aFrame).grip();
    }

    if (poppedFrames) {
      packet.poppedFrames = poppedFrames;
    }

    return packet;
  },

  _nest: function TA_nest() {
    if (this._hooks.preNest) {
      var nestData = this._hooks.preNest();
    }

    DebuggerServer.xpcInspector.enterNestedEventLoop();

    dbg_assert(this.state === "running");

    if (this._hooks.postNest) {
      this._hooks.postNest(nestData)
    }

    // "continue" resumption value.
    return undefined;
  },

  _resumed: function TA_resumed() {
    this._state = "running";

    // Drop the actors in the pause actor pool.
    this.conn.removeActorPool(this._pausePool);

    this._pausePool = null;
    this._pauseActor = null;
    this._youngestFrame = null;

    return { from: this.actorID, type: "resumed" };
  },

  /**
   * Expire frame actors for frames that have been popped.
   *
   * @returns A list of actor IDs whose frames have been popped.
   */
  _updateFrames: function TA_updateFrames() {
    let popped = [];

    // Create the actor pool that will hold the still-living frames.
    let framePool = new ActorPool(this.conn);
    let frameList = [];

    for each (let frameActor in this._frameActors) {
      if (frameActor.frame.live) {
        framePool.addActor(frameActor);
        frameList.push(frameActor);
      } else {
        popped.push(frameActor.actorID);
      }
    }

    // Remove the old frame actor pool, this will expire
    // any actors that weren't added to the new pool.
    if (this._framePool) {
      this.conn.removeActorPool(this._framePool);
    }

    this._frameActors = frameList;
    this._framePool = framePool;
    this.conn.addActorPool(framePool);

    return popped;
  },

  _frameActor: function TA_threadActor(aFrame) {
    if (aFrame.actor) {
      return aFrame.actor;
    }

    let actor = new FrameActor(aFrame, this);
    this._frameActors.push(actor);
    this._framePool.addActor(actor);
    aFrame.actor = actor;

    return actor;
  },

  /**
   * Create a grip for the given debuggee value.  If the value is an
   * object, will create a pause-lifetime actor.
   */
  valueGrip: function TA_valueGrip(aValue) {
    let type = typeof(aValue);
    if (type === "boolean" || type === "string" || type === "number") {
      return aValue;
    }

    if (aValue === null) {
      return { type: "null" };
    }

    if (aValue === undefined) {
      return { type: "undefined" }
    }

    if (typeof(aValue) === "object") {
      return this.pauseObjectGrip(aValue);
    }

    dbg_assert(false, "Failed to provide a grip for: " + aValue);
    return null;
  },

  objectGrip: function TA_objectGrip(aValue, aPool) {
    if (!aPool.objectActors) {
      aPool.objectActors = new WeakMap();
    }

    if (aPool.objectActors.has(aValue)) {
      return aPool.objectActors.get(aValue).grip();
    }

    let actor = new ObjectActor(aValue, this);
    aPool.addActor(actor);
    aPool.objectActors.set(aValue, actor);
    return actor.grip();
  },

  pauseObjectGrip: function TA_pauseObjectGrip(aValue) {
    if (!this._pausePool) {
      throw "Object grip requested while not paused.";
    }

    return this.objectGrip(aValue, this._pausePool);
  },

  threadObjectGrip: function TA_threadObjectGrip(aValue) {
    return this.objectGrip(aValue, this.threadLifetimePool);
  },

  // JS Debugger hooks.
  uncaughtExceptionHook: function TA_uncaughtExceptionHook(aException) {
    dumpn("Got an exception:" + aException);
  },

  onDebuggerStatement: function TA_onDebuggerStatement(aFrame) {
    try {
      let packet = this._paused(aFrame);
      if (!packet) {
        return undefined;
      }
      packet.why = { type: "debuggerStatement" };
      this.conn.send(packet);
      return this._nest();
    } catch(e) {
      dumpn("Got an exception during onDebuggerStatement: " + e + ': ' + e.stack);
      return undefined;
    }
  },

  onNewScript: function TA_onNewScript(aScript, aFunction) {
    dumpn("Got a new script:" + aScript + ", url: " + aScript.url +
          ", startLine: " + aScript.startLine + ", lineCount: " +
          aScript.lineCount + ", strictMode: " + aScript.strictMode +
          ", function: " + aFunction);
    // Use a sparse array for storing the scripts for each URL in order to
    // optimize retrieval. XXX: in case this is not fast enough for very large
    // files with too many scripts, we could sort the hash of script locations
    // or use a trie.
    if (!this._scripts[aScript.url]) {
      this._scripts[aScript.url] = [];
    }
    this._scripts[aScript.url][aScript.startLine] = aScript;
    // Notify the client.
    this.conn.send({ from: this.actorID, type: "newScript",
                     url: aScript.url, startLine: aScript.startLine });
  }

};

ThreadActor.prototype.requestTypes = {
  "attach": ThreadActor.prototype.onAttach,
  "detach": ThreadActor.prototype.onDetach,
  "resume": ThreadActor.prototype.onResume,
  "clientEvaluate": ThreadActor.prototype.onClientEvaluate,
  "frames": ThreadActor.prototype.onFrames,
  "releaseMany": ThreadActor.prototype.onReleaseMany,
  "setBreakpoint": ThreadActor.prototype.onSetBreakpoint,
  "scripts": ThreadActor.prototype.onScripts
};


/**
 * Creates a PauseActor.
 *
 * PauseActors exist for the lifetime of a given debuggee pause.  Used to
 * scope pause-lifetime grips.
 *
 * @param ActorPool aPool
 *        The actor pool created for this pause.
 */
function PauseActor(aPool)
{
  this.pool = aPool;
}

PauseActor.prototype = {
  actorPrefix: "pause"
};


function ObjectActor(aObj, aThreadActor)
{
  this.obj = aObj;
  this.threadActor = aThreadActor;
}

ObjectActor.prototype = {
  actorPrefix: "obj",

  WRONG_STATE_RESPONSE: {
    error: "wrongState",
    message: "Object actors can only be accessed while the thread is paused."
  },

  grip: function OA_grip() {
    return { "type": "object",
             "class": this.obj["class"],
             "actor": this.actorID };
  },

  release: function AO_release() {
    this.registeredPool.objectActors.delete(this.obj);
    this.registeredPool.removeActor(this.actorID);
  },

  onNameAndParameters: function OA_onNameAndParameters(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

    if (this.obj["class"] !== "Function") {
      // XXXspec: Error type for this.
      return { error: "unrecognizedPacketType",
               message: "nameAndParameters request is only valid for object grips with a 'Function' class." };
    }

    return { name: this.obj.name || null,
             parameters: this.obj.parameterNames };
  },

  onThreadGrip: function OA_onThreadGrip(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }

    return { threadGrip: this.threadActor.threadObjectGrip(this.obj) };
  },

  onRelease: function OA_onRelease(aRequest) {
    if (this.threadActor.state !== "paused") {
      return this.WRONG_STATE_RESPONSE;
    }
    if (this.registeredPool !== this.threadActor.threadLifetimePool) {
      // XXXspec: error type?
      return { error: "unrecognizedPacketType",
               message: "release is only recognized on thread-lifetime actors." };
    }

    this.release();
    return {};
  },
};

ObjectActor.prototype.requestTypes = {
  "nameAndParameters": ObjectActor.prototype.onNameAndParameters,
  "threadGrip": ObjectActor.prototype.onThreadGrip,
  "release": ObjectActor.prototype.onRelease,
};


function FrameActor(aFrame, aThreadActor)
{
  this.frame = aFrame;
  this.threadActor = aThreadActor;
}

FrameActor.prototype = {
  actorPrefix: "frame",

  grip: function FA_grip() {
    let grip = { actor: this.actorID,
                 type: this.frame.type };
    if (this.frame.type === "call") {
      grip.callee = this.threadActor.valueGrip(this.frame.callee);
      grip.calleeName = this.frame.callee.name;
    }

    grip.arguments = this.args();

    if (!this.frame.older) {
      grip.oldest = true;
    }

    return grip;
  },

  args: function FA_args() {
    if (!this.frame["arguments"]) {
      return [];
    }

    return [this.threadActor.valueGrip(arg)
            for each (arg in this.frame["arguments"])];
  },

  onPop: function FA_onPop(aRequest) {
    return { error: "notImplemented",
             message: "Popping frames is not yet implemented." };
  }
};

FrameActor.prototype.requestTypes = {
  "pop": FrameActor.prototype.onPop,
};


/**
 * Creates a BreakpointActor. BreakpointActors exist for the lifetime of their
 * containing thread and are responsible for deleting breakpoints, handling
 * breakpoint hits and associating breakpoints with scripts.
 *
 * @param ThreadActor aThreadActor
 *        The parent thread actor that contains this breakpoint.
 * @param Debugger.Script aScript
 *        The script this breakpoint is set on.
 */
function BreakpointActor(aThreadActor, aScript)
{
  this.threadActor = aThreadActor;
  this.script = aScript;
}

BreakpointActor.prototype = {
  actorPrefix: "breakpoint",

  hit: function BA_hit(aFrame) {
    try {
      let packet = this.threadActor._paused(aFrame);
      if (!packet) {
        return undefined;
      }
      // TODO: add the rest of the breakpoints on that line.
      packet.why = { type: "breakpoint", actors: [ this.actorID ] };
      this.conn.send(packet);
      return this.threadActor._nest();
    } catch(e) {
      dumpn("Got an exception during hit: " + e + ': ' + e.stack);
      return undefined;
    }
  },

  onDelete: function BA_onDelete(aRequest) {
    this.threadActor.breakpointActorPool.removeActor(this.actorID);
    this.script.clearBreakpoint(this);
    this.script = null;

    return { from: this.actorID };
  }
};

BreakpointActor.prototype.requestTypes = {
  "delete": BreakpointActor.prototype.onDelete,
};

