package it.telepepper.quest;

import android.app.Activity;
import android.view.WindowManager;
import android.widget.*;
import it.telepepper.ui.StudioStyle;

/** Compact, scrollable connection fields with a persistent entry action. */
final class ConnectionForm {
    static TextView label(StudioStyle ui, String value, int size, int color) {
        return ui.text(value, size >= 24 ? 22 : Math.min(size, 14), color, size >= 24);
    }
    static EditText field(StudioStyle ui, LinearLayout parent, String title, String value, boolean secret) {
        EditText edit = ui.field(parent, title, value, secret);
        TextView label = (TextView) parent.getChildAt(parent.getChildCount()-2);
        label.setTextSize(12);
        LinearLayout.LayoutParams space = (LinearLayout.LayoutParams) label.getLayoutParams();
        space.setMargins(0, ui.dp(8), 0, ui.dp(3)); label.setLayoutParams(space);
        edit.setTextSize(16); edit.setPadding(ui.dp(10), ui.dp(6), ui.dp(10), ui.dp(6));
        edit.getLayoutParams().height = ui.dp(44);
        return edit;
    }
    static void show(Activity activity, StudioStyle ui, LinearLayout root, ScrollView scroll, Button enter) {
        root.setPadding(ui.dp(16),ui.dp(14),ui.dp(16),ui.dp(14));
        LinearLayout panel = ui.column(); panel.setBackgroundColor(ui.background);
        panel.addView(scroll,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout footer=ui.column(); footer.setPadding(ui.dp(16),ui.dp(8),ui.dp(16),ui.dp(12));
        footer.setBackgroundColor(ui.background); enter.setTextSize(14);
        footer.addView(enter,new LinearLayout.LayoutParams(-1,ui.dp(48)));
        panel.addView(footer,new LinearLayout.LayoutParams(-1,-2));
        activity.getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE
                | WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN);
        activity.setContentView(panel);
    }
}
