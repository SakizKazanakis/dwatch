package nodomain.freeyourgadget.gadgetbridge.monolith;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

/** Offline deterministic checks; fixtures derive from the owner's real HK8 3407 HCI capture. */
public final class SifliWireCodecJvmTest {
    private static int assertions;

    private static void assertTrue(boolean actual, String label) {
        assertions++;
        if (!actual) throw new AssertionError(label);
    }

    private static void equalsBytes(byte[] expected, byte[] actual, String label) {
        assertTrue(Arrays.equals(expected, actual), label + "\nexpected " + Arrays.toString(expected)
                + "\nactual " + Arrays.toString(actual));
    }

    private static byte[] hex(String s) {
        String clean = s.replace(" ", "");
        byte[] out = new byte[clean.length() / 2];
        for (int i = 0; i < out.length; i++) {
            out[i] = (byte) Integer.parseInt(clean.substring(i * 2, i * 2 + 2), 16);
        }
        return out;
    }

    private static void rejects(Runnable body, String label) {
        boolean rejected = false;
        try {
            body.run();
        } catch (IllegalArgumentException expected) {
            rejected = true;
        }
        assertTrue(rejected, label);
    }

    public static void main(String[] args) {
        byte[] start = SifliWireCodec.start(0, 97448);
        // Exact ATT characteristic value observed in OEM transfer (not ATT opcode/handle).
        byte[] knownStartFrame = hex("04 00 0b 00 00 00 07 00 00 00 02 a8 7c 01 00");
        equalsBytes(knownStartFrame, SifliWireCodec.frame(4, start, 247).get(0),
                "captured OEM start frame");
        equalsBytes(hex("00 00 07 00 00 00 02 a8 7c 01 00"), start, "start opcode/fields");

        byte[] knownStartAck = hex("01 00 00 00 00 28");
        assertTrue(SifliWireCodec.responseStatus(knownStartAck, 1) == 0, "captured start ACK");
        rejects(() -> SifliWireCodec.responseStatus(knownStartAck, 3), "reject wrong ACK opcode");
        rejects(() -> SifliWireCodec.responseStatus(hex("01 00 04"), 1), "reject truncated ACK");
        assertTrue(SifliWireCodec.responseStatus(hex("05 00 07 00 01 00 00 00"), 5) == 7,
                "preserve nonzero response code");

        byte[] path = "/ex/installer_wf/wf_clock23.so".getBytes(StandardCharsets.UTF_8);
        byte[] fileStart = SifliWireCodec.fileStart("/ex/installer_wf/wf_clock23.so", 3416);
        assertTrue(fileStart.length == 10 + path.length, "path field length");
        assertTrue(fileStart[0] == 2 && fileStart[1] == 0, "file start opcode");
        assertTrue((fileStart[8] & 0xFF) == path.length, "path length LE");

        byte[] sample = new byte[4096];
        for (int i = 0; i < sample.length; ++i) sample[i] = (byte) (i * 17 + 3);
        byte[] data = SifliWireCodec.data(1, sample, 0, sample.length);
        assertTrue(data.length == 4104, "4 KB data command length");
        assertTrue(data[0] == 4 && data[1] == 0 && data[4] == 1, "data command and 1-based index");
        List<byte[]> frames = SifliWireCodec.frame(4, data, 517);
        assertTrue(frames.size() > 2, "4096 B split");
        assertTrue(frames.get(0).length == 244 && frames.get(0)[1] == 1,
                "first fragment 244 bytes, flag 1");
        assertTrue(frames.get(1).length == 244 && frames.get(1)[1] == 2,
                "continuation 244 bytes, flag 2");
        assertTrue(frames.get(frames.size() - 1)[1] == 3, "last fragment flag 3");
        SifliWireCodec.Reassembler assembler = new SifliWireCodec.Reassembler(4);
        byte[] reconstructed = null;
        for (byte[] part : frames) {
            byte[] candidate = assembler.accept(part);
            if (candidate != null) reconstructed = candidate;
        }
        equalsBytes(data, reconstructed, "reassemble full data from exact frames");

        byte[] shortPayload = hex("08 00 00 00");
        equalsBytes(shortPayload, SifliWireCodec.frame(4, shortPayload, 23).get(0),
                "short payload framing", false);
        // A short write must include the 4-byte serial envelope.
        equalsBytes(hex("04 00 04 00 08 00 00 00"),
                SifliWireCodec.frame(4, shortPayload, 23).get(0), "short frame");

        assertTrue(SifliWireCodec.crc32Mpeg2("123456789".getBytes(StandardCharsets.US_ASCII))
                == 0x0376E6E7, "canonical MPEG-2 CRC vector");
        byte[] body = hex("00 01 02 03");
        byte[] withCrc = SifliWireCodec.withCrc(body);
        assertTrue(SifliWireCodec.hasCrcTrailer(withCrc), "CRC trailer matches");
        withCrc[0] ^= 1;
        assertTrue(!SifliWireCodec.hasCrcTrailer(withCrc), "CRC corruption detected");

        rejects(() -> SifliWireCodec.frame(4, new byte[65536], 517),
                "uint16 overflow rejected");
        rejects(() -> SifliWireCodec.frame(4, new byte[10], 3),
                "invalid mtu rejected");
        rejects(() -> new SifliWireCodec.Reassembler(4)
                .accept(hex("04 00 05 00 01 02")), "bad single-frame length");
        rejects(() -> new SifliWireCodec.Reassembler(4)
                .accept(hex("04 03 01 02")), "orphan terminal frame");
        SifliWireCodec.Reassembler badFinal = new SifliWireCodec.Reassembler(4);
        badFinal.accept(hex("04 01 05 00 01"));
        rejects(() -> badFinal.accept(hex("04 03 02")), "incomplete reassembly");
        assertTrue(SifliWireCodec.endFile().length == 4
                && SifliWireCodec.end().length == 4, "end packet lengths");
        System.out.println("SifliWireCodecJvmTest: " + assertions + " assertions PASS");
    }

    private static void equalsBytes(byte[] ignored, byte[] actual, String label, boolean unused) {
        assertTrue(actual.length == ignored.length + 4, label + " envelope length");
    }
}
