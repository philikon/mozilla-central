/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.os.*;
import android.app.*;
import android.text.*;
import android.text.format.Time;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.util.*;

import org.json.JSONObject;

import org.mozilla.gecko.gfx.LayerController;
import org.mozilla.gecko.gfx.GeckoLayerClient;

public class Gecko implements GeckoEventListener {
    private static final String LOGTAG = "Gecko";
    public static final String ACTION_DEBUG = "org.mozilla.gecko.DEBUG";

    public static Gecko instance;
    private Activity mActivity;
    public Handler mMainHandler;
    private GeckoProfile mProfile;
    private GeckoThread mGeckoThread = null;
    public enum LaunchState {Launching, WaitForDebugger,
                             Launched, GeckoRunning, GeckoExiting};
    private LaunchState mLaunchState = LaunchState.Launching;
    private static LayerController mLayerController;
    private static GeckoLayerClient mLayerClient;

    // These methods are invoked on the Gecko thread
    public interface ChromeClient {
        void onReady();
        void restartApplication();
    }

    private ChromeClient mChromeClient;

    public Gecko(Activity activity, Handler mainHandler) {
        // There's only one singleton instance of Gecko. It can be referenced as
        // Gecko.instance.
        assert instance == null;
        instance = this;

        mActivity = activity;
        mMainHandler = mainHandler;

        GeckoAppShell.loadMozGlue();

        mLayerController = new LayerController(activity);
        /*
         * Hook a placeholder layer client up to the layer controller so that the user can pan
         * and zoom a cached screenshot of the previous page. This call will return null if
         * there is no cached screenshot; in that case, we have no choice but to display a
         * checkerboard.
         *
         * TODO: Fall back to a built-in screenshot of the Fennec Start page for a nice first-
         * run experience, perhaps?
         */
        mLayerClient = new GeckoLayerClient(activity);
    }

    void setChromeClient(ChromeClient client) {
        mChromeClient = client;
    }

    public Context getContext() {
        return mActivity;
    }

    public WindowManager getWindowManager() {
        return mActivity.getWindowManager();
    }

    public DisplayMetrics getDisplayMetrics() {
        DisplayMetrics dm = new DisplayMetrics();
        getWindowManager().getDefaultDisplay().getMetrics(dm);
        return dm;
    }

    public Handler getMainHandler() {
        return mMainHandler;
    }

    public void setProfile(GeckoProfile profile) {
        mProfile = profile;
    }

    public GeckoProfile getProfile() {
        // fall back to default profile if we didn't load a specific one
        if (mProfile == null) {
            mProfile = GeckoProfile.get(mActivity);
        }
        return mProfile;
    }

    public LayerController getLayerController() {
        return mLayerController;
    }

    public GeckoLayerClient getLayerClient() {
        return mLayerClient;
    }

    public void handleMessage(String event, JSONObject message) {
        if (event.equals("Gecko:Ready")) {
            mLayerController.setLayerClient(mLayerClient);

            mLayerController.getView().getTouchEventHandler().setOnTouchListener(new View.OnTouchListener() {
                public boolean onTouch(View view, MotionEvent event) {
                    if (event == null)
                        return true;
                    GeckoAppShell.sendEventToGecko(GeckoEvent.createMotionEvent(event));
                    return true;
                }
            });
            mChromeClient.onReady();
            setLaunchState(LaunchState.GeckoRunning);
            GeckoAppShell.sendPendingEventsToGecko();
            connectGeckoLayerClient();
        }
    }

    /*** Initialization ***/

    public boolean checkLaunchState(LaunchState checkState) {
        synchronized(mLaunchState) {
            return mLaunchState == checkState;
        }
    }

    void setLaunchState(LaunchState setState) {
        synchronized(mLaunchState) {
            mLaunchState = setState;
        }
    }

    // if mLaunchState is equal to checkState this sets mLaunchState to setState
    // and return true. Otherwise we return false.
    boolean checkAndSetLaunchState(LaunchState checkState, LaunchState setState) {
        synchronized(mLaunchState) {
            if (mLaunchState != checkState)
                return false;
            mLaunchState = setState;
            return true;
        }
    }

    private void connectGeckoLayerClient() {
        LayerController layerController = getLayerController();
        layerController.setLayerClient(mLayerClient);

        layerController.getView().getTouchEventHandler().setOnTouchListener(new View.OnTouchListener() {
            public boolean onTouch(View view, MotionEvent event) {
                if (event == null)
                    return true;
                GeckoAppShell.sendEventToGecko(GeckoEvent.createMotionEvent(event));
                return true;
            }
        });
    }

    public void initialize(Intent intent, String uri) {
        initialize(intent, uri, false);
    }

    public void initialize(Intent intent, String uri, boolean shouldRestoreSession) {
        String action = intent.getAction();
        mGeckoThread = new GeckoThread(intent, uri, shouldRestoreSession);
        GeckoAppShell.registerGeckoEventListener("Gecko:Ready", this);
        if (!ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.Launched)) {
            mGeckoThread.start();
        } else if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitForDebugger)) {
            mMainHandler.postDelayed(new Runnable() {
                public void run() {
                    Log.i(LOGTAG, "Launching from debug intent after 5s wait");
                    setLaunchState(LaunchState.Launching);
                    mGeckoThread.start();
                }
            }, 1000 * 5 /* 5 seconds */);
            Log.i(LOGTAG, "Intent : ACTION_DEBUG - waiting 5s before launching");
        }
    }

    public boolean didReceiveNewIntent(String action) {
        if (ACTION_DEBUG.equals(action) &&
            checkAndSetLaunchState(LaunchState.Launching, LaunchState.WaitForDebugger)) {
            mMainHandler.postDelayed(new Runnable() {
                public void run() {
                    Log.i(LOGTAG, "Launching from debug intent after 5s wait");
                    setLaunchState(LaunchState.Launching);
                    mGeckoThread.start();
                }
            }, 1000 * 5 /* 5 seconds */);
            Log.i(LOGTAG, "Intent : ACTION_DEBUG - waiting 5s before launching");
            return true;
        }
        return checkLaunchState(LaunchState.WaitForDebugger);
    }

    public void quit() {
        synchronized(mLaunchState) {
            if (mLaunchState == LaunchState.GeckoRunning)
                GeckoAppShell.notifyGeckoOfEvent(
                    GeckoEvent.createBroadcastEvent("Browser:Quit", null));
            else
                System.exit(0);
            mLaunchState = LaunchState.GeckoExiting;
        }
    }

    public boolean isReady() {
        return mLaunchState == LaunchState.GeckoRunning;
    }

    public void moveTaskToBack() {
        // XXX(robarnold): who calls this?
        GeckoApp.mAppContext.moveTaskToBack(true);
    }

    public void xreDidExit(boolean restartScheduled) {
        if (restartScheduled) {
            mChromeClient.restartApplication();
        } else {
            Log.i(LOGTAG, "we're done, good bye");
            GeckoApp.mAppContext.finish();
        }
    }

    public void setFullScreen(boolean fullscreen) {
        GeckoApp.mAppContext.setFullScreen(fullscreen);
    }

    public String showFilePickerForMimeType(String aMimeType) {
        return GeckoApp.mAppContext.showFilePicker(aMimeType);
    }

    public boolean showFilePickerForMimeType(String aMimeType, ActivityResultHandler handler) {
        return GeckoApp.mAppContext.showFilePicker(aMimeType, handler);
    }

    public void setRequestedOrientation(int orientation) {
        GeckoApp.mAppContext.setRequestedOrientation(orientation);
    }

    public void requestRender() {
        mLayerController.getView().requestRender();
    }

    public void enableCameraView() {
        GeckoApp.mAppContext.enableCameraView();
    }

    public void disableCameraView() {
        GeckoApp.mAppContext.disableCameraView();
    }

    /*** Plugins ***/

    public static class DefaultPluginClient implements PluginClient {
        public String[] getPluginDirectories() { return new String[0]; }
        public String getPluginPackage(String libName) { return ""; }
        public void addPluginView(final View view,
                                  final int x, final int y,
                                  final int w, final int h,
                                  final String metadata) {}
        public void removePluginView(final View view) {}

        // Some plugins require Surfaces
        public Surface createSurface() { return null; }
        public void destroySurface(Surface surface) {}
        public void showSurface(Surface surface,
                                int x, int y,
                                int w, int h,
                                boolean inverted,
                                boolean blend,
                                String metadata) {}
        public void hideSurface(Surface surface) {}

        public void showPlugins() {}
        public void hidePlugins(boolean hideLayers) {}
        public void repositionPluginViews(boolean setVisible) {}
    }
}
