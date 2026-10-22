package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.Manifest;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.graphics.LinearGradient;
import android.graphics.Shader;
import android.location.Location;
import android.location.LocationManager;
import android.os.BatteryManager;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import java.text.NumberFormat;
import java.util.Calendar;
import java.util.List;
import java.util.Locale;

import nodomain.freeyourgadget.gadgetbridge.GBApplication;
import nodomain.freeyourgadget.gadgetbridge.R;
import nodomain.freeyourgadget.gadgetbridge.activities.ControlCenterv2;
import nodomain.freeyourgadget.gadgetbridge.activities.HeartRateUtils;
import nodomain.freeyourgadget.gadgetbridge.activities.PermissionsActivity;
import nodomain.freeyourgadget.gadgetbridge.activities.welcome.WelcomeActivity;
import nodomain.freeyourgadget.gadgetbridge.database.DBHandler;
import nodomain.freeyourgadget.gadgetbridge.devices.DeviceManager;
import nodomain.freeyourgadget.gadgetbridge.devices.SampleProvider;
import nodomain.freeyourgadget.gadgetbridge.impl.GBDevice;
import nodomain.freeyourgadget.gadgetbridge.model.ActivitySample;
import nodomain.freeyourgadget.gadgetbridge.model.DailyTotals;
import nodomain.freeyourgadget.gadgetbridge.model.RecordedDataTypes;
import nodomain.freeyourgadget.gadgetbridge.model.Weather;
import nodomain.freeyourgadget.gadgetbridge.model.WeatherSpec;
import nodomain.freeyourgadget.gadgetbridge.util.PermissionsUtils;
import nodomain.freeyourgadget.gadgetbridge.util.Prefs;

/** Monolith dashboard ("Пульт"): link state, both batteries, pulse, steps, air temperature, coordinates, quick actions. */
public class MonolithActivity extends AppCompatActivity {
    private static final int REQUEST_LOCATION = 41;

    private PulseWireView wire;
    private TextView linkState;
    private View linkDot;
    private TextView deviceName;
    private TextView deviceAddress;
    private TextView watchBattery;
    private ProgressBar watchBatteryBar;
    private TextView phoneBattery;
    private ProgressBar phoneBatteryBar;
    private TextView heartRate;
    private TextView steps;
    private TextView temperature;
    private TextView coords;
    private boolean finding;
    private GBDevice shownDevice;
    private Button find;

    private final BroadcastReceiver deviceReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            refreshDevice();
        }
    };

    private final BroadcastReceiver phoneBatteryReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            showPhoneBattery(intent);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        setTheme(R.style.MonolithTheme);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.monolith_activity);

        wire = findViewById(R.id.monolith_wire);
        linkState = findViewById(R.id.monolith_link_state);
        linkDot = findViewById(R.id.monolith_link_dot);
        deviceName = findViewById(R.id.monolith_device_name);
        deviceAddress = findViewById(R.id.monolith_device_address);
        watchBattery = findViewById(R.id.monolith_watch_battery);
        watchBatteryBar = findViewById(R.id.monolith_watch_battery_bar);
        phoneBattery = findViewById(R.id.monolith_phone_battery);
        phoneBatteryBar = findViewById(R.id.monolith_phone_battery_bar);
        heartRate = findViewById(R.id.monolith_heart_rate);
        steps = findViewById(R.id.monolith_steps);
        temperature = findViewById(R.id.monolith_temperature);
        coords = findViewById(R.id.monolith_coords);

        TextView wordmark = findViewById(R.id.monolith_wordmark);
        wordmark.post(() -> wordmark.getPaint().setShader(new LinearGradient(0, 0, wordmark.getWidth(), 0,
                new int[]{0xFFF4F5F3, 0xFF9AA1A6, 0xFFD9DCDB, 0xFF7C8388, 0xFFC6CACB},
                new float[]{0f, 0.28f, 0.5f, 0.72f, 1f}, Shader.TileMode.CLAMP)));

        find = findViewById(R.id.monolith_find);
        find.setOnClickListener(v -> {
            if (shownDevice == null) {
                return;
            }
            setFinding(!finding);
        });
        findViewById(R.id.monolith_sync).setOnClickListener(v -> {
            GBApplication.deviceService().onFetchRecordedData(RecordedDataTypes.TYPE_SYNC);
            Toast.makeText(this, "Синхронизация запущена", Toast.LENGTH_SHORT).show();
        });
        findViewById(R.id.monolith_settings).setOnClickListener(v ->
                startActivity(new Intent(this, ControlCenterv2.class)));
        coords.setOnClickListener(v -> requestLocation());

        if (savedInstanceState == null) {
            routeOnboarding();
        }
    }

    // Same dispatch as ControlCenterv2: welcome flow on first run, permissions afterwards.
    private void routeOnboarding() {
        Prefs prefs = GBApplication.getPrefs();
        if (prefs.getBoolean("first_run", true)) {
            startActivity(new Intent(this, WelcomeActivity.class));
        } else if (prefs.getBoolean("permission_pestering", true) && !PermissionsUtils.checkAllPermissions(this)) {
            startActivity(new Intent(this, PermissionsActivity.class));
        }
    }

    private void setFinding(boolean on) {
        finding = on;
        if (shownDevice != null) {
            GBApplication.deviceService(shownDevice).onFindDevice(on);
        }
        find.setText(on ? "Стоп" : "Найти часы");
    }

    @Override
    protected void onResume() {
        super.onResume();
        IntentFilter filter = new IntentFilter();
        filter.addAction(GBDevice.ACTION_DEVICE_CHANGED);
        filter.addAction(DeviceManager.ACTION_DEVICES_CHANGED);
        LocalBroadcastManager.getInstance(this).registerReceiver(deviceReceiver, filter);
        showPhoneBattery(registerReceiver(phoneBatteryReceiver, new IntentFilter(Intent.ACTION_BATTERY_CHANGED)));
        refreshDevice();
        refreshTemperature();
        refreshLocation();
    }

    @Override
    protected void onPause() {
        if (finding) {
            setFinding(false);
        }
        LocalBroadcastManager.getInstance(this).unregisterReceiver(deviceReceiver);
        unregisterReceiver(phoneBatteryReceiver);
        super.onPause();
    }

    private void refreshDevice() {
        List<GBDevice> devices = GBApplication.app().getDeviceManager().getDevices();
        GBDevice device = devices.isEmpty() ? null : devices.get(0);
        for (GBDevice d : devices) {
            if (d.isConnected()) {
                device = d;
                break;
            }
        }

        if (finding && device != shownDevice) {
            setFinding(false);
        }
        shownDevice = device;

        if (device == null) {
            deviceName.setText("Часы не добавлены");
            deviceAddress.setText("");
            setLink(false, "Нет устройства");
            watchBattery.setText("—");
            watchBatteryBar.setProgress(0);
            heartRate.setText("—");
            steps.setText("—");
            return;
        }

        deviceName.setText(device.getAliasOrName());
        deviceAddress.setText(device.getAddress());
        setLink(device.isConnected(), device.isConnected() ? "Связь" : device.getStateString(this));

        int level = device.getBatteryLevel(0);
        if (level >= 0) {
            watchBattery.setText(level + "%");
            watchBatteryBar.setProgress(level);
        } else {
            watchBattery.setText("—");
            watchBatteryBar.setProgress(0);
        }

        GBDevice forSteps = device;
        new Thread(() -> {
            String text;
            try {
                long count = DailyTotals.getDailyTotalsForDevice(forSteps, Calendar.getInstance()).getSteps();
                text = NumberFormat.getIntegerInstance(new Locale("ru")).format(count);
            } catch (Exception e) {
                text = "—";
            }
            String shown = text;
            String pulse = latestHeartRate(forSteps);
            runOnUiThread(() -> {
                steps.setText(shown);
                heartRate.setText(pulse);
            });
        }).start();
    }

    /** Last valid heart-rate value recorded in the past 24 hours, or a dash. */
    private static String latestHeartRate(GBDevice device) {
        int now = (int) (System.currentTimeMillis() / 1000);
        try (DBHandler db = GBApplication.acquireDB()) {
            SampleProvider<? extends ActivitySample> provider =
                    device.getDeviceCoordinator().getSampleProvider(device, db.getDaoSession());
            if (provider == null) {
                return "—";
            }
            List<? extends ActivitySample> samples = provider.getAllActivitySamples(now - 24 * 3600, now);
            for (int i = samples.size() - 1; i >= 0; i--) {
                int hr = samples.get(i).getHeartRate();
                if (HeartRateUtils.getInstance().isValidHeartRateValue(hr)) {
                    return String.valueOf(hr);
                }
            }
        } catch (Exception e) {
            // No database or no samples yet.
        }
        return "—";
    }

    /** Current air temperature from the weather Gadgetbridge last received (stored in kelvin). */
    private void refreshTemperature() {
        WeatherSpec spec = Weather.getInstance().getWeatherSpec();
        if (spec == null) {
            temperature.setText("—");
            return;
        }
        int celsius = spec.currentTemp - 273;
        temperature.setText((celsius > 0 ? "+" : "") + celsius + "°");
    }

    private void setLink(boolean connected, String label) {
        wire.setActive(connected);
        linkState.setText(label);
        int color = ContextCompat.getColor(this, connected ? R.color.monolith_signal : R.color.monolith_steel);
        linkState.setTextColor(color);
        linkDot.setBackgroundColor(color);
    }

    private void showPhoneBattery(Intent intent) {
        if (intent == null) {
            return;
        }
        int level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) {
            phoneBattery.setText("—");
            return;
        }
        int percent = Math.round(level * 100f / scale);
        phoneBattery.setText(percent + "%");
        phoneBatteryBar.setProgress(percent);
    }

    private boolean hasLocationPermission() {
        return ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
                || ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_COARSE_LOCATION) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestLocation() {
        if (!hasLocationPermission()) {
            ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION}, REQUEST_LOCATION);
        } else {
            refreshLocation();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQUEST_LOCATION) {
            refreshLocation();
        }
    }

    @SuppressWarnings("MissingPermission")
    private void refreshLocation() {
        if (!hasLocationPermission()) {
            coords.setText("Нажмите, чтобы разрешить геопозицию");
            return;
        }
        LocationManager lm = (LocationManager) getSystemService(Context.LOCATION_SERVICE);
        Location best = null;
        for (String provider : lm.getProviders(true)) {
            Location l = lm.getLastKnownLocation(provider);
            if (l != null && (best == null || l.getTime() > best.getTime())) {
                best = l;
            }
        }
        if (best == null) {
            coords.setText("Нет данных о местоположении");
            return;
        }
        coords.setText(String.format(Locale.US, "LAT  %.4f° %s\nLON  %.4f° %s",
                Math.abs(best.getLatitude()), best.getLatitude() >= 0 ? "N" : "S",
                Math.abs(best.getLongitude()), best.getLongitude() >= 0 ? "E" : "W"));
    }
}
