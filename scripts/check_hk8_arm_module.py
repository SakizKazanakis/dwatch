#!/usr/bin/env python3
"""Static ARM ELF compatibility filter, NOT a firmware-loader execution test."""
import argparse
import re
import subprocess

# Verbatim undefined symbol names observed in the user's OEM wf_clock23.so.
OEM_IMPORTS={
 'app_tileview_only_change_anim_status','app_tileview_register_watchface',
 'dlmodule_free_user_data','dlmodule_get_res_mng','dlmodule_get_user_data',
 'dlmodule_set_res_mng','lv_cont_create','lv_ext_get_locale',
 'lv_ext_res_mng_builtin_destroy','lv_ext_res_mng_builtin_init',
 'lv_ext_set_locale','lv_i18n_lang_pack','lv_obj_align',
 'lv_obj_set_parent_event','lv_obj_set_size','lv_page_glue_obj',
 'rt_free','rt_malloc','wf_obj_analog_clock_create','wf_obj_img_create',
 'wf_view_deinit','wf_view_init','wf_view_pause','wf_view_resume'
}

def run(args):
    return subprocess.run(['readelf',*args],text=True,capture_output=True,
                          check=True).stdout

def imported(symbols):
    return {f[-1] for row in symbols.splitlines() if (f:=row.split())
            and len(f)>=8 and f[6]=='UND'}

def relocation_types(text):
    return set(re.findall(r'\bR_ARM_[A-Z0-9_]+\b',text))

def inspect(path):
    header=run(['-h',path])
    symbols=run(['-sW',path])
    reloc=run(['-rW',path])
    dynamic=run(['-d',path])
    if not all(marker in header for marker in ('ELF32','ARM','DYN','hard-float ABI')):
        raise ValueError('architecture or ABI does not match captured OEM module')
    imports=imported(symbols)
    extras=imports-OEM_IMPORTS
    if extras:
        raise ValueError('imports not found in OEM module: '+repr(sorted(extras)))
    kinds=relocation_types(reloc)
    if not kinds.issubset({'R_ARM_GLOB_DAT','R_ARM_RELATIVE'}) or 'R_ARM_GLOB_DAT' not in kinds:
        raise ValueError('relocations differ from checked OEM classes')
    for name in ('module_init','module_cleanup','monolith_watchface_register','monolith_create_view'):
        if not re.search(r'\b'+re.escape(name)+r'\b',symbols):
            raise ValueError('prototype is missing export '+name)
    if '(HASH)' not in dynamic or '(SYMTAB)' not in dynamic:
        raise ValueError('missing observed SysV hash/symbol dynamic layout')
    return {'import_count':len(imports),'imports':sorted(imports),
            'relocations':sorted(kinds),'runtime_loadable':'UNKNOWN',
            'resource_pack_linkable':'UNKNOWN','safe_to_upload':False}

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('offline_arm_elf')
    report=inspect(ap.parse_args().offline_arm_elf)
    print('OFFLINE ELF STRUCTURE PASS:',report)
    print('ON-WATCH LOADER ACCEPTANCE UNVERIFIED. NEVER UPLOAD THIS FILE.')
