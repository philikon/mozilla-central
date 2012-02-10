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
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Malini Das (mdas@mozilla.com)
 *    Jonathan Griffin (jgriffin@mozilla.com)
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

/*
 * The Marionette object, passed to the script context.
 */

var Marionette = {
  is_async: false,
  win: null,
  tests: [],
  logObj: null,
  onerror: null,

  reset: function Marionette__reset() {
    Marionette.is_async = false;
    Marionette.tests = [];
    Marionette.onerror = null;
  },

  ok: function Marionette__ok(condition, name, diag) {
    var test = {'result': !!condition, 'name': name, 'diag': diag};
    Marionette.logResult(test, "TEST-PASS", "TEST-UNEXPECTED-FAIL");
    Marionette.tests.push(test);
  },

  is: function Marionette__is(a, b, name) {
    var pass = (a == b);
    var diag = pass ? Marionette.repr(a) + " should equal " + Marionette.repr(b)
                    : "got " + Marionette.repr(a) + ", expected " + Marionette.repr(b);
    Marionette.ok(pass, name, diag);
  },

  isnot: function Marionette__isnot (a, b, name) {
    var pass = (a != b);
    var diag = pass ? Marionette.repr(a) + " should not equal " + Marionette.repr(b)
                    : "didn't expect " + Marionette.repr(a) + ", but got it";
    Marionette.ok(pass, name, diag);
  },

  log: function Marionette__log(msg, level) {
    if (Marionette.logObj != null) {
      Marionette.logObj.log(msg, level);
    }
  },

  getLogs: function Marionette__getLogs() {
    if (Marionette.logObj != null) {
      Marionette.logObj.getLogs();
    }
  },

  generate_results: function Marionette__generate_results() {
    var passed = 0;
    var failed = 0;
    var failures = [];
    for (var i in Marionette.tests) {
      if(Marionette.tests[i].result) {
        passed++;
      }
      else {
        failed++;
        failures.push({'name': Marionette.tests[i].name,
                       'diag': Marionette.tests[i].diag});
      }
    }
    Marionette.reset();
    return {"passed": passed, "failed": failed, "failures": failures};
  },

  finish: function Marionette__finish() {
    if (Marionette.is_async) {
      if (Marionette.context == "content") {
        Marionette.asyncComplete(Marionette.generate_results(), 0);
      }
      else {
        Marionette.returnFunc(Marionette.generate_results(), 0);
      }
      Marionette.win.window.onerror = Marionette.onerror;
    }
    else {
      return Marionette.generate_results();
    }
  },

  returnFunc: function Marionette__returnFunc(value, status) {
    if (value == undefined)
      value = null;
    if (status == 0 || status == undefined) {
      Marionette.__conn.send({from: Marionette.__actorID, value: value, status: status});
    }
    else {
      var error_msg = {message: value, status: status, stacktrace: null};
      Marionette.__conn.send({from: Marionette.__actorID, error: error_msg});
    }
    if (Marionette.__timer != null) {
      Marionette.__timer.cancel();
      Marionette.__timer = null;
    }
  },

  asyncComplete: function Marionette__async_completed(value, status) {
      var document = Marionette.win.window.document;
      var __marionetteRes = document.getUserData('__marionetteRes');
      if(__marionetteRes.status == undefined) {
        __marionetteRes.value = value;
        __marionetteRes.status = status;
        document.setUserData('__marionetteRes', __marionetteRes, null);
        var ev = document.createEvent('Events');
        ev.initEvent('marionette-async-response', true, false);
        document.dispatchEvent(ev);
      }
    },

  logToFile: function Marionette__logToFile(file) {
    //TODO
  },

  logResult: function Marionette__logResult(test, passString, failString) {
    //TODO: dump to file
    var resultString = test.result ? passString : failString;
    var diagnostic = test.name + (test.diag ? " - " + test.diag : "");
    var msg = [resultString, diagnostic].join(" | ");
    dump("MARIONETTE TEST RESULT:" + msg + "\n");
  },

  repr: function Marionette__repr(o) {
      if (typeof(o) == "undefined") {
          return "undefined";
      } else if (o === null) {
          return "null";
      }
      try {
          if (typeof(o.__repr__) == 'function') {
              return o.__repr__();
          } else if (typeof(o.repr) == 'function' && o.repr != arguments.callee) {
              return o.repr();
          }
     } catch (e) {
     }
     try {
          if (typeof(o.NAME) == 'string' && (
                  o.toString == Function.prototype.toString ||
                  o.toString == Object.prototype.toString
              )) {
              return o.NAME;
          }
      } catch (e) {
      }
      try {
          var ostring = (o + "");
      } catch (e) {
          return "[" + typeof(o) + "]";
      }
      if (typeof(o) == "function") {
          o = ostring.replace(/^\s+/, "");
          var idx = o.indexOf("{");
          if (idx != -1) {
              o = o.substr(0, idx) + "{...}";
          }
      }
      return ostring;
  },

  defaultWaitForTimeout: 10000,
  waitFor: function test_waitFor(callback, test, timeout) {
      if (test()) {
          callback();
          return;
      }
      timeout = timeout || Date.now();
      if (Date.now() - timeout > Marionette.defaultWaitForTimeout) {
          throw 'waitFor timeout';
      }
      Marionette.win.window.setTimeout(Marionette.waitFor, 100, callback, test, timeout);
  },
};

