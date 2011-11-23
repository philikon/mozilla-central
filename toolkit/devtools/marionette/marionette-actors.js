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

//set up the marionette content listener
var prefs = Components.classes["@mozilla.org/preferences-service;1"]
                     .getService(Components.interfaces.nsIPrefBranch);
var running = prefs.setBoolPref("marionette.contentListener", false);
var messageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                         getService(Ci.nsIChromeFrameMessageManager);
/* we only need one instance of each listener running in content space */
if (!prefs.getBoolPref("marionette.contentListener")) {
  messageManager.loadFrameScript("resource:///modules/marionette-listener.js", true);
  prefs.setBoolPref("marionette.contentListener", true);
}

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
  /* //for use with future actors
  listActors: function MRA_listActors() {
    return { "from": "root",
             "actors": [actor.actorID for each (actor in this._marionetteActors)] };
  },
  */
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
  this.messageListener = new MarionetteResponder(this);
  this.browser = null;
  this.tab = null;
}

MarionetteDriverActor.prototype = {

  actorPrefix: "marionette",

  //We will only ever be running one browser at a time
  //If no browser is running (ie: in B2G) start one up
  newSession: function MDA_newSession(aRequest) {
    if (!isB2G) {
      //XXX: this doesn't work in b2g
      //TODO: check if browser has started, if not, kick it off
      var WindowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1']  
                              .getService(Components.interfaces.nsIWindowMediator);  
      var win = WindowMediator.getMostRecentWindow('navigator:browser');
      this.browser = win.Browser; //BrowserApp?
      this.tab = this.browser.addTab("about:blank", true);
    }
    this.messageManager.sendAsyncMessage("Marionette:newSession", {B2G: isB2G});
  },

  setContext: function MDA_setContext(aRequest) {
    var context = aRequest.value;
    if (context != "content" && context != "chrome") {
      var error = { message: "invalid context", status: null, stacktrace: null};
      this.messageManager.sendAsyncMessage("Marionette:error", {error:error});
    }
    else {
      this.messageManager.sendAsyncMessage("Marionette:setContext", {value:context});
    }
  },

  execute: function MDA_execute(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:executeScript", {value: aRequest.value, args: aRequest.args});
  },

  setScriptTimeout: function MDA_setScriptTimeout(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:setScriptTimeout", {value: aRequest.value});
  },

  executeAsync: function MDA_executeAsync(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:executeAsyncScript", {value: aRequest.value, args: aRequest.args});
  },

  goUrl: function MDA_goUrl(aRequest) {
    this.messageManager.addMessageListener("DOMContentLoaded", this,true);
    this.browser.selectedBrowser.loadURI(aRequest.value);
  },

  deleteSession: function MDA_deleteSession(aRequest) {
    //this.messageManager.sendAsyncMessage("Marionette:deleteSession", {});
    if (!isB2G && this.tab != null) {
      this.browser.closeTab(this.tab);
      this.tab == null
    }
    this.conn.send({from:this.actorID, ok: true});
  },
  receiveMessage: function(message) {
    if (message.name == "DOMContentLoaded") {
      this.conn.send({from:this.actorID, ok: true});
      this.messageManager.removeMessageListener("DOMContentLoaded", this,true);
    }
  },
};

MarionetteDriverActor.prototype.requestTypes = {
  "newSession": MarionetteDriverActor.prototype.newSession,
  "setContext": MarionetteDriverActor.prototype.setContext,
  "executeScript": MarionetteDriverActor.prototype.execute,
  "setScriptTimeout": MarionetteDriverActor.prototype.setScriptTimeout,
  "executeAsyncScript": MarionetteDriverActor.prototype.executeAsync,
  "goUrl": MarionetteDriverActor.prototype.goUrl,
  "deleteSession": MarionetteDriverActor.prototype.deleteSession
};

/* 
 * MarionetteResponder listener was created instead of using a 
 * listener function since we need the actor's connection and id
 * in order to respond. We will only ever service one marionette JSON protocol user at a time.
 */
function MarionetteResponder(actor) {
  this.actor = actor;
  this.messageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                        .getService(Ci.nsIChromeFrameMessageManager);
  this.messageManager.addMessageListener("Marionette:ok", this);
  this.messageManager.addMessageListener("Marionette:done", this);
  this.messageManager.addMessageListener("Marionette:error", this);
}

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");  

MarionetteResponder.prototype = {
  classDescription: "MarionetteResponder",
  contractID: "@mozilla.org/marionette-responder;1",
  classID: Components.ID("{7FF808DA-FF1D-11E0-B50D-F84B4824019B}"),
  QueryInterface: XPCOMUtils.generateQI(Ci.nsIFrameMessageListener),

  receiveMessage: function(message) {
    if (message.name == "Marionette:done") {
      this.actor.conn.send({from:this.actor.actorID, value: message.json.value});
    }
    else if (message.name == "Marionette:ok") {
      this.actor.conn.send({from:this.actor.actorID, ok: true});
    }
    else if (message.name == "Marionette:error") {
      var error_msg = {status: message.json.error.status, message: message.json.error.message, stacktrace: message.json.error.stacktrace };
      this.actor.conn.send({from:this.actor.actorID, error: error_msg});
    }
  }
}
