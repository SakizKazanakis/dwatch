package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;
import android.view.animation.LinearInterpolator;

/**
 * A steel connector line with glowing pulses travelling along it, like the
 * connectors on the DIKRANON site. Two bright pulses run phone → watch, one
 * dim pulse runs back.
 */
public class PulseWireView extends View {
    private static final int SEAM = 0xFF2A2F34;
    private static final int STEEL = 0xFF6F8794;
    private static final int SIGNAL = 0xFFCFE3EF;

    private final Paint linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint pulsePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint glowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ValueAnimator animator = ValueAnimator.ofFloat(0f, 1f);
    private float phase;
    private boolean active = true;

    public PulseWireView(Context context) {
        this(context, null);
    }

    public PulseWireView(Context context, AttributeSet attrs) {
        super(context, attrs);
        linePaint.setStrokeWidth(dp(1));
        glowPaint.setMaskFilter(new BlurMaskFilter(dp(4), BlurMaskFilter.Blur.NORMAL));
        setLayerType(LAYER_TYPE_SOFTWARE, glowPaint);
        animator.setDuration(2800);
        animator.setRepeatCount(ValueAnimator.INFINITE);
        animator.setInterpolator(new LinearInterpolator());
        animator.addUpdateListener(a -> {
            phase = (float) a.getAnimatedValue();
            invalidate();
        });
    }

    /** Pulses run only while the watch is connected. */
    public void setActive(boolean active) {
        this.active = active;
        if (active && isAttachedToWindow() && !animator.isStarted()) {
            animator.start();
        } else if (!active) {
            animator.cancel();
        }
        invalidate();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (active) {
            animator.start();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        animator.cancel();
        super.onDetachedFromWindow();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        linePaint.setShader(new LinearGradient(0, 0, w, 0,
                new int[]{SEAM, STEEL, SEAM}, null, Shader.TileMode.CLAMP));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        float cy = getHeight() / 2f;
        canvas.drawLine(0, cy, getWidth(), cy, linePaint);
        if (!active) {
            return;
        }
        drawPulse(canvas, cy, phase, SIGNAL);
        drawPulse(canvas, cy, (phase + 0.5f) % 1f, SIGNAL);
        drawPulse(canvas, cy, 1f - ((phase * 0.78f + 0.25f) % 1f), STEEL);
    }

    private void drawPulse(Canvas canvas, float cy, float t, int color) {
        // Fade in over the first 15% of the run and out over the last 15%.
        float alpha = Math.min(1f, Math.min(t, 1f - t) / 0.15f);
        float x = t * getWidth();
        float r = dp(2.5f);
        glowPaint.setColor(color);
        glowPaint.setAlpha((int) (alpha * 200));
        canvas.drawCircle(x, cy, r * 2, glowPaint);
        pulsePaint.setColor(color);
        pulsePaint.setAlpha((int) (alpha * 255));
        canvas.drawCircle(x, cy, r, pulsePaint);
    }

    private float dp(float v) {
        return v * getResources().getDisplayMetrics().density;
    }
}
