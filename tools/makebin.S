/*
 * "objcopy -O binary" they say...
 * how can I say "spit the default ELF object format"?
 */
.data
.global dump_wrpc_info
dump_wrpc_info:
.incbin "dump-wrpc-info-host.bin"

.global dump_ppsi_info
dump_ppsi_info:
.incbin "dump-ppsi-info-host.bin"

