/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009-2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

package org.mozilla.gecko;

import org.mozilla.gecko.db.BrowserDB;

import android.content.Context;
import android.preference.DialogPreference;
import android.util.AttributeSet;
import android.util.Log;

class ConfirmPreference extends DialogPreference {
    private static final String LOGTAG = "GeckoConfirmPreference";

    private String mAction = null;
    private Context mContext = null;
    public ConfirmPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        mAction = attrs.getAttributeValue(null, "action");
        mContext = context;
    }
    public ConfirmPreference(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        mAction = attrs.getAttributeValue(null, "action");
        mContext = context;
    }
    protected void onDialogClosed(boolean positiveResult) {
        if (!positiveResult)
            return;
        if ("clear_history".equalsIgnoreCase(mAction)) {
            GeckoAppShell.getHandler().post(new Runnable(){
                public void run() {
                    BrowserDB.clearHistory(mContext.getContentResolver());
                    GeckoApp.mAppContext.mFavicons.clearFavicons();
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("browser:purge-session-history", null));
                    GeckoApp.mAppContext.handleClearHistory();
                }
            });
        } else if ("clear_private_data".equalsIgnoreCase(mAction)) {
            GeckoAppShell.getHandler().post(new Runnable(){
                public void run() {
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createBroadcastEvent("Sanitize:ClearAll", null));
                }
            });
        }
        Log.i(LOGTAG, "action: " + mAction);
    }
}
