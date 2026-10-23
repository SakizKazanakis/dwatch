package nodomain.freeyourgadget.gadgetbridge.monolith;

import android.Manifest;
import android.annotation.SuppressLint;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCallback;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothProfile;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Looper;
import android.os.SystemClock;

import androidx.core.content.ContextCompat;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Read-only, one-shot GATT service discovery against the user's selected watch.
 * Records only this GATT client's connection and discovery events; it CANNOT
 * sniff Wearfit Pro packets or identify unexposed hardware registers.
 * Does not read or write characteristic values, subscribe, negotiate MTU, or issue OTA commands.
 */
@SuppressLint("MissingPermission")
public final class MonolithBleProbe {
    private static final long TIMEOUT_MS = 25_000;
    private final Context context;
    private final String address;
    private final long startedAt = SystemClock.elapsedRealtime();
    private final CountDownLatch completed = new CountDownLatch(1);
    private final AtomicBoolean finished = new AtomicBoolean();
    private final StringBuilder events = new StringBuilder();
    private volatile String profile = "GATT discovery did not finish.";

    public MonolithBleProbe(Context context, String address) {
        this.context = context.getApplicationContext();
        this.address = address;
    }

    private synchronized void log(String event) {
        events.append("+").append(SystemClock.elapsedRealtime() - startedAt)
                .append("ms ").append(event).append("\n");
    }

    private void finish(String text) {
        if (finished.compareAndSet(false, true)) {
            profile = text;
            completed.countDown();
        }
    }

    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt gatt, int status, int newState) {
            log("connection: status=" + status + " state=" + newState);
            if (finished.get()) return;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                finish("GATT connection failed: status " + status);
                return;
            }
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                if (!gatt.discoverServices()) {
                    finish("GATT discoverServices returned false");
                } else {
                    log("discoverServices requested (read-only)");
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                finish("GATT disconnected before discovery");
            }
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt gatt, int status) {
            log("service discovery: status=" + status);
            if (finished.get()) return;
            if (status != BluetoothGatt.GATT_SUCCESS) {
                finish("GATT service discovery failed: status " + status);
                return;
            }
            try {
                List<MonolithBleProfile.Service> services = new ArrayList<>();
                for (BluetoothGattService service : gatt.getServices()) {
                    List<MonolithBleProfile.Characteristic> chars = new ArrayList<>();
                    for (BluetoothGattCharacteristic c : service.getCharacteristics()) {
                        List<UUID> descriptors = new ArrayList<>();
                        for (BluetoothGattDescriptor descriptor : c.getDescriptors()) {
                            descriptors.add(descriptor.getUuid());
                        }
                        chars.add(new MonolithBleProfile.Characteristic(
                                c.getUuid(), c.getProperties(), descriptors));
                    }
                    services.add(new MonolithBleProfile.Service(service.getUuid(), chars));
                }
                log("discovered " + services.size() + " GATT services");
                finish(MonolithBleProfile.describe(services));
            } catch (RuntimeException e) {
                finish("Could not format GATT profile: " + e.getClass().getSimpleName());
            }
        }
    };

    /** Invoke ONLY from a worker thread. Always closes the separate GATT client. */
    public String probeBlocking() {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            throw new IllegalStateException("BLE diagnosis requires a worker thread");
        }
        BluetoothGatt gatt = null;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                    ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT)
                            != PackageManager.PERMISSION_GRANTED) {
                return makeReport("BLUETOOTH_CONNECT permission not granted.");
            }
            BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
            if (adapter == null || !adapter.isEnabled()) {
                return makeReport("Bluetooth is disabled or unavailable.");
            }
            log("diagnostic connect requested; no commands or data writes");
            BluetoothDevice device = adapter.getRemoteDevice(address);
            gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE);
            if (gatt == null) {
                return makeReport("connectGatt returned null.");
            }
            if (!completed.await(TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                finish("GATT discovery timed out after " + TIMEOUT_MS + "ms");
            }
            return makeReport(profile);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            return makeReport("BLE diagnosis interrupted.");
        } catch (SecurityException | IllegalArgumentException e) {
            return makeReport("BLE diagnosis rejected: " + e.getClass().getSimpleName());
        } finally {
            finished.set(true);
            if (gatt != null) {
                try {
                    gatt.close();
                    log("diagnostic GATT client closed");
                } catch (RuntimeException ignored) {
                    log("GATT client close failed");
                }
            }
        }
    }

    private synchronized String makeReport(String details) {
        return "MONOLITH — READ-ONLY BLE DIAGNOSTICS\n"
                + "Watch address: " + address + "\n"
                + "Scope: only this GATT client (not Wearfit Pro traffic)\n"
                + "No BLE characteristic writes, reads, notification subscriptions, or OTA.\n"
                + "Caution: parallel GATT clients may affect connection stability.\n\n"
                + "EVENTS\n" + events + "\nRESULT\n" + details + "\n";
    }
}
