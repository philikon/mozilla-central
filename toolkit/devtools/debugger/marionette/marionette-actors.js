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
var marMessageManager = Cc["@mozilla.org/globalmessagemanager;1"].
                           getService(Ci.nsIChromeFrameMessageManager);
marMessageManager.loadFrameScript("resource:///modules/marionette-listener.js", true);

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
    //TODO: check if browser has started, if not, kick it off
    var WindowMediator = Components.classes['@mozilla.org/appshell/window-mediator;1']  
                            .getService(Components.interfaces.nsIWindowMediator);  
    var win = WindowMediator.getMostRecentWindow('navigator:browser');
    this.browser = win.Browser; //BrowserApp?
    this.tab = this.browser.addTab("about:blank", true);
    return { value: 'mobile' };
  },

  execute: function MDA_execute(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:executeScript", {value: aRequest.value, args: aRequest.args, session: aRequest.session });
  },

  setScriptTimeout: function MDA_setScriptTimeout(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:setScriptTimeout", {value: aRequest.value, session: aRequest.session });
  },

  executeAsync: function MDA_executeAsync(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:executeAsyncScript", {value: aRequest.value, args: aRequest.args, session: aRequest.session });
  },

  deleteSession: function MDA_deleteSession(aRequest) {
    this.messageManager.sendAsyncMessage("Marionette:deleteSession", {session: aRequest.session});
    this.browser.closeTab(this.tab);
  },
};

MarionetteDriverActor.prototype.requestTypes = {
  "executeScript": MarionetteDriverActor.prototype.execute,
  "setScriptTimeout": MarionetteDriverActor.prototype.setScriptTimeout,
  "executeAsyncScript": MarionetteDriverActor.prototype.executeAsync,
  "newSession": MarionetteDriverActor.prototype.newSession,
  "deleteSession": MarionetteDriverActor.prototype.deleteSession
  //"executeInChrome": MarionetteDriverActor.prototype.executeInChrome,
};

/* 
 * MarionetteResponder listener was created instead of using a 
 * listener function since we need the actor's connection and id
 * in order to respond
 */
function MarionetteResponder(actor) {
  this.actor = actor;
  this.messageManager = Cc["@mozilla.org/globalmessagemanager;1"]
                        .getService(Ci.nsIChromeFrameMessageManager);
  this.messageManager.addMessageListener("Marionette:done", this);
  this.messageManager.addMessageListener("Marionette:error", this);
  this.messageManager.addMessageListener("Marionette:deleteSession", this);
}

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");  

MarionetteResponder.prototype = {
  classDescription: "MarionetteResponder",
  contractID: "@mozilla.org/marionette-controller;1",
  classID: Components.ID("{7FF808DA-FF1D-11E0-B50D-F84B4824019B}"),
  QueryInterface: XPCOMUtils.generateQI(Ci.nsIFrameMessageListener),

  receiveMessage: function(message) {
    if (message.name == "Marionette:done") {
      if (message.json.ok != undefined) {
        this.actor.conn.send({from:this.actor.actorID, session: message.json.session, type:message.json.type, ok: message.json.ok});
      }
      else {
        this.actor.conn.send({from:this.actor.actorID, session: message.json.session, type:message.json.type, value: message.json.value});
      }
    }
    else if (message.name == "Marionette:error") {
      var error_msg = {status: message.json.error.status, message: message.json.error.message, stacktrace: message.json.error.stacktrace };
      this.actor.conn.send({from:this.actor.actorID, type:message.json.type, error: error_msg});
    }
    else if (message.name == "Marionette:deleteSession") {
      this.messageManager.removeMessageListener("Marionette:done", this);
      this.messageManager.removeMessageListener("Marionette:error", this);
      this.messageManager.removeMessageListener("Marionette:deleteSession", this);
      this.actor.conn.send({from:this.actor.actorID, type:"deleteSession", session: message.json.session, ok:"ok"});
    }
  }
}
