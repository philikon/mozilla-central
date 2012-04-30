/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;

public class WebViewExample extends Activity {
	private GeckoWebView mWebView;

    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mWebView = new GeckoWebView(this);
        setContentView(mWebView);

        //mWebView.getSettings().setJavaScriptEnabled(true);
        //mWebView.loadUrl("http://www.google.com");
    }

}
