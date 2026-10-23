package nodomain.freeyourgadget.gadgetbridge.monolith;

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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Pushes files into the filesystem of a SiFli based watch (HK8 Pro Max), the way Wearfit Pro
 * does for custom backgrounds: SiFli "serial transport" GATT characteristic, watchface
 * file-push commands, every file 4-byte aligned with a trailing CRC-32/MPEG-2.
 *
 * Opens its own GATT client next to Gadgetbridge's connection and runs blocking on the
 * calling thread, so call {@link #push} off the main thread.
 */
@SuppressLint("MissingPermission")
public class SifliFilePush {
    public static final int TYPE_FACE = 0;
    public static final int TYPE_CUSTOM = 3;

    private static final UUID SERIAL_DATA = UUID.fromString("00000000-0000-0200-6473-5f696c666973");
    private static final UUID WEARFIT_WRITE = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
    private static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");
    private static final int CATEGORY_WATCHFACE = 4;

    private static final int CMD_START = 0;
    private static final int CMD_FILE_START = 2;
    private static final int CMD_FILE_DATA = 4;
    private static final int CMD_FILE_END = 6;
    private static final int CMD_END = 8;
    private static final int CMD_LOSE_CHECK = 10;
    private static final int CMD_LOSE_CHECK_RSP = 11;
    private static final int CMD_INFO = 13;

    private static final long COMMAND_TIMEOUT_MS = 60_000;
    private static final long WRITE_TIMEOUT_MS = 5_000;
    private static final int MAX_SERIAL_PAYLOAD = 0xFFFF;
    private static final int MAX_RX_BYTES = 0xFFFF;
    private volatile boolean connected;

    public interface Listener {
        void onProgress(int percent);
    }

    public static class Entry {
        final String path;
        final byte[] data;

        /** @param path absolute path on the watch, e.g. "/dyn/resource/clock_bg.bin" */
        public Entry(String path, byte[] data) {
            this.path = path;
            this.data = data;
        }
    }

    private final Context context;
    private final String address;
    private final Listener listener;

    private final Object writeLock = new Object();
    private final BlockingQueue<Object> events = new LinkedBlockingQueue<>();
    private final BlockingQueue<byte[]> responses = new LinkedBlockingQueue<>();
    private final AtomicBoolean loseCheckAck = new AtomicBoolean();

    private BluetoothGatt gatt;
    private BluetoothGattCharacteristic serial;
    private BluetoothGattCharacteristic wearfitWrite;
    private int mtu = 23;
    private boolean writeDone;
    private int writeStatus;

    private byte[] rxBuffer;
    private int rxIndex;

    public SifliFilePush(Context context, String address, Listener listener) {
        this.context = context.getApplicationContext();
        this.address = address;
        this.listener = listener;
    }

    /** Sends the files; throws with a readable reason on failure. */
    public void push(List<Entry> files, int type) throws Exception {
        pushWithStyle(files, type);
    }

    /**
     * Tells the watch how to draw the clock over a custom background (Wearfit "new style"
     * command on its main channel). Location and font are the watch's own preset indices.
     */
    /** The older style command (Wearfit TransferOrder.sendStyle) for watches without the new one. */
    public static byte[] styleCommandOld(int r, int g, int b, int font, int location) {
        return new byte[]{(byte) 0xAB, 0, 7, (byte) 0xFF, (byte) 0x9C, (byte) r, (byte) g, (byte) b, (byte) font, (byte) location};
    }

    public static byte[] styleCommand(int r, int g, int b, int font, int location) {
        return new byte[]{(byte) 0xEA, 0, 9, 0, 13, 1, (byte) r, (byte) g, (byte) b, (byte) font, (byte) location, (byte) 0xFF};
    }

    /**
     * Wearfit's "custom dial" handshake on its main channel (TransferOrder.startSendPic with
     * everything zero but the state): 0 before the upload, 1 after success, 2 after failure.
     * The watch only lists the new background as a dial after state 1 and the style.
     */
    private static byte[] dialTransferState(int state) {
        return new byte[]{(byte) 0xAD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (byte) state};
    }

    /**
     * Pushes files as a custom dial: announces the transfer, uploads, then confirms it and sends
     * {@code style} on the Wearfit write characteristic, as Wearfit Pro does.
     */
    public void pushWithStyle(List<Entry> files, int type, byte[]... styles) throws Exception {
        byte[][] style = styles.length > 0 ? styles : null;
        List<byte[]> payloads = new ArrayList<>();
        long total = 0;
        for (Entry e : files) {
            byte[] data = type == TYPE_CUSTOM ? withCrc(align4(e.data)) : e.data;
            payloads.add(data);
            total += data.length;
        }
        // This gate is intentionally non-constant so the upload implementation remains compilable.
        if (!isUploadProtocolVerified()) {
            throw new UnsupportedOperationException("Передача файлов заблокирована до проверки протокола HK8 3407");
        }
        try {
            connect();
            if (style != null) {
                if (wearfitWrite == null) {
                    throw new Exception("на часах нет командного канала Wearfit");
                }
                command(dialTransferState(0));
            }
            try {
                run(files, payloads, type, total);
            } catch (Exception e) {
                if (style != null) {
                    try {
                        command(dialTransferState(2));
                    } catch (Exception ignored) {
                        // Already failing; the watch drops the transfer on its own.
                    }
                }
                throw e;
            }
            if (style != null) {
                command(dialTransferState(1));
                for (byte[] s : style) {
                    Thread.sleep(300);
                    command(s);
                }
            }
        } finally {
            close();
        }
    }

    private static boolean isUploadProtocolVerified() {
        return false; // Must be backed by captured HK8 3407 protocol evidence before enabling.
    }

    private void run(List<Entry> files, List<byte[]> payloads, int type, long total) throws Exception {
        // Total start: type, protocol flag 2, total length.
        ByteBuffer start = le(11);
        start.putShort((short) CMD_START).putShort((short) 7).putShort((short) type).put((byte) 2).putInt((int) total);
        send(start.array());
        byte[] rsp = await(CMD_START + 1);
        check(rsp, "старт");
        int remoteVersion = 0;
        int blockLength = 0;
        if (rsp.length >= 14) {
            remoteVersion = u16(rsp, 6);
            blockLength = u16(rsp, 8);
        }

        if (remoteVersion >= 1) {
            int blocks = 0;
            for (byte[] p : payloads) {
                blocks += blockLength > 0 ? (p.length + blockLength - 1) / blockLength : 0;
            }
            ByteBuffer info;
            if (remoteVersion >= 3) {
                info = le(40);
                info.putShort((short) CMD_INFO).putShort((short) 36).putInt(blocks).put(md5Hex(payloads));
            } else {
                info = le(8);
                info.putShort((short) CMD_INFO).putShort((short) 4).putInt(blocks);
            }
            send(info.array());
            check(await(CMD_INFO + 1), "проверка места");
        }

        int partLen = mtu <= 23 ? 1024 : 4096;
        long sent = 0;
        for (int f = 0; f < files.size(); f++) {
            byte[] name = files.get(f).path.getBytes(StandardCharsets.UTF_8);
            byte[] data = payloads.get(f);

            ByteBuffer fileStart = le(10 + name.length);
            // Body: file length (u32), name length (u16), name.
            fileStart.putShort((short) CMD_FILE_START).putShort((short) (name.length + 6)).putInt(data.length).putShort((short) name.length).put(name);
            send(fileStart.array());
            check(await(CMD_FILE_START + 1), "файл");

            int count = (data.length + partLen - 1) / partLen;
            int retries = 0;
            for (int i = 0; i < count; i++) {
                int off = i * partLen;
                int len = Math.min(partLen, data.length - off);
                ByteBuffer part = le(8 + len);
                part.putShort((short) CMD_FILE_DATA).putShort((short) (len + 4)).putInt(i + 1).put(data, off, len);
                send(part.array());
                byte[] dataRsp = await(CMD_FILE_DATA + 1);
                int result = dataRsp.length >= 4 ? s16(dataRsp, 2) : -1;
                if (result == 4 || result == 7) {
                    if (++retries > 5) {
                        throw new Exception("часы не принимают данные (" + result + ")");
                    }
                    if (dataRsp.length >= 8) {
                        // Watch asks to continue from its expected (1-based) index.
                        i = Math.max(0, i32(dataRsp, 4) - 1) - 1;
                    } else {
                        // Busy: send the same part again a moment later.
                        Thread.sleep(300);
                        i--;
                    }
                    continue;
                }
                if (result != 0) {
                    throw new Exception("часы отклонили данные (" + result + ")");
                }
                retries = 0;
                if (listener != null) {
                    listener.onProgress((int) ((sent + off + len) * 100 / total));
                }
            }
            sent += data.length;

            ByteBuffer fileEnd = le(4);
            fileEnd.putShort((short) CMD_FILE_END).putShort((short) 0);
            send(fileEnd.array());
            check(await(CMD_FILE_END + 1), "конец файла");
        }

        ByteBuffer end = le(4);
        end.putShort((short) CMD_END).putShort((short) 0);
        send(end.array());
        check(await(CMD_END + 1), "завершение");
    }

    // --- GATT ---------------------------------------------------------------------------

    private final BluetoothGattCallback callback = new BluetoothGattCallback() {
        @Override
        public void onConnectionStateChange(BluetoothGatt g, int status, int newState) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                events.add(new Exception("GATT connection status " + status));
                return;
            }
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                connected = true;
                // The link is usually already up (Gadgetbridge holds it); some stacks refuse a second MTU request.
                if (!g.requestMtu(247)) {
                    g.discoverServices();
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                connected = false;
                events.add(new Exception("связь с часами потеряна"));
                synchronized (writeLock) {
                    writeStatus = -1;
                    writeDone = true;
                    writeLock.notifyAll();
                }
            }
        }

        @Override
        public void onMtuChanged(BluetoothGatt g, int newMtu, int status) {
            if (status == BluetoothGatt.GATT_SUCCESS && newMtu >= 23) {
                mtu = newMtu;
            } else {
                mtu = 23;
            }
            if (!g.discoverServices()) events.add(new Exception("GATT discoverServices failed"));
        }

        @Override
        public void onServicesDiscovered(BluetoothGatt g, int status) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                events.add(new Exception("GATT service discovery status " + status));
                return;
            }
            for (BluetoothGattService service : g.getServices()) {
                for (BluetoothGattCharacteristic c : service.getCharacteristics()) {
                    if (SERIAL_DATA.equals(c.getUuid())) {
                        serial = c;
                    } else if (WEARFIT_WRITE.equals(c.getUuid())) {
                        wearfitWrite = c;
                    }
                }
            }
            if (serial == null) {
                events.add(new Exception("на часах нет сервиса загрузки SiFli"));
                return;
            }
            if (!g.setCharacteristicNotification(serial, true)) {
                events.add(new Exception("Не удалось включить уведомления SiFli"));
                return;
            }
            BluetoothGattDescriptor d = serial.getDescriptor(CCCD);
            if (d == null) {
                events.add(new Exception("Отсутствует CCCD 0x2902 для SiFli"));
                return;
            }
            d.setValue(BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE);
            if (!g.writeDescriptor(d)) events.add(new Exception("CCCD write failed"));
        }

        @Override
        public void onDescriptorWrite(BluetoothGatt g, BluetoothGattDescriptor d, int status) {
            if (!CCCD.equals(d.getUuid()) || !SERIAL_DATA.equals(d.getCharacteristic().getUuid()) || status != BluetoothGatt.GATT_SUCCESS) {
                events.add(new Exception("CCCD write callback invalid: " + status));
            } else {
                events.add("ready");
            }
        }

        @Override
        public void onCharacteristicWrite(BluetoothGatt g, BluetoothGattCharacteristic c, int status) {
            synchronized (writeLock) {
                writeStatus = status;
                writeDone = true;
                writeLock.notifyAll();
            }
        }

        @Override
        public void onCharacteristicChanged(BluetoothGatt g, BluetoothGattCharacteristic c) {
            if (SERIAL_DATA.equals(c.getUuid())) {
                receive(c.getValue());
            }
        }
    };

    private void connect() throws Exception {
        BluetoothAdapter adapter = BluetoothAdapter.getDefaultAdapter();
        if (adapter == null || !adapter.isEnabled()) {
            throw new Exception("Bluetooth выключен");
        }
        BluetoothDevice device = adapter.getRemoteDevice(address);
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE);
        Object event = events.poll(6, TimeUnit.SECONDS);
        if (event == null && gatt != null) {
            // No MTU answer: go on with the default packet size.
            gatt.discoverServices();
            event = events.poll(30, TimeUnit.SECONDS);
        }
        if (event == null) {
            throw new Exception("часы не ответили");
        }
        if (event instanceof Exception) {
            throw (Exception) event;
        }
    }

    private void close() {
        if (gatt != null) {
            // Only drop our client; disconnect() could take Gadgetbridge's link down with it.
            gatt.close();
            gatt = null;
        }
    }

    /** A command on the Wearfit main channel, with a response when the characteristic offers one. */
    private void command(byte[] value) throws Exception {
        boolean acked = (wearfitWrite.getProperties() & BluetoothGattCharacteristic.PROPERTY_WRITE) != 0;
        write(wearfitWrite, value, acked ? BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT : BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
    }

    private void write(BluetoothGattCharacteristic c, byte[] value, int writeType) throws Exception {
        synchronized (writeLock) {
            writeDone = false;
            c.setWriteType(writeType);
            c.setValue(value);
            if (gatt == null || !gatt.writeCharacteristic(c)) {
                throw new Exception("не удалось записать в часы");
            }
            long deadline = System.currentTimeMillis() + WRITE_TIMEOUT_MS;
            while (!writeDone) {
                long left = deadline - System.currentTimeMillis();
                if (left <= 0) {
                    throw new Exception("часы не приняли пакет");
                }
                writeLock.wait(left);
            }
            if (writeStatus != BluetoothGatt.GATT_SUCCESS) {
                throw new Exception("ошибка записи (" + writeStatus + ")");
            }
        }
    }

    /** Serial transport framing: [category, flag, len16] then continuation packets [category, flag]. */
    private void send(byte[] message) throws Exception {
        int max = Math.min(mtu - 3, 244);
        int len = message.length;
        if (max < 5 || len > MAX_SERIAL_PAYLOAD) throw new Exception("Недопустимый размер BLE пакета: " + len);
        if (len + 4 <= max) {
            ByteBuffer one = le(len + 4);
            one.put((byte) CATEGORY_WATCHFACE).put((byte) 0).putShort((short) len).put(message);
            write(serial, one.array(), BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
            return;
        }
        ByteBuffer first = le(max);
        first.put((byte) CATEGORY_WATCHFACE).put((byte) 1).putShort((short) len).put(message, 0, max - 4);
        write(serial, first.array(), BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
        int off = max - 4;
        while (off < len) {
            int chunk = Math.min(max - 2, len - off);
            boolean last = off + chunk == len;
            ByteBuffer next = le(chunk + 2);
            next.put((byte) CATEGORY_WATCHFACE).put((byte) (last ? 3 : 2)).put(message, off, chunk);
            write(serial, next.array(), BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE);
            off += chunk;
        }
    }

    private void receive(byte[] v) {
        if (v == null || v.length < 2) {
            return;
        }
        int category = v[0];
        int flag = v[1];
        if (flag == 0 && v.length >= 4) {
            int len = u16(v, 2);
            if (len != v.length - 4 || len > MAX_RX_BYTES) return;
            byte[] out = new byte[len];
            System.arraycopy(v, 4, out, 0, len);
            deliver(category, out);
        } else if (flag == 1 && v.length >= 4) {
            int claimedLength = u16(v, 2);
            if (claimedLength > MAX_RX_BYTES || claimedLength < v.length - 4) {
                rxBuffer = null;
                return;
            }
            rxBuffer = new byte[claimedLength];
            rxIndex = 0;
            append(v, 4);
        } else if ((flag == 2 || flag == 3) && rxBuffer != null) {
            append(v, 2);
            if (flag == 3) {
                if (rxIndex != rxBuffer.length) {
                    rxBuffer = null;
                    return;
                }
                deliver(category, rxBuffer);
                rxBuffer = null;
            }
        }
    }

    private void append(byte[] v, int from) {
        int n = Math.min(v.length - from, rxBuffer.length - rxIndex);
        System.arraycopy(v, from, rxBuffer, rxIndex, n);
        rxIndex += n;
    }

    private void deliver(int category, byte[] payload) {
        if (category != CATEGORY_WATCHFACE || payload.length < 2) {
            return;
        }
        if (u16(payload, 0) == CMD_LOSE_CHECK) {
            // Watch noticed a gap: acknowledge it from the sending thread and treat it like a data response.
            loseCheckAck.set(true);
            byte[] asData = payload.clone();
            asData[0] = CMD_FILE_DATA + 1;
            asData[1] = 0;
            if (payload.length >= 4 && s16(payload, 2) == 8) {
                events.add(new Exception("часы прервали загрузку"));
            }
            responses.add(asData);
            return;
        }
        responses.add(payload);
    }

    private byte[] await(int command) throws Exception {
        long deadline = System.currentTimeMillis() + COMMAND_TIMEOUT_MS;
        while (true) {
            if (loseCheckAck.getAndSet(false)) {
                ByteBuffer ack = le(4);
                ack.putShort((short) CMD_LOSE_CHECK_RSP).putShort((short) 0);
                send(ack.array());
            }
            Object failure = events.poll();
            if (failure instanceof Exception) {
                throw (Exception) failure;
            }
            long left = deadline - System.currentTimeMillis();
            if (left <= 0) {
                throw new Exception("часы не ответили на команду " + (command - 1));
            }
            byte[] rsp = responses.poll(Math.min(left, 500), TimeUnit.MILLISECONDS);
            if (rsp != null && u16(rsp, 0) == command) {
                return rsp;
            }
        }
    }

    // --- helpers ------------------------------------------------------------------------

    private static void check(byte[] rsp, String step) throws Exception {
        int result = rsp.length >= 4 ? s16(rsp, 2) : -1;
        if (result != 0) {
            throw new Exception("часы отказали на шаге «" + step + "» (" + result + ")");
        }
    }

    private static byte[] align4(byte[] data) {
        int pad = (4 - data.length % 4) % 4;
        if (pad == 0) {
            return data;
        }
        byte[] out = new byte[data.length + pad];
        System.arraycopy(data, 0, out, 0, data.length);
        return out;
    }

    private static byte[] withCrc(byte[] data) {
        byte[] out = new byte[data.length + 4];
        System.arraycopy(data, 0, out, 0, data.length);
        ByteBuffer.wrap(out, data.length, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(crc32Mpeg2(data));
        return out;
    }

    static int crc32Mpeg2(byte[] data) {
        int crc = 0xFFFFFFFF;
        for (byte b : data) {
            crc ^= (b & 0xFF) << 24;
            for (int i = 0; i < 8; i++) {
                crc = (crc & 0x80000000) != 0 ? (crc << 1) ^ 0x04C11DB7 : crc << 1;
            }
        }
        return crc;
    }

    private static byte[] md5Hex(List<byte[]> payloads) throws Exception {
        MessageDigest md = MessageDigest.getInstance("MD5");
        for (byte[] p : payloads) {
            md.update(p);
        }
        StringBuilder sb = new StringBuilder();
        for (byte b : md.digest()) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString().getBytes(StandardCharsets.US_ASCII);
    }

    private static ByteBuffer le(int size) {
        return ByteBuffer.allocate(size).order(ByteOrder.LITTLE_ENDIAN);
    }

    private static int u16(byte[] b, int off) {
        return (b[off] & 0xFF) | (b[off + 1] & 0xFF) << 8;
    }

    private static int s16(byte[] b, int off) {
        return (short) u16(b, off);
    }

    private static int i32(byte[] b, int off) {
        return ByteBuffer.wrap(b, off, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
    }
}
