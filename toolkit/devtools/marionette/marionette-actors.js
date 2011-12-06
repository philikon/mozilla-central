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

"use strict";
/**
 * Gecko-specific actors.
 */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(Components.interfaces.nsIPrefBranch);
prefs.setBoolPref("marionette.contentListener", false);

var xulAppInfo = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULAppInfo);
var isB2G = xulAppInfo.name.indexOf('B2G') > -1;

function createRootActor(aConnection)
{
  return new MarionetteRootActor(aConnection);
}

function MarionetteRootActor(aConnection)
{
  this.conn = aConnection;
  /* will only ever be one marionette actor */
  this._marionetteActor = new MarionetteDriverActor(this.conn);
  this._marionetteActorPool = null; //hold future actors

  this._marionetteActorPool = new ActorPool(this.conn);
  this._marionetteActorPool.addActor(this._marionetteActor);
  this.conn.addActorPool(this._marionetteActorPool);
}

MarionetteRootActor.prototype = {
  sayHello: function MRA_sayHello() {
    return { from: "root",
             applicationType: "gecko",
             traits: [] };
  },

  disconnect: function MRA_disconnect() {
    this._marionetteActor.deleteSession();
  },

  /* used to get the running marionette actor, so we can issue commands */
  getMarionetteID: function MRA_getMarionette() {
    return { "from": "root",
             "id": this._marionetteActor.actorID } ;
  },
}

MarionetteRootActor.prototype.requestTypes = {
  "getMarionetteID": MarionetteRootActor.prototype.getMarionetteID,
  "sayHello": MarionetteRootActor.prototype.sayHello
};

function MarionetteDriverActor(aConnection)
{
  this.conn = aConnection;
  this.messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                             getService(Ci.nsIChromeFrameMessageManager);
  this.messageManager.addMessageListener("Marionette:ok", this);
  this.messageManager.addMessageListener("Marionette:done", this);
  this.messageManager.addMessageListener("Marionette:error", this);
  this.windowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService(Components.interfaces.nsIWindowMediator);
  this.browser = null;
  this.tab = null;
  this.context = "content";
  this.scriptTimeout = null;
  this.timer = null;
}

MarionetteDriverActor.prototype = {

  actorPrefix: "marionette",

  sendResponse: function (value) {
    this.conn.send({from:this.actorID, value: value});
  },

  sendOk: function() {
    this.conn.send({from:this.actorID, ok: true});
  },

  sendError: function (message, status, trace) {
    var error_msg = {message: message, status: status, stacktrace: trace};
    this.conn.send({from:this.actorID, error: error_msg});
  },

  getCurrentWindow: function() {
    var type = null;
    if (!isB2G) {
      type = 'navigator:browser';
    }
    return this.windowMediator.getMostRecentWindow(type);
  },

  //We will only ever be running one browser at a time
  //If no browser is running (ie: in B2G) start one up
  newSession: function MDA_newSession(aRequest) {
    if (isB2G) {
      if (!prefs.getBoolPref("marionette.contentListener")) {
        this.messageManager.loadFrameScript("resource:///modules/marionette-listener.js", false);
        prefs.setBoolPref("marionette.contentListener", true);
      }
    } else {
      //XXX: this doesn't work in b2g
      //TODO: check if browser has started, if not, kick it off
      var WindowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1']  
                              .getService(Components.interfaces.nsIWindowMediator);  
      var win = WindowMediator.getMostRecentWindow('navigator:browser');
      if(win.gBrowser != undefined) {
        this.browser = win.gBrowser; 
      }
      else {
        this.browser = win.Browser; //BrowserApp for birch?
      }

      this.tab = this.browser.addTab("about:blank", true);
      /* we only need one instance of each listener running in content space */
      if (!prefs.getBoolPref("marionette.contentListener")) {
        if(win.gBrowser != undefined) {
          this.browser.getBrowserForTab(this.tab).messageManager.loadFrameScript("resource:///modules/marionette-listener.js", false);
        }
        else {
          this.tab.browser.messageManager.loadFrameScript("resource:///modules/marionette-listener.js", false);
        }
        prefs.setBoolPref("marionette.contentListener", true);
      }
    }
    this.messageManager.sendAsyncMessage("Marionette:newSession", {B2G: isB2G});
  },

  setContext: function MDA_setContext(aRequest) {
    var context = aRequest.value;
    if (context != "content" && context != "chrome") {
      this.sendError("invalid context", 17, null); //TODO find more appropriate errmsg
    }
    else {
      this.context = context;
      this.sendOk();
    }
  },

  execute: function MDA_execute(aRequest) {
    if (this.context == "chrome") {
      var curWindow = this.getCurrentWindow();
      try {
        var params = aRequest.args;
        var _chromeSandbox = new Cu.Sandbox(curWindow,
           { sandboxPrototype: curWindow, wantXrays: false, 
             sandboxName: ''});
        _chromeSandbox.__marionetteParams = params;
        var script = "var func = function() {" + aRequest.value + "}; func.apply(null, __marionetteParams);";
        var res = Cu.evalInSandbox(script, _chromeSandbox);
        this.sendResponse(res);
      }
      catch (e) {
        // 17 = JavascriptException
        this.sendError(e.name + ': ' + e.message, 17, null);
      }
    }
    else {
      this.messageManager.sendAsyncMessage("Marionette:executeScript", {value: aRequest.value, args: aRequest.args});
    }
  },

  setScriptTimeout: function MDA_setScriptTimeout(aRequest) {
    var timeout = parseInt(aRequest.value);
    if(isNaN(timeout)){
      this.sendError("Not a Number", 17, null); //TODO: find more appropriate error message
    }
    else {
      this.scriptTimeout = timeout;
      this.messageManager.sendAsyncMessage("Marionette:setScriptTimeout", {value: timeout});
      this.sendOk();
    }
  },

  executeAsync: function MDA_executeAsync(aRequest) {
    if (this.context == "chrome") {
      var curWindow = this.getCurrentWindow();
      this.timer = Components.classes["@mozilla.org/timer;1"]
                  .createInstance(Components.interfaces.nsITimer);
      var params = aRequest.args;
      var _chromeSandbox = new Cu.Sandbox(curWindow,
         { sandboxPrototype: curWindow, wantXrays: false, 
           sandboxName: ''});
      _chromeSandbox.__marionetteParams = params;
      _chromeSandbox.__marionetteTimer = this.timer;
      _chromeSandbox.__conn = this.conn; //must send msgs on actual connection, since we can't send messages from chrome->chrome
      _chromeSandbox.__actorID = this.actorID;
      var returnFunc = 'var returnFunc = function(value, status) { __conn.send({from: __actorID, value: value, status: status});' 
                                                               +'__marionetteTimer.cancel(); __marionetteTimer = null;};';
      var script = returnFunc
                  +'__marionetteParams.push(returnFunc);'
                  +'var marionetteScriptFinished = returnFunc;'
                  +'var timeoutFunc = function() {returnFunc("timed out", 28);};'
                  +'var __marionetteFunc = function() {' + aRequest.value + '};'
                  +'__marionetteFunc.apply(null, __marionetteParams);'
                  +'if(__marionetteTimer != null) {__marionetteTimer.initWithCallback(timeoutFunc, '+ this.scriptTimeout +', Components.interfaces.nsITimer.TYPE_ONE_SHOT);}';
      try {
       Cu.evalInSandbox(script, _chromeSandbox);
      } catch (e) {
        sendError("e.message", 17, null);
      }
    }
    else {
      this.messageManager.sendAsyncMessage("Marionette:executeAsyncScript", {value: aRequest.value, args: aRequest.args});
    }
  },

  goUrl: function MDA_goUrl(aRequest) {
    this.messageManager.addMessageListener("DOMContentLoaded", this,true);
    this.browser.selectedBrowser.loadURI(aRequest.value);
  },

  setSearchTimeout: function MDA_setSearchTimeout(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:setSearchTimeout", {value: aRequest.value});
  },

  findElement: function MDA_findElement(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:findElement", {value: aRequest.value, using: aRequest.using, element: aRequest.element});
  },

  clickElement: function MDA_clickElement(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:clickElement", {element: aRequest.element});
  },

  deleteSession: function MDA_deleteSession(aRequest) {
    if (!isB2G && this.tab != null) {
      if(this.browser.closeTab == undefined) {
        this.browser.removeTab(this.tab);
      }
      else {
        this.browser.closeTab(this.tab);
      }
      this.tab == null
      //don't set this pref for B2G since the framescript can be safely reused
      this.messageManager.sendAsyncMessage("Marionette:deleteSession", {});
      prefs.setBoolPref("marionette.contentListener", false);
    }
    this.messageManager.removeMessageListener("Marionette:ok", this);
    this.messageManager.removeMessageListener("Marionette:done", this);
    this.messageManager.removeMessageListener("Marionette:error", this);
    this.sendOk();
  },

  receiveMessage: function(message) {
    if (message.name == "DOMContentLoaded") {
      this.sendOk();
      this.messageManager.removeMessageListener("DOMContentLoaded", this,true);
    }
    else if (message.name == "Marionette:done") {
      this.sendResponse(message.json.value);
    }
    else if (message.name == "Marionette:ok") {
      this.sendOk();
    }
    else if (message.name == "Marionette:error") {
      this.sendError(message.json.message, message.json.status, message.json.stacktrace);
    }
  },
};

MarionetteDriverActor.prototype.requestTypes = {
  "newSession": MarionetteDriverActor.prototype.newSession,
  "setContext": MarionetteDriverActor.prototype.setContext,
  "executeScript": MarionetteDriverActor.prototype.execute,
  "setScriptTimeout": MarionetteDriverActor.prototype.setScriptTimeout,
  "executeAsyncScript": MarionetteDriverActor.prototype.executeAsync,
  "setSearchTimeout": MarionetteDriverActor.prototype.setSearchTimeout,
  "findElement": MarionetteDriverActor.prototype.findElement,
  "clickElement": MarionetteDriverActor.prototype.clickElement,
  "goUrl": MarionetteDriverActor.prototype.goUrl,
  "deleteSession": MarionetteDriverActor.prototype.deleteSession
};

