package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.PowerManager;
import android.provider.Settings;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.core.content.ContextCompat;
import androidx.core.content.res.ResourcesCompat;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import nodomain.freeyourgadget.gadgetbridge.GBApplication;
import nodomain.freeyourgadget.gadgetbridge.R;

/**
 * First start: the launch sequence, then one screen that switches on everything the watch link
 * needs. Replaces the upstream welcome pages and permission list.
 */
public class MonolithIntroActivity extends AppCompatActivity {
    static final String PREF_DONE = "monolith_intro_done";
    private static final String PREF_ASKED_LISTENER = "monolith_asked_listener";

    private LinearLayout rows;
    private final List<Step> steps = new ArrayList<>();

    private abstract class Step {
        final String title;
        final String subtitle;
        TextView subtitleView;
        TextView status;

        Step(String title, String subtitle) {
            this.title = title;
            this.subtitle = subtitle;
        }

        abstract boolean done();

        abstract void run();

        String hint() {
            return subtitle;
        }
    }

    private class PermissionStep extends Step {
        private final String[] permissions;
        private final int code;

        PermissionStep(String title, String subtitle, int code, String... permissions) {
            super(title, subtitle);
            this.code = code;
            this.permissions = declared(permissions);
        }

        @Override
        boolean done() {
            for (String p : permissions) {
                if (ContextCompat.checkSelfPermission(MonolithIntroActivity.this, p) != PackageManager.PERMISSION_GRANTED) {
                    return false;
                }
            }
            return true;
        }

        @Override
        void run() {
            if (permissions.length > 0) {
                ActivityCompat.requestPermissions(MonolithIntroActivity.this, permissions, code);
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setTheme(R.style.MonolithTheme);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.monolith_intro);
        rows = findViewById(R.id.monolith_intro_rows);

        List<String> link = new ArrayList<>(Arrays.asList(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            link.add(Manifest.permission.BLUETOOTH_SCAN);
            link.add(Manifest.permission.BLUETOOTH_CONNECT);
        }
        steps.add(new PermissionStep("Связь с часами", "Bluetooth и геопозиция: поиск часов, координаты, погода", 1,
                link.toArray(new String[0])));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            steps.add(new PermissionStep("Уведомления Monolith", "Статус связи и сигналы от часов", 2,
                    Manifest.permission.POST_NOTIFICATIONS));
        }
        List<String> calls = new ArrayList<>(Arrays.asList(Manifest.permission.READ_PHONE_STATE,
                Manifest.permission.READ_CALL_LOG, Manifest.permission.READ_CONTACTS));
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            calls.add(Manifest.permission.ANSWER_PHONE_CALLS);
        }
        steps.add(new PermissionStep("Звонки на часах", "Имя звонящего и ответ с запястья", 3, calls.toArray(new String[0])));
        steps.add(new Step("Работа в фоне", "Связь не обрывается, когда экран выключен") {
            @Override
            boolean done() {
                PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
                return Build.VERSION.SDK_INT < Build.VERSION_CODES.M || pm.isIgnoringBatteryOptimizations(getPackageName());
            }

            @SuppressLint("BatteryLife")
            @Override
            void run() {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                    open(new Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:" + getPackageName())));
                }
            }
        });
        steps.add(new Step("Сообщения на часы", "Уведомления телефона приходят на часы") {
            @Override
            boolean done() {
                return NotificationManagerCompat.getEnabledListenerPackages(MonolithIntroActivity.this).contains(getPackageName());
            }

            @Override
            String hint() {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && prefs().getBoolean(PREF_ASKED_LISTENER, false)) {
                    return "Android закрывает этот доступ приложениям не из магазина. Нажми сюда, затем ⋮ справа вверху → «Разрешить ограниченные настройки» и вернись.";
                }
                return subtitle;
            }

            @Override
            void run() {
                boolean asked = prefs().getBoolean(PREF_ASKED_LISTENER, false);
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && asked) {
                    prefs().edit().putBoolean(PREF_ASKED_LISTENER, false).apply();
                    open(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + getPackageName())));
                } else {
                    prefs().edit().putBoolean(PREF_ASKED_LISTENER, true).apply();
                    open(new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS));
                }
            }
        });

        for (Step step : steps) {
            rows.addView(row(step));
        }
        findViewById(R.id.monolith_intro_continue).setOnClickListener(v -> finishIntro());

        View panel = findViewById(R.id.monolith_intro_panel);
        if (savedInstanceState == null) {
            panel.setVisibility(View.INVISIBLE);
            MonolithLaunch.play(this, () -> {
                panel.setVisibility(View.VISIBLE);
                MonolithLaunch.reveal((LinearLayout) panel);
            });
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        refresh();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        refresh();
    }

    private void refresh() {
        for (Step step : steps) {
            boolean done = step.done();
            step.status.setText(done ? "ГОТОВО" : "ВКЛЮЧИТЬ");
            step.status.setTextColor(done ? 0xFFCFE3EF : 0xFFE8E9E6);
            step.subtitleView.setText(done ? step.subtitle : step.hint());
        }
    }

    private View row(Step step) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setBackgroundResource(R.drawable.monolith_cell);
        int pad = MonolithLaunch.dp(this, 16);
        row.setPadding(pad, pad, pad, pad);

        LinearLayout text = new LinearLayout(this);
        text.setOrientation(LinearLayout.VERTICAL);
        TextView title = label(step.title, R.font.plex_sans, 15, 0xFFE8E9E6, 0f);
        step.subtitleView = label(step.subtitle, R.font.plex_mono, 11, 0xFF9AA3AB, 0.02f);
        step.subtitleView.setPadding(0, MonolithLaunch.dp(this, 4), 0, 0);
        text.addView(title);
        text.addView(step.subtitleView);
        row.addView(text, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f));

        step.status = label("", R.font.plex_mono, 10, 0xFFE8E9E6, 0.16f);
        step.status.setPadding(MonolithLaunch.dp(this, 12), 0, 0, 0);
        row.addView(step.status);

        row.setOnClickListener(v -> {
            if (!step.done()) {
                step.run();
            }
        });
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = MonolithLaunch.dp(this, 10);
        row.setLayoutParams(params);
        return row;
    }

    private TextView label(String text, int font, int sp, int color, float spacing) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTypeface(ResourcesCompat.getFont(this, font));
        view.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        view.setTextColor(color);
        view.setLetterSpacing(spacing);
        return view;
    }

    private void finishIntro() {
        GBApplication.getPrefs().getPreferences().edit()
                .putBoolean(PREF_DONE, true)
                .putBoolean("first_run", false)
                .putBoolean("permission_pestering", false)
                .apply();
        startActivity(new Intent(this, MonolithActivity.class).putExtra(MonolithActivity.EXTRA_NO_LAUNCH, true));
        if (GBApplication.app().getDeviceManager().getDevices().isEmpty()) {
            Intent discovery = MonolithActivity.discoveryIntent(this);
            if (discovery != null) {
                startActivity(discovery);
            }
        }
        finish();
    }

    private SharedPreferences prefs() {
        return GBApplication.getPrefs().getPreferences();
    }

    private void open(Intent intent) {
        try {
            startActivity(intent);
        } catch (Exception e) {
            startActivity(new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + getPackageName())));
        }
    }

    /** Only the permissions this build declares; asking for others fails silently and would never turn green. */
    private String[] declared(String[] wanted) {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), PackageManager.GET_PERMISSIONS);
            List<String> have = info.requestedPermissions == null ? new ArrayList<>() : Arrays.asList(info.requestedPermissions);
            List<String> out = new ArrayList<>();
            for (String p : wanted) {
                if (have.contains(p)) {
                    out.add(p);
                }
            }
            return out.toArray(new String[0]);
        } catch (PackageManager.NameNotFoundException e) {
            return wanted;
        }
    }
}
