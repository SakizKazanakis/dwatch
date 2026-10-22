package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.LinearGradient;
import android.graphics.Shader;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.core.content.res.ResourcesCompat;

import nodomain.freeyourgadget.gadgetbridge.R;

/** Full-screen launch sequence laid over an activity: the mark assembles, the wordmark settles, then it all fades away. */
final class MonolithLaunch {
    private MonolithLaunch() {
    }

    static void play(Activity activity, Runnable after) {
        ViewGroup content = activity.findViewById(android.R.id.content);
        FrameLayout overlay = new FrameLayout(activity);
        overlay.setBackgroundColor(0xFF0D0F11);
        overlay.setClickable(true);

        LinearLayout column = new LinearLayout(activity);
        column.setOrientation(LinearLayout.VERTICAL);
        column.setGravity(Gravity.CENTER_HORIZONTAL);

        MonolithMarkView mark = new MonolithMarkView(activity);
        column.addView(mark, new LinearLayout.LayoutParams(dp(activity, 220), dp(activity, 220)));

        TextView wordmark = new TextView(activity);
        wordmark.setText("MONOLITH");
        wordmark.setTypeface(ResourcesCompat.getFont(activity, R.font.michroma));
        wordmark.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        wordmark.setTextColor(0xFFE8E9E6);
        wordmark.setAlpha(0f);
        wordmark.setLetterSpacing(0.7f);
        column.addView(wordmark, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        TextView eyebrow = new TextView(activity);
        eyebrow.setText("DIKRANON");
        eyebrow.setTypeface(ResourcesCompat.getFont(activity, R.font.plex_mono));
        eyebrow.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        eyebrow.setLetterSpacing(0.5f);
        eyebrow.setTextColor(0xFF6F8794);
        eyebrow.setAlpha(0f);
        LinearLayout.LayoutParams eyebrowParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        eyebrowParams.topMargin = dp(activity, 12);
        column.addView(eyebrow, eyebrowParams);

        overlay.addView(column, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        content.addView(overlay, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        wordmark.post(() -> wordmark.getPaint().setShader(new LinearGradient(0, 0, wordmark.getWidth(), 0,
                new int[]{0xFFF4F5F3, 0xFF9AA1A6, 0xFFD9DCDB, 0xFF7C8388, 0xFFC6CACB},
                new float[]{0f, 0.28f, 0.5f, 0.72f, 1f}, Shader.TileMode.CLAMP)));

        mark.play(1500, () -> overlay.animate().alpha(0f).setStartDelay(450).setDuration(420).withEndAction(() -> {
            content.removeView(overlay);
            if (after != null) {
                after.run();
            }
        }));

        // Wordmark: letters close in from wide spacing while it fades up.
        ValueAnimator spacing = ValueAnimator.ofFloat(0.7f, 0.22f);
        spacing.setStartDelay(500);
        spacing.setDuration(1100);
        spacing.addUpdateListener(v -> wordmark.setLetterSpacing((float) v.getAnimatedValue()));
        spacing.start();
        wordmark.animate().alpha(1f).setStartDelay(500).setDuration(700).start();
        eyebrow.animate().alpha(1f).setStartDelay(1100).setDuration(500).start();
    }

    /** Content rises into place, child by child. */
    static void reveal(ViewGroup group) {
        for (int i = 0; i < group.getChildCount(); i++) {
            View child = group.getChildAt(i);
            child.setAlpha(0f);
            child.setTranslationY(dp(group.getContext(), 18));
            child.animate().alpha(1f).translationY(0f).setStartDelay(70L * i).setDuration(420).start();
        }
    }

    static int dp(android.content.Context context, int dp) {
        return Math.round(dp * context.getResources().getDisplayMetrics().density);
    }
}
