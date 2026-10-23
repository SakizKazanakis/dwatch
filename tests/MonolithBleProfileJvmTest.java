package nodomain.freeyourgadget.gadgetbridge.monolith;

import java.util.Arrays;
import java.util.Collections;
import java.util.UUID;

/** Standalone JVM tests: no Android runtime and no connected watch required. */
public final class MonolithBleProfileJvmTest {
    private static void require(boolean ok, String message) {
        if (!ok) throw new AssertionError(message);
    }

    public static void main(String[] args) {
        String empty = MonolithBleProfile.describe(Collections.emptyList());
        require(empty.contains("SiFli transport: NOT FOUND"), "SiFli absent");
        require(empty.contains("Wearfit command: NOT FOUND"), "Wearfit absent");
        require(empty.contains("firmware write: not verified"), "must not infer firmware access");

        MonolithBleProfile.Characteristic sf = new MonolithBleProfile.Characteristic(
                UUID.fromString("00000000-0000-0200-6473-5f696c666973"),
                0x1E,
                Collections.singletonList(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
        );
        MonolithBleProfile.Characteristic wf = new MonolithBleProfile.Characteristic(
                UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e"),
                0x0E, Collections.emptyList()
        );
        MonolithBleProfile.Service sfService = new MonolithBleProfile.Service(
                UUID.fromString("00000000-0000-0000-6473-5f696c666973"),
                Collections.singletonList(sf)
        );
        MonolithBleProfile.Service wfService = new MonolithBleProfile.Service(
                UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e"),
                Collections.singletonList(wf)
        );
        String matching = MonolithBleProfile.describe(Arrays.asList(sfService, wfService));
        require(matching.contains("SiFli transport: AVAILABLE"), "SiFli available");
        require(matching.contains("Wearfit command: AVAILABLE"), "Wearfit available");
        require(matching.contains("CCCD: present"), "descriptor present");
        require(matching.contains("READ|WRITE_NO_RESPONSE|WRITE|NOTIFY"), "properties mapping");
        require(matching.contains("firmware write: not verified"), "must not equate GATT with firmware writes");

        MonolithBleProfile.Service wrongParent = new MonolithBleProfile.Service(
                UUID.fromString("12345678-0000-0000-0000-000000000001"),
                Collections.singletonList(sf)
        );
        String misplaced = MonolithBleProfile.describe(Collections.singletonList(wrongParent));
        require(misplaced.contains("SiFli transport: NOT FOUND"), "must match service and child characteristic");

        System.out.println("MonolithBleProfileJvmTest: 9 assertions PASS");
    }
}
