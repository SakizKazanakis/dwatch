#!/usr/bin/env python3
"""Read-only OEM HK8 3407 eZIP display and lifetime investigation.

Original OEM executable is private. Fixed Thumb offsets are accepted ONLY for
the exact SHA-pinned 3412-byte source. No firmware/USB/BLE access.
"""
import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

OEM_SHA="a037add2606e114e27705e2bdb0dd17e220fa32d2f1b712dfffd83697037bfec"
MONOLITH_SHA="e4d2412b8c2736fffe9b68f729d6cbbd1b1baefe4f68f2010c16bf60bfc03d9b"
OEM_BACKGROUND="ex/resource/wf_clock23/wf_clock23_bg.bin"
IMAGE_PATHS=[
    OEM_BACKGROUND,
    "ex/installer_wf/wf_clock23_tn.bin",
    "ex/resource/wf_clock23/wf_clock23_m.bin",
    "ex/resource/wf_clock23/wf_clock23_h.bin",
    "ex/resource/wf_clock23/wf_clock23_s.bin",
    "ex/resource/wf_clock23/wf_clock23_yuan_02.bin",
    "ex/resource/wf_clock23/wf_clock23_yuan_01.bin"
]

def image_header(data):
    if len(data)<8:raise ValueError("truncated LVGL image")
    h,=struct.unpack_from("<I",data,0)
    w,ht,fmt=(h>>10)&2047,(h>>21)&2047,h&31
    if not w or not ht or fmt not in (1,2,4,5):
        raise ValueError("invalid LVGL format/dimensions")
    return {"width":w,"height":ht,"format":fmt,"header":f"0x{h:08x}"}

def crc32_mpeg2(data):
    c=0xffffffff
    for b in data:
        c^=b<<24
        for i in range(8):
            c=((c<<1)^(0x04c11db7 if c&0x80000000 else 0))&0xffffffff
    return c

def geometry(src_w,src_h,screen_w,screen_h):
    if min(src_w,src_h,screen_w,screen_h)<=0:raise ValueError("invalid dimensions")
    scale=min(screen_w/src_w,screen_h/src_h)
    inside=[round(src_w*scale),round(src_h*scale)]
    wdiff,hdiff=src_w-screen_w,src_h-screen_h
    return {"overflow":[wdiff,hdiff],
      "no_scale_center_crop":[wdiff//2,wdiff-wdiff//2,hdiff//2,hdiff-hdiff//2],
      "contain_size":inside,
      "contain_unused_pixels":[screen_w-inside[0],screen_h-inside[1]],
      "approved_transform":None,
      "OEM_helper_scales":"UNKNOWN"}

def inspect_elf(path):
    b=Path(path).read_bytes()
    sha=hashlib.sha256(b).hexdigest()
    if sha!=OEM_SHA or len(b)!=3412:
        raise ValueError("fixed-address disassembly applies only to captured OEM ELF")
    # For this SHA-pinned ELF only: .text starts at virtual 0x668/file offset 0x5a0.
    signatures={
      0x682:"5d6a",    # context parent [R3,#0x24]
      0x68a:"1c21",    # user-data request, R1=28
      0x69c:"4ff4f772",# root height=494 in R2
      0x6a0:"4ff4cd71",# root width=410 in R1
      0x6ce:"1c21",    # user-data request for view_init
      0x6dc:"1c21",    # user-data request for wf_obj_img_create
      0x6fc:"1c21",    # user-data request for analog_clock_create
      0x766:"1c21",    # user-data request during event3
      0x774:"1c21",    # user-data request during teardown
      0x818:"2c20"     # rt_malloc(44) on module_init
    }
    for addr,hextext in signatures.items():
        needle=bytes.fromhex(hextext)
        off=0x5a0+addr-0x668
        if b[off:off+len(needle)]!=needle:
            raise ValueError(f"OEM code changed at virtual {addr:#x}")
    return {
       "sha256":sha,"context_parent_offset":0x24,
       "lvgl_root":[410,494],
       "module_resource_manager_bytes":44,
       "view_user_data_request_bytes":28,
       "event0_order":[
          "dlmodule_get_user_data(name,28)",
          "lv_cont_create(parent,NULL)","lv_obj_set_size(root,410,494)",
          "lv_obj_align","lv_obj_set_parent_event","lv_page_glue_obj",
          "dlmodule_get_user_data(name,28)","wf_view_init",
          "dlmodule_get_user_data(name,28)","wf_obj_img_create",
          "dlmodule_get_user_data(name,28)","wf_obj_analog_clock_create"],
       "event3_order":["dlmodule_get_user_data","wf_view_deinit",
                       "dlmodule_get_user_data","dlmodule_free_user_data (conditional)"],
       "module_cleanup_order":["dlmodule_get_res_mng",
                               "lv_ext_res_mng_builtin_destroy (conditional)","rt_free"],
       "opaque_vendor_image_helper":"wf_obj_img_create",
       "explicit_zoom_or_decoder_call_in_this_ELF":False,
       "image_cache_memory_allocations":"UNKNOWN",
       "heap_total_free_and_actual_decoder_RAM":"UNKNOWN"}

def audit(elf,oem_zip,monolith):
    original=Path(monolith).read_bytes()
    if (len(original)!=158937 or
        hashlib.sha256(original).hexdigest()!=MONOLITH_SHA):
        raise ValueError("Monolith source is not the verified existing design")
    source=image_header(original)
    if (source["width"],source["height"],source["format"])!=(485,520,1):
        raise ValueError("existing design header changed")
    resources={}
    with zipfile.ZipFile(oem_zip) as z:
        for n in IMAGE_PATHS:
            b=z.read(n)
            received_crc,=struct.unpack_from("<I",b,len(b)-4)
            if crc32_mpeg2(b[:-4])!=received_crc:
                raise ValueError("original HCI reconstruction CRC failed: "+n)
            resources[n]=dict(image_header(b),size=len(b),crc_ok=True)
    bg=resources[OEM_BACKGROUND]
    if (bg["width"],bg["height"],bg["format"])!=(410,494,2):
        raise ValueError("original OEM background differs")
    return {
       "oem":inspect_elf(elf),
       "manufacturer_resources":resources,
       "existing_monolith":dict(source,bytes=len(original),sha256=MONOLITH_SHA),
       "placement":geometry(485,520,410,494),
       "illustrative_full_frame_rgb888_bytes":485*520*3,
       "illustrative_full_frame_argb8888_bytes":410*494*4,
       "actual_allocations_from_ezip_decoder":"NOT OBSERVED",
       "can_render_on_HK8":"UNVERIFIED",
       "safe_to_upload":False}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--oem-elf",required=True)
    p.add_argument("--oem-zip",required=True)
    p.add_argument("--monolith-bin",required=True)
    a=p.parse_args()
    print(json.dumps(audit(a.oem_elf,a.oem_zip,a.monolith_bin),indent=2))

if __name__=="__main__":main()
