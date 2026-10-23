package nodomain.freeyourgadget.gadgetbridge.monolith;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.UUID;

/**
 * Immutable model and formatter for GATT discovery. No Android dependency and no BLE writes.
 * The expected UUIDs are observed on HK8 PRO MAX code 3407, firmware 2.09.
 */
public final class MonolithBleProfile {
    public static final UUID SIFLI_SERVICE = UUID.fromString("00000000-0000-0000-6473-5f696c666973");
    public static final UUID SIFLI_DATA = UUID.fromString("00000000-0000-0200-6473-5f696c666973");
    public static final UUID WEARFIT_SERVICE = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
    public static final UUID WEARFIT_RX = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e");
    public static final UUID CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb");

    private MonolithBleProfile() {}

    public static final class Characteristic {
        public final UUID uuid;
        public final int properties;
        public final List<UUID> descriptors;

        public Characteristic(UUID uuid, int properties, List<UUID> descriptors) {
            this.uuid = uuid;
            this.properties = properties;
            this.descriptors = Collections.unmodifiableList(new ArrayList<>(descriptors));
        }
    }

    public static final class Service {
        public final UUID uuid;
        public final List<Characteristic> characteristics;

        public Service(UUID uuid, List<Characteristic> characteristics) {
            this.uuid = uuid;
            this.characteristics = Collections.unmodifiableList(new ArrayList<>(characteristics));
        }
    }

    private static Characteristic find(List<Service> services, UUID serviceId, UUID characteristicId) {
        for (Service service : services) {
            if (!serviceId.equals(service.uuid)) continue;
            for (Characteristic characteristic : service.characteristics) {
                if (characteristicId.equals(characteristic.uuid)) return characteristic;
            }
        }
        return null;
    }

    private static String properties(int bits) {
        StringBuilder result = new StringBuilder();
        appendProperty(result, bits, 0x01, "BROADCAST");
        appendProperty(result, bits, 0x02, "READ");
        appendProperty(result, bits, 0x04, "WRITE_NO_RESPONSE");
        appendProperty(result, bits, 0x08, "WRITE");
        appendProperty(result, bits, 0x10, "NOTIFY");
        appendProperty(result, bits, 0x20, "INDICATE");
        appendProperty(result, bits, 0x40, "SIGNED_WRITE");
        appendProperty(result, bits, 0x80, "EXTENDED");
        return result.length() == 0 ? "NONE" : result.toString();
    }

    private static void appendProperty(StringBuilder result, int bits, int flag, String label) {
        if ((bits & flag) == 0) return;
        if (result.length() > 0) result.append("|");
        result.append(label);
    }

    public static String describe(List<Service> services) {
        StringBuilder out = new StringBuilder();
        Characteristic sifli = find(services, SIFLI_SERVICE, SIFLI_DATA);
        Characteristic wearfit = find(services, WEARFIT_SERVICE, WEARFIT_RX);
        out.append("Reference: HK8 PRO MAX / HK ONE / code 3407 / system 2.09\n");
        out.append("Reference is based on prior manual observations, NOT read from GATT.\n");
        out.append("SiFli transport: ").append(sifli == null ? "NOT FOUND" : "AVAILABLE (UUID only)").append("\n");
        out.append("Wearfit command: ").append(wearfit == null ? "NOT FOUND" : "AVAILABLE (UUID only)").append("\n");
        out.append("CCCD: ").append(sifli != null && sifli.descriptors.contains(CCCD) ? "present" : "not found").append("\n");
        out.append("firmware write: not verified\n");
        out.append("MTU: unknown; no negotiation attempted\n");
        out.append("GATT services observed: ").append(services.size()).append("\n");
        for (Service service : services) {
            out.append("SERVICE ").append(service.uuid).append("\n");
            for (Characteristic c : service.characteristics) {
                out.append("  CHAR ").append(c.uuid).append(" props=").append(properties(c.properties)).append("\n");
                for (UUID d : c.descriptors) {
                    out.append("    DESC ").append(d).append("\n");
                }
            }
        }
        return out.toString();
    }
}
