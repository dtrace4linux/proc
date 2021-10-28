# if !defined(DSTR_H_INCLUDE__)
# define	DSTR_H_INCLUDE__	1

# if __cplusplus
extern "C" {
# endif

/**********************************************************************/
/*   Simple counted string type.				      */
/**********************************************************************/
typedef struct cstr_t {
	int	c_len;
	char	*c_ptr;
	} cstr_t;

/**********************************************************************/
/*   Dynamic string type.					      */
/**********************************************************************/
typedef struct dstr_t {
	char	*dl_str;
	int	dl_size;
	int	dl_offset;
	} dstr_t;

# define	DSTR_MAXSIZE(dstr)	((dstr)->dl_size)
# define	DSTR_SIZE(dstr)		((dstr)->dl_offset)
# define	DSTR_STRING(dstr)	((dstr)->dl_str)
# define	DSTR_USTRING(dstr)	(unsigned char *) ((dstr)->dl_str)
# define	DSTR_STREND(dstr)	((dstr)->dl_str + (dstr)->dl_offset)

# if !defined(DLL_EXPORT)
#	define	DLL_EXPORT
#	define	EXPORT_FUNCTION
# endif

# if !defined(TYPE_int64)
#	define	TYPE_int64	unsigned long
# endif

DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_char(dstr_t *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_cstring(dstr_t *, char *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_float(dstr_t *, double);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_id(dstr_t *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_int(dstr_t *, long);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_int_2(dstr_t *, long, long);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_int16(dstr_t *, long);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_int24(dstr_t *, long);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_int32(dstr_t *, long);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_int64(dstr_t *, TYPE_int64);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_integer(dstr_t *, TYPE_int64);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_nchar(dstr_t *, int, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_ptr(dstr_t *, int, char *);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_list(dstr_t *, char *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_mem(dstr_t *, char *, size_t);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_null(dstr_t *);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_add_string(dstr_t *, char *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_free(dstr_t *);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_init(dstr_t *, size_t);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_static_init(dstr_t *, void *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_clear(dstr_t *);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_zinit(dstr_t *, int);
DLL_EXPORT void EXPORT_FUNCTION dstr_remove(dstr_t *dstr, int num);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_reserve(dstr_t *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_reserve_zero(dstr_t *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_resize(dstr_t *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_set_size(dstr_t *, int);
DLL_EXPORT void EXPORT_FUNCTION dstr_set_space(dstr_t *dstr, int size);
DLL_EXPORT void EXPORT_FUNCTION dstr_setup(dstr_t *dstr, void *ptr, int used);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_size(dstr_t *, int);
DLL_EXPORT void	EXPORT_FUNCTION	dstr_terminate(dstr_t *);

# if defined(REFSTR_H_INCLUDE__)
DLL_EXPORT ref_t *EXPORT_FUNCTION	dstr_add_str(dstr_t *, char *, int);
# endif

# if __cplusplus
}
# endif

#else

# if defined(REFSTR_H_INCLUDE__)
DLL_EXPORT ref_t *EXPORT_FUNCTION	dstr_add_str(dstr_t *, char *, int);
# endif

# endif /* DSTR_H_INCLUDE__ */
