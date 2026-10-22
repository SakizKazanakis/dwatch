package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.LinearInterpolator;

/**
 * The DKRNN mark, animated for the launch screen: the two platinum blades slide in and lock
 * together, a light band sweeps across the metal, and signal rings pulse out from the seam.
 */
public class MonolithMarkView extends View {
    private static final float MARK_W = 200f;
    private static final float MARK_H = 288f;
    private static final int[] PLATINUM = {0xFF7C8388, 0xFFC6CACB, 0xFFF4F5F3, 0xFF9AA1A6, 0xFFD9DCDB, 0xFF7C8388};
    private static final float[] PLATINUM_STOPS = {0f, 0.25f, 0.45f, 0.6f, 0.8f, 1f};

    private final Path left = new Path();
    private final Path right = new Path();
    private final Paint metal = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint ring = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Matrix shine = new Matrix();
    private final DecelerateInterpolator ease = new DecelerateInterpolator(2.2f);

    private float assemble;   // 0 → 1: blades slide in
    private float sweep;      // 0 → 1: light band crosses the mark
    private float pulse;      // 0 → 1, repeating: signal rings
    private boolean pulsing;
    private ValueAnimator pulseAnimator;

    public MonolithMarkView(Context context) {
        this(context, null);
    }

    public MonolithMarkView(Context context, AttributeSet attrs) {
        super(context, attrs);
        left.moveTo(21, 22);
        left.lineTo(79, 76);
        left.lineTo(79, 179);
        left.lineTo(90, 189);
        left.lineTo(90, 266);
        left.lineTo(82, 266);
        left.lineTo(21, 200);
        left.close();
        right.moveTo(179, 22);
        right.lineTo(121, 76);
        right.lineTo(121, 179);
        right.lineTo(110, 189);
        right.lineTo(110, 266);
        right.lineTo(118, 266);
        right.lineTo(179, 200);
        right.close();
        ring.setStyle(Paint.Style.STROKE);
        ring.setColor(0xFFCFE3EF);
    }

    /** Shows the finished mark without animating. */
    public void showStill() {
        assemble = 1f;
        sweep = 1f;
        invalidate();
    }

    /** Plays the full sequence and calls {@code done} once the blades have locked and the shine passed. */
    public void play(long durationMs, Runnable done) {
        ValueAnimator a = ValueAnimator.ofFloat(0f, 1f);
        a.setDuration(durationMs);
        a.setInterpolator(new LinearInterpolator());
        a.addUpdateListener(v -> {
            float t = (float) v.getAnimatedValue();
            assemble = ease.getInterpolation(Math.min(1f, t / 0.55f));
            sweep = Math.max(0f, Math.min(1f, (t - 0.45f) / 0.5f));
            if (t > 0.5f && !pulsing) {
                startPulse();
            }
            invalidate();
        });
        a.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                if (done != null) {
                    done.run();
                }
            }
        });
        a.start();
    }

    private void startPulse() {
        pulsing = true;
        pulseAnimator = ValueAnimator.ofFloat(0f, 1f);
        pulseAnimator.setDuration(1800);
        pulseAnimator.setRepeatCount(ValueAnimator.INFINITE);
        pulseAnimator.setInterpolator(new LinearInterpolator());
        pulseAnimator.addUpdateListener(v -> {
            pulse = (float) v.getAnimatedValue();
            invalidate();
        });
        pulseAnimator.start();
    }

    @Override
    protected void onDetachedFromWindow() {
        if (pulseAnimator != null) {
            pulseAnimator.cancel();
        }
        super.onDetachedFromWindow();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float w = getWidth();
        float h = getHeight();
        float scale = Math.min(w / MARK_W, h / MARK_H) * 0.62f;
        float cx = w / 2f;
        float cy = h / 2f;

        if (pulsing) {
            float maxR = Math.min(w, h) / 2f;
            for (int i = 0; i < 2; i++) {
                float p = (pulse + i * 0.5f) % 1f;
                ring.setStrokeWidth(Math.max(1f, 2.5f * (1f - p) * getResources().getDisplayMetrics().density));
                ring.setAlpha((int) (110 * (1f - p)));
                canvas.drawCircle(cx, cy, maxR * (0.42f + 0.58f * p), ring);
            }
        }

        // Light band position: starts off the left edge, ends off the right edge.
        LinearGradient gradient = new LinearGradient(0, 0, MARK_W, MARK_H, PLATINUM, PLATINUM_STOPS, Shader.TileMode.MIRROR);
        shine.setTranslate((sweep - 0.5f) * MARK_W * 1.6f, 0);
        gradient.setLocalMatrix(shine);
        metal.setShader(gradient);
        metal.setAlpha((int) (255 * Math.min(1f, assemble * 1.4f)));

        float slide = (1f - assemble) * MARK_W * 0.55f;
        canvas.save();
        canvas.translate(cx - MARK_W * scale / 2f, cy - MARK_H * scale / 2f);
        canvas.scale(scale, scale);
        canvas.save();
        canvas.translate(-slide, 0);
        canvas.drawPath(left, metal);
        canvas.restore();
        canvas.save();
        canvas.translate(slide, 0);
        canvas.drawPath(right, metal);
        canvas.restore();
        canvas.restore();
    }
}
