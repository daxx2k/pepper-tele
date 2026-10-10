package it.telepepper.quest;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;

/** Explicit panel/immersive transitions keep setup in the headset Home environment. */
final class HeadsetNavigation {
    private HeadsetNavigation() {}

    static void closeStudio(Activity activity) {
        activity.startActivity(new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        activity.finishAndRemoveTask();
    }

    static void openStudio(Activity activity) {
        Intent studio = new Intent(activity, TeleoperationActivity.class)
                .setAction(Intent.ACTION_MAIN).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        activity.startActivity(studio);
        activity.finishAndRemoveTask();
    }

    static void openSetupInHome(Activity activity) {
        Intent panel = new Intent(activity, LauncherActivity.class)
                .setAction(Intent.ACTION_MAIN)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
        PendingIntent pending = PendingIntent.getActivity(activity, 0, panel,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Intent home = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra("extra_launch_in_home_pending_intent", pending);
        activity.startActivity(home);
        activity.finishAndRemoveTask();
    }
}
