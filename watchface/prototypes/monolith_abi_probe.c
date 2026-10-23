/*
 * MONOLITH offline ABI probe for HK8 PRO MAX, NOT a functional watchface.
 * Deliberately no LVGL/RT-Thread calls: their signatures and loader contract
 * have not been verified. NEVER upload this ELF to the watch.
 *
 * Target: ELF32 little-endian ARM ET_DYN, EABI5, hard-float (observed on
 * the user's OEM wf_clock23.so).
 */
volatile unsigned int monolith_probe_state = 0;

__attribute__((visibility("default")))
void module_init(void *module) {
    (void)module;
    monolith_probe_state = 1u;
}

__attribute__((visibility("default")))
void module_cleanup(void *module) {
    (void)module;
    monolith_probe_state = 0u;
}

__attribute__((visibility("default")))
int monolith_watchface_probe_version(void) {
    return 1;
}
