/*
 * "objcopy -O binary" they say...
 * how can I say "spit the default ELF object format"?
 */
.data
.global dump_wrpc_info_target
dump_wrpc_info_target:
.incbin "dump-wrpc-info-host.bin"

.global dump_ppsi_info_target
dump_ppsi_info_target:
.incbin "dump-ppsi-info-host.bin"

