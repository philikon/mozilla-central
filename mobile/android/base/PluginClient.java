/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.view.Surface;
import android.view.View;

public interface PluginClient {

    // These methods are invoked from the Gecko thread
    String[] getPluginDirectories();
    String getPluginPackage(String libName);
    void addPluginView(final View view,
                       final int x, final int y,
                       final int w, final int h,
                       final String metadata);
    void removePluginView(final View view);

    // Some plugins require Surfaces
     Surface createSurface();
     void destroySurface(Surface surface);
     void showSurface(Surface surface,
                      int x, int y,
                      int w, int h,
                      boolean inverted,
                      boolean blend,
                      String metadata);
     void hideSurface(Surface surface);

     void showPlugins();
     void hidePlugins(boolean hideLayers);
     void repositionPluginViews(boolean setVisible);
}
