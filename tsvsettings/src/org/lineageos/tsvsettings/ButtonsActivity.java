/*
 * SPDX-License-Identifier: Apache-2.0
 */
package org.lineageos.tsvsettings;

import android.os.Bundle;
import android.os.SystemProperties;

import androidx.fragment.app.FragmentActivity;
import androidx.preference.ListPreference;
import androidx.preference.PreferenceFragmentCompat;

/** Lets the user choose what the mic-mute button and privacy slider do (read by vendor tsv-buttons). */
public class ButtonsActivity extends FragmentActivity {

    static final String PROP_MUTE = "persist.sys.tsv.mute_action";
    static final String PROP_SLIDER = "persist.sys.tsv.slider_action";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getActionBar() != null) getActionBar().setDisplayHomeAsUpEnabled(true);
        if (savedInstanceState == null) {
            getSupportFragmentManager().beginTransaction()
                    .replace(android.R.id.content, new ButtonsFragment())
                    .commit();
        }
    }

    @Override
    public boolean onNavigateUp() {
        finish();
        return true;
    }

    public static class ButtonsFragment extends PreferenceFragmentCompat {
        @Override
        public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
            setPreferencesFromResource(R.xml.buttons, rootKey);
            bind("mute_action", PROP_MUTE);
            bind("slider_action", PROP_SLIDER);
        }

        private void bind(String key, String prop) {
            ListPreference pref = findPreference(key);
            if (pref == null) return;
            pref.setValue(SystemProperties.get(prop, "power"));
            pref.setSummaryProvider(ListPreference.SimpleSummaryProvider.getInstance());
            pref.setOnPreferenceChangeListener((p, value) -> {
                SystemProperties.set(prop, (String) value);
                return true;
            });
        }
    }
}
