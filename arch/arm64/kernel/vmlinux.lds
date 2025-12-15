OUTPUT_ARCH(aarch64)
ENTRY(_text)
jiffies = jiffies_64;
PECOFF_FILE_ALIGNMENT = 0x200;
SECTIONS
{
 /DISCARD/ : { *(.exitcall.exit) *(.discard) *(.discard.*) *(.export_symbol) *(.modinfo) *(.gnu.version*) }
 /DISCARD/ : {
  *(.interp .dynamic)
  *(.dynsym .dynstr .hash .gnu.hash)
 }
 . = ((((-(((1)) << ((((48))) - 1)))) + (0x80000000)));
 .head.text : {
  _text = .;
  KEEP(*(.head.text))
 }
 .text : ALIGN(0x00010000) {
  _stext = .;
   . = ALIGN(4); __irqentry_text_start = .; *(.irqentry.text) __irqentry_text_end = .;
   . = ALIGN(4); __softirqentry_text_start = .; *(.softirqentry.text) __softirqentry_text_end = .;
   . = ALIGN(4); __entry_text_start = .; *(.entry.text) __entry_text_end = .;
   . = ALIGN(4); *(.text.hot .text.hot.*) *(.text .text.fixup) *(.text.unlikely .text.unlikely.*) *(.text.unknown .text.unknown.*) . = ALIGN(4); __noinstr_text_start = .; *(.noinstr.text) __cpuidle_text_start = .; *(.cpuidle.text) __cpuidle_text_end = .; __noinstr_text_end = .; *(.ref.text) *(.text.asan.* .text.tsan.*)
   . = ALIGN(4); __sched_text_start = .; *(.sched.text) __sched_text_end = .;
   . = ALIGN(4); __lock_text_start = .; *(.spinlock.text) __lock_text_end = .;
   . = ALIGN(4); __kprobes_text_start = .; *(.kprobes.text) __kprobes_text_end = .;
   . = ALIGN((1 << 16)); __hyp_idmap_text_start = .; *(.hyp.idmap.text) __hyp_idmap_text_end = .; __hyp_text_start = .; *(.hyp.text) . = ALIGN(0x00000008); __start___kvm_ex_table = .; *(__kvm_ex_table) __stop___kvm_ex_table = .; . = ALIGN((1 << 16)); __hyp_text_end = .;
   *(.gnu.warning)
 }
 . = ALIGN(0x00010000);
 _etext = .;
 . = ALIGN(((1 << 16))); .rodata : AT(ADDR(.rodata) - 0) { __start_rodata = .; *(.rodata) *(.rodata.*) . = ALIGN(32); __sched_class_highest = .; *(__stop_sched_class) *(__dl_sched_class) *(__rt_sched_class) *(__fair_sched_class) *(__idle_sched_class) __sched_class_lowest = .; . = ALIGN(8); __start_ro_after_init = .; *(.data..ro_after_init) . = ALIGN(8); __start___jump_table = .; KEEP(*(__jump_table)) __stop___jump_table = .; __end_ro_after_init = .; . = ALIGN(8); __start___tracepoints_ptrs = .; KEEP(*(__tracepoints_ptrs)) __stop___tracepoints_ptrs = .; *(__tracepoints_strings) } .rodata1 : AT(ADDR(.rodata1) - 0) { *(.rodata1) } .pci_fixup : AT(ADDR(.pci_fixup) - 0) { __start_pci_fixups_early = .; KEEP(*(.pci_fixup_early)) __end_pci_fixups_early = .; __start_pci_fixups_header = .; KEEP(*(.pci_fixup_header)) __end_pci_fixups_header = .; __start_pci_fixups_final = .; KEEP(*(.pci_fixup_final)) __end_pci_fixups_final = .; __start_pci_fixups_enable = .; KEEP(*(.pci_fixup_enable)) __end_pci_fixups_enable = .; __start_pci_fixups_resume = .; KEEP(*(.pci_fixup_resume)) __end_pci_fixups_resume = .; __start_pci_fixups_suspend = .; KEEP(*(.pci_fixup_suspend)) __end_pci_fixups_suspend = .; __start_pci_fixups_resume_early = .; KEEP(*(.pci_fixup_resume_early)) __end_pci_fixups_resume_early = .; __start_pci_fixups_suspend_late = .; KEEP(*(.pci_fixup_suspend_late)) __end_pci_fixups_suspend_late = .; } .builtin_fw : AT(ADDR(.builtin_fw) - 0) ALIGN(8) { __start_builtin_fw = .; KEEP(*(.builtin_fw)) __end_builtin_fw = .; } __ksymtab : AT(ADDR(__ksymtab) - 0) { __start___ksymtab = .; KEEP(*(SORT(___ksymtab+*))) __stop___ksymtab = .; } __ksymtab_gpl : AT(ADDR(__ksymtab_gpl) - 0) { __start___ksymtab_gpl = .; KEEP(*(SORT(___ksymtab_gpl+*))) __stop___ksymtab_gpl = .; } __kcrctab : AT(ADDR(__kcrctab) - 0) { __start___kcrctab = .; KEEP(*(SORT(___kcrctab+*))) __stop___kcrctab = .; } __kcrctab_gpl : AT(ADDR(__kcrctab_gpl) - 0) { __start___kcrctab_gpl = .; KEEP(*(SORT(___kcrctab_gpl+*))) __stop___kcrctab_gpl = .; } __ksymtab_strings : AT(ADDR(__ksymtab_strings) - 0) { *(__ksymtab_strings) } __init_rodata : AT(ADDR(__init_rodata) - 0) { *(.ref.rodata) } __param : AT(ADDR(__param) - 0) { __start___param = .; KEEP(*(__param)) __stop___param = .; } __modver : AT(ADDR(__modver) - 0) { __start___modver = .; KEEP(*(__modver)) __stop___modver = .; } . = ALIGN(4); __ex_table : AT(ADDR(__ex_table) - 0) { __start___ex_table = .; KEEP(*(__ex_table)) __stop___ex_table = .; } /DISCARD/ : { *(.note.GNU-stack) *(.note.gnu.property) } .notes : AT(ADDR(.notes) - 0) { __start_notes = .; KEEP(*(.note.*)) __stop_notes = .; } . = ALIGN(((1 << 16))); __end_rodata = .;
 .hyp.rodata : { . = ALIGN((1 << 16)); __hyp_rodata_start = .; *(.hyp.data..ro_after_init) *(.hyp.rodata) . = ALIGN((1 << 16)); __hyp_rodata_end = .; }
 .got : { *(.got) }
 .got.plt : { *(.got.plt) }
 ASSERT(SIZEOF(.got.plt) == 0 || SIZEOF(.got.plt) == 0x18,
        "Unexpected GOT/PLT entries detected!")
 .rodata.text : {
  . = ALIGN((1 << 16)); __entry_tramp_text_start = .; *(.entry.tramp.text) . = ALIGN((1 << 16)); __entry_tramp_text_end = .; *(.entry.tramp.rodata)
  . = ALIGN(4); __hibernate_exit_text_start = .; *(.hibernate_exit.text) __hibernate_exit_text_end = .;
  . = ALIGN(4); __relocate_new_kernel_start = .; *(.kexec_relocate.text) __relocate_new_kernel_end = .;
  . = ALIGN(0x00001000); __idmap_text_start = .; *(.idmap.text) __idmap_text_end = .;
  . = ALIGN((1 << 16));
 }
 idmap_pg_dir = .;
 . += (1 << 16);
 tramp_pg_dir = .;
 . += (1 << 16);
 reserved_pg_dir = .;
 . += (1 << 16);
 swapper_pg_dir = .;
 . += (1 << 16);
 . = ALIGN(0x00010000);
 __init_begin = .;
 __inittext_begin = .;
 . = ALIGN(8); .init.text : AT(ADDR(.init.text) - 0) { _sinittext = .; *(.init.text .init.text.*) *(.text.startup) *(.meminit.text*) _einittext = .; }
 __exittext_begin = .;
 .exit.text : {
  *(.exit.text) *(.text.exit)
 }
 __exittext_end = .;
 . = ALIGN(4);
 .altinstructions : {
  __alt_instructions = .;
  *(.altinstructions)
  __alt_instructions_end = .;
 }

 . = ALIGN(0x00010000);
 __inittext_end = .;
 __initdata_begin = .;
 init_idmap_pg_dir = .;
 . += (( 1 + (((((((_end + 0x00200000 + (1 << 16))) - 1) >> (((16 - 3) * (4 - (4 - 3)) + 3))) - (((((((-(((1)) << ((((48))) - 1)))) + (0x80000000))))) >> (((16 - 3) * (4 - (4 - 3)) + 3))) + 1) + (1))) + (0) + (((((((_end + 0x00200000 + (1 << 16))) - 1) >> (((16 - 3) * (4 - (2)) + 3))) - (((((((-(((1)) << ((((48))) - 1)))) + (0x80000000))))) >> (((16 - 3) * (4 - (2)) + 3))) + 1) + (1)))) * (1 << 16));
 init_idmap_pg_end = .;
 .init.data : {
  KEEP(*(SORT(___kentry+*))) *(.init.data .init.data.*) *(.meminit.data*) *(.init.rodata .init.rodata.*) . = ALIGN(8); __start_ftrace_events = .; KEEP(*(_ftrace_events)) __stop_ftrace_events = .; __start_ftrace_eval_maps = .; KEEP(*(_ftrace_eval_map)) __stop_ftrace_eval_maps = .; . = ALIGN(8); __start_syscalls_metadata = .; KEEP(*(__syscalls_metadata)) __stop_syscalls_metadata = .; . = ALIGN(8); __start_kprobe_blacklist = .; KEEP(*(_kprobe_blacklist)) __stop_kprobe_blacklist = .; *(.meminit.rodata) . = ALIGN(8); __clk_of_table = .; KEEP(*(__clk_of_table)) KEEP(*(__clk_of_table_end)) . = ALIGN(8); __reservedmem_of_table = .; KEEP(*(__reservedmem_of_table)) KEEP(*(__reservedmem_of_table_end)) . = ALIGN(8); __timer_of_table = .; KEEP(*(__timer_of_table)) KEEP(*(__timer_of_table_end)) . = ALIGN(8); __cpu_method_of_table = .; KEEP(*(__cpu_method_of_table)) KEEP(*(__cpu_method_of_table_end)) . = ALIGN(8); __cpuidle_method_of_table = .; KEEP(*(__cpuidle_method_of_table)) KEEP(*(__cpuidle_method_of_table_end)) . = ALIGN(32); __dtb_start = .; KEEP(*(.dtb.init.rodata)) __dtb_end = .; . = ALIGN(8); __irqchip_of_table = .; KEEP(*(__irqchip_of_table)) KEEP(*(__irqchip_of_table_end)) . = ALIGN(8); __irqchip_acpi_probe_table = .; KEEP(*(__irqchip_acpi_probe_table)) __irqchip_acpi_probe_table_end = .; . = ALIGN(8); __timer_acpi_probe_table = .; KEEP(*(__timer_acpi_probe_table)) __timer_acpi_probe_table_end = .; . = ALIGN(8); __governor_thermal_table = .; KEEP(*(__governor_thermal_table)) __governor_thermal_table_end = .; . = ALIGN(8); __earlycon_table = .; KEEP(*(__earlycon_table)) __earlycon_table_end = .; . = ALIGN(8); __start_lsm_info = .; KEEP(*(.lsm_info.init)) __end_lsm_info = .; . = ALIGN(8); __start_early_lsm_info = .; KEEP(*(.early_lsm_info.init)) __end_early_lsm_info = .; . = ALIGN(8); __kunit_suites_start = .; KEEP(*(.kunit_test_suites)) __kunit_suites_end = .;
  . = ALIGN(16); __setup_start = .; KEEP(*(.init.setup)) __setup_end = .;
  __initcall_start = .; KEEP(*(.initcallearly.init)) __initcall0_start = .; KEEP(*(.initcall0.init)) KEEP(*(.initcall0s.init)) __initcall1_start = .; KEEP(*(.initcall1.init)) KEEP(*(.initcall1s.init)) __initcall2_start = .; KEEP(*(.initcall2.init)) KEEP(*(.initcall2s.init)) __initcall3_start = .; KEEP(*(.initcall3.init)) KEEP(*(.initcall3s.init)) __initcall4_start = .; KEEP(*(.initcall4.init)) KEEP(*(.initcall4s.init)) __initcall5_start = .; KEEP(*(.initcall5.init)) KEEP(*(.initcall5s.init)) __initcallrootfs_start = .; KEEP(*(.initcallrootfs.init)) KEEP(*(.initcallrootfss.init)) __initcall6_start = .; KEEP(*(.initcall6.init)) KEEP(*(.initcall6s.init)) __initcall7_start = .; KEEP(*(.initcall7.init)) KEEP(*(.initcall7s.init)) __initcall_end = .;
  __con_initcall_start = .; KEEP(*(.con_initcall.init)) __con_initcall_end = .;
  . = ALIGN(4); __initramfs_start = .; KEEP(*(.init.ramfs)) . = ALIGN(8); KEEP(*(.init.ramfs.info))
  *(.init.altinstructions .init.bss)
 }
 .exit.data : {
  *(.exit.data .exit.data.*) *(.fini_array .fini_array.*) *(.dtors .dtors.*)
 }
 . = ALIGN((1 << 16)); .data..percpu : AT(ADDR(.data..percpu) - 0) { __per_cpu_load = .; __per_cpu_start = .; *(.data..percpu..first) . = ALIGN((1 << 16)); *(.data..percpu..page_aligned) . = ALIGN((1 << (6))); *(.data..percpu..read_mostly) . = ALIGN((1 << (6))); *(.data..percpu) *(.data..percpu..shared_aligned) __per_cpu_end = .; }
 . = ALIGN((1 << 16)); .hyp.data..percpu : { *(.hyp.data..percpu) }
 .hyp.reloc : ALIGN(4) { __hyp_reloc_begin = .; *(.hyp.reloc) __hyp_reloc_end = .; }
 .rela.dyn : ALIGN(8) {
  __rela_start = .;
  *(.rela .rela*)
  __rela_end = .;
 }
 .relr.dyn : ALIGN(8) {
  __relr_start = .;
  *(.relr.dyn)
  __relr_end = .;
 }
 . = ALIGN(0x00010000);
 __initdata_end = .;
 __init_end = .;
 _data = .;
 _sdata = .;
 . = ALIGN((1 << 16)); .data : AT(ADDR(.data) - 0) { . = ALIGN((((1)) << (14 + 0))); __start_init_task = .; init_thread_union = .; init_stack = .; KEEP(*(.data..init_task)) KEEP(*(.data..init_thread_info)) . = __start_init_task + (((1)) << (14 + 0)); __end_init_task = .; . = ALIGN((1 << 16)); __nosave_begin = .; *(.data..nosave) . = ALIGN((1 << 16)); __nosave_end = .; . = ALIGN((1 << 16)); *(.data..page_aligned) . = ALIGN((1 << 16)); . = ALIGN((1 << (6))); *(.data..cacheline_aligned) . = ALIGN((1 << (6))); *(.data..read_mostly) . = ALIGN((1 << (6))); *(.xiptext) *(.data) *(.data..decrypted) *(.ref.data) *(.data..shared_aligned) *(.data.unlikely) __start_once = .; *(.data.once) __end_once = .; . = ALIGN(32); *(__tracepoints) . = ALIGN(8); __start___dyndbg_classes = .; KEEP(*(__dyndbg_classes)) __stop___dyndbg_classes = .; __start___dyndbg = .; KEEP(*(__dyndbg)) __stop___dyndbg = .; __start___trace_bprintk_fmt = .; KEEP(*(__trace_printk_fmt)) __stop___trace_bprintk_fmt = .; . = ALIGN(32); __start__bpf_raw_tp = .; KEEP(*(__bpf_raw_tp_map)) __stop__bpf_raw_tp = .; __start___tracepoint_str = .; KEEP(*(__tracepoint_str)) __stop___tracepoint_str = .; CONSTRUCTORS } . = ALIGN(8); __bug_table : AT(ADDR(__bug_table) - 0) { __start___bug_table = .; KEEP(*(__bug_table)) __stop___bug_table = .; }
 .mmuoff.data.write : ALIGN(0x00000800) {
  __mmuoff_data_start = .;
  *(.mmuoff.data.write)
 }
 . = ALIGN(0x00000800);
 .mmuoff.data.read : {
  *(.mmuoff.data.read)
  __mmuoff_data_end = .;
 }
 .pecoff_edata_padding : { BYTE(0); . = ALIGN(PECOFF_FILE_ALIGNMENT); }
 __pecoff_data_rawsize = ABSOLUTE(. - __initdata_begin);
 _edata = .;
 . = ALIGN((1 << 16)); __bss_start = .; . = ALIGN((1 << 16)); .sbss : AT(ADDR(.sbss) - 0) { *(.dynsbss) *(.sbss) *(.scommon) } . = ALIGN(0); .bss : AT(ADDR(.bss) - 0) { __hyp_bss_start = .; *(.hyp.bss) . = ALIGN((1 << 16)); __hyp_bss_end = .; . = ALIGN((1 << 16)); *(.bss..page_aligned) . = ALIGN((1 << 16)); *(.dynbss) *(.bss) *(COMMON) } . = ALIGN(0); __bss_stop = .;
 . = ALIGN((1 << 16));
 init_pg_dir = .;
 . += ((1 << 16) * ( 1 + (((((((_end)) - 1) >> (((16 - 3) * (4 - (4 - 3)) + 3))) - (((((((-(((1)) << ((((48))) - 1)))) + (0x80000000))))) >> (((16 - 3) * (4 - (4 - 3)) + 3))) + 1) + ((0)))) + (0) + (((((((_end)) - 1) >> (((16 - 3) * (4 - (2)) + 3))) - (((((((-(((1)) << ((((48))) - 1)))) + (0x80000000))))) >> (((16 - 3) * (4 - (2)) + 3))) + 1) + ((0))))));
 init_pg_end = .;
 . = ALIGN(0x00010000);
 __pecoff_data_size = ABSOLUTE(. - __initdata_begin);
 _end = .;
 .stab 0 : { *(.stab) } .stabstr 0 : { *(.stabstr) } .stab.excl 0 : { *(.stab.excl) } .stab.exclstr 0 : { *(.stab.exclstr) } .stab.index 0 : { *(.stab.index) } .stab.indexstr 0 : { *(.stab.indexstr) }
 .debug 0 : { *(.debug) } .line 0 : { *(.line) } .debug_srcinfo 0 : { *(.debug_srcinfo) } .debug_sfnames 0 : { *(.debug_sfnames) } .debug_aranges 0 : { *(.debug_aranges) } .debug_pubnames 0 : { *(.debug_pubnames) } .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) } .debug_abbrev 0 : { *(.debug_abbrev) } .debug_line 0 : { *(.debug_line) } .debug_frame 0 : { *(.debug_frame) } .debug_str 0 : { *(.debug_str) } .debug_loc 0 : { *(.debug_loc) } .debug_macinfo 0 : { *(.debug_macinfo) } .debug_pubtypes 0 : { *(.debug_pubtypes) } .debug_ranges 0 : { *(.debug_ranges) } .debug_weaknames 0 : { *(.debug_weaknames) } .debug_funcnames 0 : { *(.debug_funcnames) } .debug_typenames 0 : { *(.debug_typenames) } .debug_varnames 0 : { *(.debug_varnames) } .debug_gnu_pubnames 0 : { *(.debug_gnu_pubnames) } .debug_gnu_pubtypes 0 : { *(.debug_gnu_pubtypes) } .debug_types 0 : { *(.debug_types) } .debug_addr 0 : { *(.debug_addr) } .debug_line_str 0 : { *(.debug_line_str) } .debug_loclists 0 : { *(.debug_loclists) } .debug_macro 0 : { *(.debug_macro) } .debug_names 0 : { *(.debug_names) } .debug_rnglists 0 : { *(.debug_rnglists) } .debug_str_offsets 0 : { *(.debug_str_offsets) }
 .comment 0 : { *(.comment) } .symtab 0 : { *(.symtab) } .strtab 0 : { *(.strtab) } .shstrtab 0 : { *(.shstrtab) }
 _kernel_size_le_lo32 = (((_end - _text) & 0xffffffff) & 0xffffffff); _kernel_size_le_hi32 = (((_end - _text) >> 32) & 0xffffffff); _kernel_flags_le_lo32 = (((((0 << 0) | (((16 - 10) / 2) << (0 + 1)) | (1 << ((0 + 1) + 2)))) & 0xffffffff) & 0xffffffff); _kernel_flags_le_hi32 = (((((0 << 0) | (((16 - 10) / 2) << (0 + 1)) | (1 << ((0 + 1) + 2)))) >> 32) & 0xffffffff);
 .plt : {
  *(.plt) *(.plt.*) *(.iplt) *(.igot .igot.plt)
 }
 ASSERT(SIZEOF(.plt) == 0, "Unexpected run-time procedure linkages detected!")
 .data.rel.ro : { *(.data.rel.ro) }
 ASSERT(SIZEOF(.data.rel.ro) == 0, "Unexpected RELRO detected!")
}
PROVIDE(__efistub_primary_entry = primary_entry);
PROVIDE(__efistub_caches_clean_inval_pou = __pi_caches_clean_inval_pou);
PROVIDE(__efistub__text = _text);
PROVIDE(__efistub__end = _end);
PROVIDE(__efistub___inittext_end = __inittext_end);
PROVIDE(__efistub__edata = _edata);
PROVIDE(__efistub_screen_info = screen_info);
PROVIDE(__efistub__ctype = _ctype);
PROVIDE(__pi___memcpy = __pi_memcpy);
PROVIDE(__pi___memmove = __pi_memmove);
PROVIDE(__pi___memset = __pi_memset);
__kvm_nvhe_kvm_patch_vector_branch = kvm_patch_vector_branch;;
__kvm_nvhe_kvm_update_va_mask = kvm_update_va_mask;;
__kvm_nvhe_kvm_get_kimage_voffset = kvm_get_kimage_voffset;;
__kvm_nvhe_kvm_compute_final_ctr_el0 = kvm_compute_final_ctr_el0;;
__kvm_nvhe_spectre_bhb_patch_loop_iter = spectre_bhb_patch_loop_iter;;
__kvm_nvhe_spectre_bhb_patch_loop_mitigation_enable = spectre_bhb_patch_loop_mitigation_enable;;
__kvm_nvhe_spectre_bhb_patch_wa3 = spectre_bhb_patch_wa3;;
__kvm_nvhe_spectre_bhb_patch_clearbhb = spectre_bhb_patch_clearbhb;;
__kvm_nvhe_alt_cb_patch_nops = alt_cb_patch_nops;;
__kvm_nvhe_kvm_vgic_global_state = kvm_vgic_global_state;;
__kvm_nvhe_nvhe_hyp_panic_handler = nvhe_hyp_panic_handler;;
__kvm_nvhe___hyp_stub_vectors = __hyp_stub_vectors;;
__kvm_nvhe_arm64_mpam_has_hcr = arm64_mpam_has_hcr;;
__kvm_nvhe_vgic_v2_cpuif_trap = vgic_v2_cpuif_trap;;
__kvm_nvhe_vgic_v3_cpuif_trap = vgic_v3_cpuif_trap;;
__kvm_nvhe___start___kvm_ex_table = __start___kvm_ex_table;;
__kvm_nvhe___stop___kvm_ex_table = __stop___kvm_ex_table;;
__kvm_nvhe_kvm_arm_pmu_available = kvm_arm_pmu_available;;
__kvm_nvhe_clear_page = __kvm_nvhe___pi_clear_page;;
__kvm_nvhe_copy_page = __kvm_nvhe___pi_copy_page;;
__kvm_nvhe_memcpy = __kvm_nvhe___pi_memcpy;;
__kvm_nvhe_memset = __kvm_nvhe___pi_memset;;
__kvm_nvhe___hyp_idmap_text_start = __hyp_idmap_text_start;;
__kvm_nvhe___hyp_idmap_text_end = __hyp_idmap_text_end;;
__kvm_nvhe___hyp_text_start = __hyp_text_start;;
__kvm_nvhe___hyp_text_end = __hyp_text_end;;
__kvm_nvhe___hyp_bss_start = __hyp_bss_start;;
__kvm_nvhe___hyp_bss_end = __hyp_bss_end;;
__kvm_nvhe___hyp_rodata_start = __hyp_rodata_start;;
__kvm_nvhe___hyp_rodata_end = __hyp_rodata_end;;
__kvm_nvhe_kvm_protected_mode_initialized = kvm_protected_mode_initialized;;
ASSERT(__hyp_idmap_text_end - __hyp_idmap_text_start <= (1 << 16),
 "HYP init code too big")
ASSERT(__idmap_text_end - (__idmap_text_start & ~(0x00001000 - 1)) <= 0x00001000,
 "ID map text too big or misaligned")
ASSERT(__hibernate_exit_text_end - __hibernate_exit_text_start <= 0x00001000,
       "Hibernate exit text is bigger than 4 KiB")
ASSERT(__hibernate_exit_text_start == swsusp_arch_suspend_exit,
       "Hibernate exit text does not start with swsusp_arch_suspend_exit")
ASSERT((__entry_tramp_text_end - __entry_tramp_text_start) <= 3*(1 << 16),
 "Entry trampoline text too big")
ASSERT(__hyp_bss_start == __bss_start, "HYP and Host BSS are misaligned")
ASSERT(_text == ((((-(((1)) << ((((48))) - 1)))) + (0x80000000))), "HEAD is misaligned")
ASSERT(swapper_pg_dir - reserved_pg_dir == ((1 << 16)),
       "RESERVED_SWAPPER_OFFSET is wrong!")
ASSERT(swapper_pg_dir - tramp_pg_dir == (2 * (1 << 16)),
       "TRAMP_SWAPPER_OFFSET is wrong!")
ASSERT(__relocate_new_kernel_end - __relocate_new_kernel_start <= 0x00001000,
       "kexec relocation code is bigger than 4 KiB")
ASSERT(4096 >= 0x00001000, "KEXEC_CONTROL_PAGE_SIZE is broken")
ASSERT(__relocate_new_kernel_start == arm64_relocate_new_kernel,
       "kexec control page does not start with arm64_relocate_new_kernel")
