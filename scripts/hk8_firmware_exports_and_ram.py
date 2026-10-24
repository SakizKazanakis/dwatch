#!/usr/bin/env python3
"""Evidence-led HK8 3407 ELF import and memory-budget investigation (READ ONLY).

An undefined ELF symbol is a *required import*, NEVER proof that the matching
ROM exports it. Product-family SRAM figures are NOT free heap. Does not issue
any BLE, serial, shell or physical-device commands.
"""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

TARGET_W, TARGET_H = 410, 494
SOURCE_W, SOURCE_H = 485, 520
SOURCE_COMPRESSED_BYTES = 158937
STREAM_SCRATCH_BYTES = 32768 + 485*3*3
SF32LB55_HCPU_SRAM_KIB = 1088
SF32LB55_LCPU_SRAM_KIB = 224
HISTORIC_FACE_SHA256 = "a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec"

def read_elf_symbols(path):
    path = Path(path)
    raw = path.read_bytes()
    if raw[:5] != b"\x7fELF\x01":
        raise ValueError("Expected a genuine ELF32 object")
    head = subprocess.run(["readelf", "-h", str(path)], text=True,
                          capture_output=True, check=True).stdout
    if not re.search(r"Machine:\s+ARM\b", head):
        raise ValueError("Not an ARM ELF")
    out = subprocess.run(["readelf", "-W", "--dyn-syms", str(path)],
                         text=True, capture_output=True, check=True).stdout
    imports, exports = set(), set()
    # Only dynsym, not untrusted strings or a full textual grep of the ROM.
    for line in out.splitlines():
        f = line.split()
        if len(f) < 8 or not f[0].endswith(":") or not f[0][:-1].isdigit():
            continue
        if f[4] not in ("GLOBAL", "WEAK"):
            continue
        name = f[7].split("@", 1)[0]
        if not name or name.startswith("_stack"):
            continue
        if f[6] == "UND":
            imports.add(name)
        elif f[3] not in ("SECTION", "FILE"):
            exports.add(name)
    return {
        "sha256": hashlib.sha256(raw).hexdigest(),
        "size_bytes": len(raw),
        "imports": sorted(imports),
        "exports": sorted(exports)
    }

def memory_requirements():
    root_pixels = TARGET_W * TARGET_H
    source_pixels = SOURCE_W * SOURCE_H
    return {
        "physical_target_display_from_oem_face": [TARGET_W, TARGET_H],
        "monolith_design_pixels": [SOURCE_W, SOURCE_H],
        "compressed_source_bytes": SOURCE_COMPRESSED_BYTES,
        "stream_decoder_heap_bytes": STREAM_SCRATCH_BYTES,
        "target_framebuffer_rgb565_bytes": root_pixels*2,
        "target_framebuffer_argb8888_bytes": root_pixels*4,
        "original_source_rgb888_bytes": source_pixels*3,
        "monolith_image_lvgl32_bytes": source_pixels*4,
        "monolith_image_lvgl16_bytes": source_pixels*2,
        "image32_plus_stream_decoder_bytes": source_pixels*4+STREAM_SCRATCH_BYTES,
        "sf32lb55_family_sram_hcpu_bytes": SF32LB55_HCPU_SRAM_KIB*1024,
        "sf32lb55_family_sram_lcpu_bytes": SF32LB55_LCPU_SRAM_KIB*1024,
        "device_hcpu_heap_free_bytes": None,
        "device_hcpu_largest_contiguous_allocatable_bytes": None,
        "device_psram_total_bytes": None,
        "device_psram_free_bytes": None,
        "actual_available_memory_status": "UNMEASURED"
    }

def parse_free_output(text):
    """Parse a REAL saved RT-Thread msh>free output, never claim live access."""
    patterns = {
       "reported_heap_total_bytes": r"total memory\s*:\s*(\d+)",
       "reported_heap_used_bytes": r"used memory\s*:\s*(\d+)",
       "reported_heap_high_water_bytes": r"maximum allocated memory\s*:\s*(\d+)"
    }
    result = {}
    for key, pat in patterns.items():
        match = re.search(pat, text, re.I)
        if not match:
            raise ValueError("Missing RT-Thread free measurement: "+key)
        result[key] = int(match.group(1))
    if result["reported_heap_used_bytes"] > result["reported_heap_total_bytes"]:
        raise ValueError("Invalid memory telemetry: used > total")
    result["reported_heap_free_arithmetic_bytes"] = (
        result["reported_heap_total_bytes"] - result["reported_heap_used_bytes"])
    result["largest_free_allocation_bytes"] = None
    result["psram_total_bytes"] = None
    result["psram_free_bytes"] = None
    result["measurement_scope"] = (
      "Only the heap exposed by the supplied RT-Thread free output; "
      "this does not identify PSRAM, contiguous allocation size or GPU buffers")
    return result

def inspect(face, resource=None, firmware=None, free_output=None):
    main = read_elf_symbols(face)
    res = read_elf_symbols(resource) if resource else None
    rom = read_elf_symbols(firmware) if firmware else None
    imported = set(main["imports"])
    resources = set(res["exports"]) if res else set()
    unresolved = sorted(imported - resources)
    result = {
       "device_model":"HK8 PRO MAX",
       "user_reported_hardware_code":"3407",
       "user_reported_firmware_version":"2.09",
       "offline_face": main,
       "original_face_identical_to_captured_sha256":
           main["sha256"] == HISTORIC_FACE_SHA256,
       "resource_module": res,
       "required_external_import_count": len(imported),
       "possible_import_provider_in_resource_file": sorted(imported & resources),
       "requires_firmware_or_other_modules": unresolved,
       "possible_firmware_or_module_import_count": len(unresolved),
       "full_firmware_export_list": None,
       "firmware_elf_candidate": rom,
       "matched_device_firmware_proven": False,
       "verified_firmware_export_coverage": None,
       "available_device_memory": memory_requirements(),
       "measured_heap_from_supplied_text": parse_free_output(
           Path(free_output).read_text()) if free_output else None,
       "conclusion": "PARTIAL: exact face imports, not exact firmware exports "
                     "or measured available RAM/PSRAM"
    }
    if rom:
        symbols = set(rom["exports"])
        result["firmware_candidate_imports_covered"] = sorted(imported & symbols)
        result["firmware_candidate_imports_missing"] = sorted(imported - resources - symbols)
        result["conclusion"] = ("CANDIDATE ONLY: externally supplied ELF is not "
            "authenticated against HK8 hardware 3407 / firmware 2.09")
    return result

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--face-elf",required=True,type=Path)
    p.add_argument("--resource-elf",type=Path)
    p.add_argument("--firmware-elf",type=Path,
                   help="Candidate ARM ELF firmware (authentication NOT automatic)")
    p.add_argument("--rtthread-free-log",type=Path,
                   help="Saved live RT-Thread free output; NOT phone /proc/meminfo")
    p.add_argument("--output",type=Path)
    args=p.parse_args()
    data=inspect(args.face_elf,args.resource_elf,args.firmware_elf,
                 args.rtthread_free_log)
    output=json.dumps(data,ensure_ascii=False,sort_keys=True,indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(output+"\n")
    print(output)

if __name__=="__main__":
    main()
