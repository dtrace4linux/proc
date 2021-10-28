DLL_EXPORT int EXPORT_FUNCTION crc16 PROTO((unsigned char *, int));
DLL_EXPORT u_int32 EXPORT_FUNCTION crc32 PROTO((unsigned char *, int));
u_int32 crc32_monocase PROTO((unsigned char *, unsigned char *, int));
void	crc32_set(unsigned long crc);
u_int32 crc32_accum(unsigned char *buf, int len);

