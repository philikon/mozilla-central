/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import org.mozilla.gecko.gfx.LayerView;
import org.json.JSONObject;
import org.json.JSONException;

import android.app.Activity;
import android.content.Context;
import android.net.http.SslCertificate;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Picture;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.webkit.DownloadListener;
import android.webkit.ValueCallback;
import android.webkit.WebBackForwardList;
import android.webkit.WebChromeClient;
import android.webkit.WebViewClient;
import android.webkit.WebSettings;
import android.webkit.WebView.HitTestResult;
import android.widget.LinearLayout;

import java.util.Map;

public class GeckoWebView extends LinearLayout
                          implements GeckoEventListener,
                                     Gecko.ChromeClient {

    Activity mActivity;
    Handler mMainHandler;
    LayerView mLayerView;

    public GeckoWebView(Activity activity) {
        super(activity);

        mActivity = activity;

        GeckoAppShell.registerGlobalExceptionHandler();

        mMainHandler = new Handler();
        Gecko gecko = new Gecko(mActivity, mMainHandler);
        Gecko.instance.setChromeClient(this);
        Gecko.instance.initialize(activity.getIntent(), null, false);

        mLayerView = Gecko.instance.getLayerController().getView();
        addView(mLayerView);
    }

    private void createTab() {
        JSONObject args = new JSONObject();
        try {
            args.put("url", "about:blank");
        } catch (JSONException e) {
            //TODO log
            return;
        }
        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createBroadcastEvent("Tab:Add", args.toString()));
    }

    /**
     * GeckoEventListener
     */

    public void handleMessage(String event, JSONObject message) {
        if (event.equals("Tab:Added")) {
            loadUrl("http://www.google.com");
        }
    }

    /**
     * ChromeClient
     */

    public void onReady() {
        GeckoAppShell.registerGeckoEventListener("Tab:Added", this);
        createTab();
        //TODO
    }

    public void restartApplication() {}

    /**
     * The Android WebView interface
     */

    public SslCertificate getCertificate() {
        return null; //TODO
    }
    public void setCertificate(SslCertificate certificate) {} //TODO

    public void savePassword(String host, String username, String password) {} //TODO
    public void setHttpAuthUsernamePassword(String host, String realm, String username, String password) {} //TODO
    public String[] getHttpAuthUsernamePassword(String host, String realm) {
        return null; //TODO
    }

    public void destroy() {} //TODO
    public void setNetworkAvailable(boolean networkUp) {} //TODO
    public void setNetworkType(String type, String subtype) {} //TODO

    public WebBackForwardList saveState(Bundle outState) {
        return null; //TODO
    }

    public WebBackForwardList restoreState(Bundle inState) {
        return null; //TODO
    }

    public void loadUrl(String url, Map<String, String> additionalHttpHeaders) {
        JSONObject args = new JSONObject();
        try {
            args.put("url", url);
        } catch (JSONException e) {
            //TODO log
            return;
        }
        GeckoAppShell.sendEventToGecko(
            GeckoEvent.createBroadcastEvent("Tab:Load", args.toString()));
    }

    public void loadUrl(String url) {
        loadUrl(url, null);
    }

    public void postUrl(String url, byte[] postData) {} //TODO
    public void loadData(String data, String mimeType, String encoding) {} //TODO
    public void loadDataWithBaseURL(String baseUrl, String data, String mimeType, String encoding, String historyUrl) {} //TODO
    public void saveWebArchive(String filename) {} //TODO
    public void saveWebArchive(String basename, boolean autoname, ValueCallback<String> callback) {} //TODO
    public void stopLoading() {} //TODO
    public void reload() {} //TODO

    public boolean canGoBack() {
        return true; //TODO
    }

    public void goBack() {} //TODO
    public boolean canGoForward() {
        return true; //TODO
    }
    public void goForward() {} //TODO
    public boolean canGoBackOrForward(int steps) {
        return true; //TODO
    }
    public void goBackOrForward(int steps) {} //TODO
    public boolean isPrivateBrowsingEnabled() {
        return true; //TODO
    }
    public boolean pageUp(boolean top) {
        return true; //TODO
    }
    public boolean pageDown(boolean bottom) {
        return true; //TODO
    }
    public void clearView() {} //TODO

    public Picture capturePicture() {
        return null; //TODO
    }

    public float getScale() {
        return 1.0f; //TODO
    }

    public void setInitialScale(int scaleInPercent) {} //TODO
    public void invokeZoomPicker() {} //TODO

    public HitTestResult getHitTestResult() {
        return null; //TODO
    }

    public void requestFocusNodeHref(Message hrefMsg) {} //TODO
    public void requestImageRef(Message msg) {} //TODO

    public String getUrl() {
        return null; //TODO
    }

    public String getOriginalUrl() {
        return null; //TODO
    }

    public String getTitle() {
        return null; //TODO
    }

    public Bitmap getFavicon() {
        return null; //TODO
    }

    public int getProgress() {
        return 100; //TODO
    }

    public int getContentHeight() {
        return 0; //TODO
    }

    public void pauseTimers() {} //TODO
    public void resumeTimers() {} //TODO
    public void onPause() {} //TODO
    public void onResume() {} //TODO
    public boolean isPaused() {
        return false; //TODO
    }

    public void freeMemory() {} //TODO
    public void clearCache(boolean includeDiskFiles) {} //TODO
    public void clearFormData() {} //TODO
    public void clearHistory() {} //TODO
    public void clearSslPreferences() {} //TODO

    public WebBackForwardList copyBackForwardList() {
        return null; //TODO
    }

    public void findNext(boolean forward) {} //TODO
    public int findAll(String find) {
        return 0; // TODO
    }
    public boolean showFindDialog(String text, boolean showIme) {
        return false; // TODO
    }
    public static String findAddress(String addr) {
        return null; //TODO
    }
    public void clearMatches() {} //TODO

    public void documentHasImages(Message response) {} //TODO

    public void setWebViewClient(WebViewClient client) {} //TODO
    public void setDownloadListener(DownloadListener listener) {} //TODO
    public void setWebChromeClient(WebChromeClient client) {} //TODO

    public void addJavascriptInterface(Object obj, String interfaceName) {} //TODO
    public void removeJavascriptInterface(String interfaceName) {} //TODO

    public WebSettings getSettings() {
        return null; //TODO
    }

    public void flingScroll(int vx, int vy) {} //TODO
    public boolean canZoomIn() {
        return true; //TODO
    }
    public boolean canZoomOut() {
        return true; //TODO
    }
    public boolean zoomIn() {
        return true; //TODO
    }
    public boolean zoomOut() {
        return true; //TODO
    }

}
