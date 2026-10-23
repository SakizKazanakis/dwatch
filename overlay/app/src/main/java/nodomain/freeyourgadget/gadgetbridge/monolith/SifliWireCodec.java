package nodomain.freeyourgadget.gadgetbridge.monolith;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Pure Java SiFli serial framing derived from one captured, successful OEM
 * HK8 PRO MAX / HK ONE / 3407 type-0 watchface transaction.
 *
 * This is a byte-level codec, not authorization to upload firmware or arbitrary files.
 * Characteristic writes remain disabled in SifliFilePush until separately validated.
 */
public final class SifliWireCodec {
    private static final int MAX_MESSAGE = 0xFFFF;
    private static final int MAX_FRAGMENT = 244;
    private static final int MAX_DATA = 4096;

    private SifliWireCodec() {}

    private static ByteBuffer le(int n) {
        return ByteBuffer.allocate(n).order(ByteOrder.LITTLE_ENDIAN);
    }

    public static byte[] start(int type, long total) {
        if (type < 0 || type > 0xFFFF || total <= 0 || total > 0xFFFFFFFFL) {
            throw new IllegalArgumentException("invalid transfer type or total");
        }
        return le(11).putShort((short) 0).putShort((short) 7)
                .putShort((short) type).put((byte) 2).putInt((int) total).array();
    }

    public static byte[] fileStart(String path, int byteCount) {
        if (path == null || !path.startsWith("/") || byteCount < 0) {
            throw new IllegalArgumentException("invalid remote path or length");
        }
        byte[] name = path.getBytes(StandardCharsets.UTF_8);
        if (name.length == 0 || name.length + 6 > 0xFFFF) {
            throw new IllegalArgumentException("remote path too long");
        }
        return le(10 + name.length).putShort((short) 2)
                .putShort((short) (name.length + 6)).putInt(byteCount)
                .putShort((short) name.length).put(name).array();
    }

    public static byte[] data(int index, byte[] file, int offset, int length) {
        if (file == null || index < 1 || offset < 0 || length <= 0
                || length > MAX_DATA || offset > file.length - length) {
            throw new IllegalArgumentException("invalid data block");
        }
        return le(8 + length).putShort((short) 4).putShort((short) (4 + length))
                .putInt(index).put(file, offset, length).array();
    }

    public static byte[] endFile() {
        return le(4).putShort((short) 6).putShort((short) 0).array();
    }

    public static byte[] end() {
        return le(4).putShort((short) 8).putShort((short) 0).array();
    }

    /** The first four bytes of each observed SiFli response are opcode and result, LE. */
    public static int responseStatus(byte[] response, int expectedOpcode) {
        if (response == null || response.length < 4
                || u16(response, 0) != expectedOpcode) {
            throw new IllegalArgumentException("missing, truncated or unexpected ACK");
        }
        return (short) u16(response, 2);
    }

    /** Strict acknowledgement validation for an offline-verified protocol. */
    public static int requireExpectedResponse(byte[] response, int expectedOpcode) {
        int status = responseStatus(response, expectedOpcode);
        if (status != 0) {
            throw new IllegalArgumentException("SiFli rejected command: " + status);
        }
        return status;
    }

    private static int u16(byte[] b, int offset) {
        return (b[offset] & 255) | ((b[offset + 1] & 255) << 8);
    }

    /**
     * Split one complete SiFli logical packet for ATT Write Command.
     * Limit of 244 bytes matches the recorded HK8 transmission even with MTU 517.
     */
    public static List<byte[]> frame(int category, byte[] message, int mtu) {
        if (category < 0 || category > 255 || message == null ||
                message.length > MAX_MESSAGE || mtu < 7) {
            throw new IllegalArgumentException("invalid SiFli message or MTU");
        }
        int max = Math.min(mtu - 3, MAX_FRAGMENT);
        if (max < 5) throw new IllegalArgumentException("invalid GATT capacity");
        List<byte[]> chunks = new ArrayList<>();
        if (message.length + 4 <= max) {
            chunks.add(le(message.length + 4).put((byte) category).put((byte) 0)
                    .putShort((short) message.length).put(message).array());
            return chunks;
        }
        int first = max - 4;
        chunks.add(le(max).put((byte) category).put((byte) 1)
                .putShort((short) message.length).put(message, 0, first).array());
        for (int offset = first; offset < message.length; ) {
            int size = Math.min(max - 2, message.length - offset);
            int flag = offset + size == message.length ? 3 : 2;
            chunks.add(le(size + 2).put((byte) category).put((byte) flag)
                    .put(message, offset, size).array());
            offset += size;
        }
        return chunks;
    }

    /**
     * Stateful defensive reassembly of one category of notifications.
     * Invalid frames throw and reset state instead of silently truncating responses.
     */
    public static final class Reassembler {
        private final int category;
        private ByteArrayOutputStream partial;
        private int expected;

        public Reassembler(int category) {
            if (category < 0 || category > 255) throw new IllegalArgumentException("bad category");
            this.category = category;
        }

        public byte[] accept(byte[] fragment) {
            if (fragment == null || fragment.length < 2 || (fragment[0] & 255) != category) {
                reset();
                throw new IllegalArgumentException("wrong category or short fragment");
            }
            int flag = fragment[1] & 255;
            if (flag == 0) {
                if (fragment.length < 4 || u16(fragment, 2) != fragment.length - 4) {
                    reset();
                    throw new IllegalArgumentException("incorrect single-frame length");
                }
                reset();
                return Arrays.copyOfRange(fragment, 4, fragment.length);
            }
            if (flag == 1) {
                if (fragment.length < 5) {
                    reset();
                    throw new IllegalArgumentException("short initial fragment");
                }
                expected = u16(fragment, 2);
                if (expected == 0 || fragment.length - 4 >= expected) {
                    reset();
                    throw new IllegalArgumentException("invalid multipart initial fragment");
                }
                partial = new ByteArrayOutputStream(expected);
                partial.write(fragment, 4, fragment.length - 4);
                return null;
            }
            if (flag != 2 && flag != 3 || partial == null || fragment.length < 3) {
                reset();
                throw new IllegalArgumentException("orphan or unknown continuation");
            }
            if (partial.size() + fragment.length - 2 > expected) {
                reset();
                throw new IllegalArgumentException("multipart overflow");
            }
            partial.write(fragment, 2, fragment.length - 2);
            if (flag == 3) {
                if (partial.size() != expected) {
                    reset();
                    throw new IllegalArgumentException("incomplete terminal fragment");
                }
                byte[] done = partial.toByteArray();
                reset();
                return done;
            }
            if (partial.size() >= expected) {
                reset();
                throw new IllegalArgumentException("missing terminal fragment");
            }
            return null;
        }

        public void reset() {
            partial = null;
            expected = 0;
        }
    }

    public static int crc32Mpeg2(byte[] value) {
        if (value == null) throw new IllegalArgumentException("null data");
        return crc32Mpeg2(value, value.length);
    }

    private static int crc32Mpeg2(byte[] value, int length) {
        int crc = 0xFFFFFFFF;
        for (int i = 0; i < length; i++) {
            crc ^= (value[i] & 255) << 24;
            for (int j = 0; j < 8; j++) {
                crc = (crc & 0x80000000) != 0 ? (crc << 1) ^ 0x04C11DB7 : crc << 1;
            }
        }
        return crc;
    }

    public static boolean hasCrcTrailer(byte[] value) {
        if (value == null || value.length < 4) return false;
        int n = value.length - 4;
        int observed = ByteBuffer.wrap(value, n, 4).order(ByteOrder.LITTLE_ENDIAN).getInt();
        return observed == crc32Mpeg2(value, n);
    }

    public static byte[] withCrc(byte[] body) {
        if (body == null) throw new IllegalArgumentException("null data");
        int padded = (body.length + 3) & ~3;
        byte[] out = Arrays.copyOf(body, padded + 4);
        int crc = crc32Mpeg2(out, padded);
        ByteBuffer.wrap(out, padded, 4).order(ByteOrder.LITTLE_ENDIAN).putInt(crc);
        return out;
    }
}
