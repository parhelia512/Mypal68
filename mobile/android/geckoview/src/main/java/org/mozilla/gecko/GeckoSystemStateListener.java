/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ContentResolver;
import android.content.Context;
import android.content.res.Configuration;
import android.database.ContentObserver;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.support.annotation.RequiresApi;
import android.util.Log;
import android.view.InputDevice;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.InputDeviceUtils;
import org.mozilla.gecko.util.ThreadUtils;

public class GeckoSystemStateListener
        implements InputManager.InputDeviceListener {
    private static final String LOGTAG = "SystemStateListener";

    private static final GeckoSystemStateListener listenerInstance = new GeckoSystemStateListener();

    private boolean mInitialized;
    private ContentObserver mContentObserver;
    private static Context sApplicationContext;
    private InputManager mInputManager;
    private static boolean sIsNightMode;

    public static GeckoSystemStateListener getInstance() {
        return listenerInstance;
    }

    private GeckoSystemStateListener() {
    }

    public synchronized void initialize(final Context context) {
        if (mInitialized) {
            Log.w(LOGTAG, "Already initialized!");
            return;
        }
        mInputManager = (InputManager)
            context.getSystemService(Context.INPUT_SERVICE);
        mInputManager.registerInputDeviceListener(listenerInstance, ThreadUtils.getUiHandler());

        sApplicationContext = context;
        ContentResolver contentResolver = sApplicationContext.getContentResolver();
        Uri animationSetting = Settings.System.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE);
        mContentObserver = new ContentObserver(new Handler(Looper.getMainLooper())) {
            @Override
            public void onChange(final boolean selfChange) {
                onDeviceChanged();
            }
        };
        contentResolver.registerContentObserver(animationSetting, false, mContentObserver);

        sIsNightMode = (sApplicationContext.getResources().getConfiguration().uiMode &
            Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;

        mInitialized = true;
    }

    public synchronized void shutdown() {
        if (!mInitialized) {
            Log.w(LOGTAG, "Already shut down!");
            return;
        }

        if (mInputManager != null) {
            Log.e(LOGTAG, "mInputManager should be valid!");
            return;
        }

        mInputManager.unregisterInputDeviceListener(listenerInstance);

        ContentResolver contentResolver = sApplicationContext.getContentResolver();
        contentResolver.unregisterContentObserver(mContentObserver);

        mInitialized = false;
        mInputManager = null;
        mContentObserver = null;
    }

    @RequiresApi(api = Build.VERSION_CODES.JELLY_BEAN_MR1)
    @WrapForJNI(calledFrom = "gecko")
    /**
     * For prefers-reduced-motion media queries feature.
     *
     * Uses `Settings.Global` which was introduced in API version 17.
     */
    private static boolean prefersReducedMotion() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1) {
            return false;
        }

        ContentResolver contentResolver = sApplicationContext.getContentResolver();

        return Settings.Global.getFloat(contentResolver,
                                        Settings.Global.ANIMATOR_DURATION_SCALE,
                                        1) == 0.0f;
    }

    @WrapForJNI(calledFrom = "gecko")
    private static void notifyPrefersReducedMotionChangedForTest() {
        ContentResolver contentResolver = sApplicationContext.getContentResolver();
        Uri animationSetting = Settings.System.getUriFor(Settings.Global.ANIMATOR_DURATION_SCALE);
        contentResolver.notifyChange(animationSetting, null);
    }

    @WrapForJNI(calledFrom = "gecko")
    /**
     * For prefers-color-scheme media queries feature.
     */
    private static boolean isNightMode() {
        return sIsNightMode;
    }

    public void updateNightMode(final int newUIMode) {
        boolean isNightMode = (newUIMode & Configuration.UI_MODE_NIGHT_MASK)
            == Configuration.UI_MODE_NIGHT_YES;
        if (isNightMode == sIsNightMode) {
            return;
        }
        sIsNightMode = isNightMode;
        onDeviceChanged();
    }

    @WrapForJNI(stubName = "OnDeviceChanged", calledFrom = "ui", dispatchTo = "gecko")
    private static native void nativeOnDeviceChanged();

    private static void onDeviceChanged() {
        if (GeckoThread.isStateAtLeast(GeckoThread.State.PROFILE_READY)) {
            nativeOnDeviceChanged();
        } else {
            GeckoThread.queueNativeCallUntil(
                    GeckoThread.State.PROFILE_READY, GeckoSystemStateListener.class,
                    "nativeOnDeviceChanged");
        }
    }

    private void notifyDeviceChanged(final int deviceId) {
        InputDevice device = InputDevice.getDevice(deviceId);
        if (device == null ||
            !InputDeviceUtils.isPointerTypeDevice(device)) {
            return;
        }
        onDeviceChanged();
    }

    @Override
    public void onInputDeviceAdded(final int deviceId) {
        notifyDeviceChanged(deviceId);
    }

    @Override
    public void onInputDeviceRemoved(final int deviceId) {
        // Call onDeviceChanged directly without checking device source types
        // since we can no longer get a valid `InputDevice` in the case of
        // device removal.
        onDeviceChanged();
    }

    @Override
    public void onInputDeviceChanged(final int deviceId) {
        notifyDeviceChanged(deviceId);
    }
}
