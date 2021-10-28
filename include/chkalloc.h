/**********************************************************************/
/*   This  file  contains the header file definitions for chkalloc.c  */
/*   These  routines  aid in debugging, and performance. The code in  */
/*   this  file  is  part  of  the CRISP package which is (C) P Fox.  */
/*   This  code  may be freely used in any product but the copyright  */
/*   remains  that  of  the author. This copyright notice is present  */
/*   to  avoid  a  conflict of interest and to ensure that CRISP can  */
/*   continue to be a part of the public domain.		      */
/*   								      */
/*   (C) 1991-2010 Paul Fox				              */
/*   $Header: Last edited: 04-Apr-2010 1.5 $ 			      */
/*   								      */
/**********************************************************************/

# if	!defined(_CHK_ALLOC_H)
# define	_CHK_ALLOC_H

# if __cplusplus
extern "C" {
# endif

/**********************************************************************/
/*   Following  macro  used  to  allow  compilation  with ANSI C and  */
/*   non-ANSI C compilers automatically.			      */
/**********************************************************************/
# if !defined(PROTO)
# 	if	defined(__STDC__) || HAVE_PROTOTYPES || __cplusplus
#		define	PROTO(x)	x
#	else
#		define	PROTO(x)	()
#	endif
# endif

# if !defined(EXPORT_FUNCTION)
#	define	EXPORT_FUNCTION
#	define	DLL_EXPORT
# endif

# if BUILDING_SHLIB
#	define	check_alloc	__check_alloc
#	define	check_calloc	__check_calloc
#	define	check_free	__check_free
#	define	check_realloc	__check_realloc
#	define	check_zalloc	__check_zalloc
#	define	chk_alloc	__chk_alloc
#	define	chk_calloc	__chk_calloc
#	define	chk_check	__chk_check
#	define	chk_dump	__chk_dump
#	define	chk_error	__chk_error
#	define	chk_exit	__chk_exit
#	define	chk_free	__chk_free
#	define	chk_free_safe	__chk_free_safe
#	define	chk_free_ptr	__chk_free_ptr
#	define	chk_get_info	__chk_get_info
#	define	chk_init	__chk_init
#	define	chk_leaks	__chk_leaks
#	define	chk_new_leaks	__chk_new_leaks
#	define	chk_print_block	__chk_print_block
#	define	chk_realloc	__chk_realloc
#	define	chk_set_owner	__chk_set_owner
#	define	chk_sigsegv	__chk_sigsegv
#	define	chk_strdup	__chk_strdup
#	define	chk_strndup	__chk_strndup
#	define	chk_stats	__chk_stats
#	define	chk_swap_alloc	__chk_swap_alloc
#	define	chk_swap_free	__chk_swap_free
#	define	chk_tail	__chk_tail
#	define	chk_zalloc	__chk_zalloc
#	define	ck_vmalloc	__ck_vmalloc
# endif

enum chkops {
	CHKOP_MALLOC_0 = 0,
	CHKOP_MALLOC_1,
	CHKOP_REALLOC_0,
	CHKOP_REALLOC_1,
	CHKOP_REALLOC_2,
	CHKOP_FREE_0,
	CHKOP_FREE_1,
	CHKOP_FREE_2,
	};

DLL_EXPORT char	*EXPORT_FUNCTION chk_strdup PROTO((char *));
DLL_EXPORT char	*EXPORT_FUNCTION chk_strndup PROTO((char *, int));
DLL_EXPORT	void	*EXPORT_FUNCTION check_alloc PROTO((unsigned int, char *));
DLL_EXPORT void	*EXPORT_FUNCTION chk_alloc PROTO((int));
DLL_EXPORT char *EXPORT_FUNCTION chk_makestr(const char *, ...);
DLL_EXPORT void	*EXPORT_FUNCTION check_calloc PROTO((int, int, char *));
DLL_EXPORT void	*EXPORT_FUNCTION check_zalloc PROTO((int, char *));
DLL_EXPORT void	*EXPORT_FUNCTION chk_calloc PROTO((int, int));
DLL_EXPORT void	*EXPORT_FUNCTION chk_zalloc PROTO((int));
DLL_EXPORT void	*EXPORT_FUNCTION check_realloc PROTO((char *, int));
DLL_EXPORT void	*EXPORT_FUNCTION chk_realloc PROTO((char *, int));
DLL_EXPORT void	EXPORT_FUNCTION chk_free PROTO((void *));
DLL_EXPORT void	EXPORT_FUNCTION chk_free_safe PROTO((void *));
DLL_EXPORT void	EXPORT_FUNCTION chk_free_ptr PROTO((void **));
DLL_EXPORT void	EXPORT_FUNCTION check_free PROTO((void *));
DLL_EXPORT void	*EXPORT_FUNCTION vm_alloc PROTO((int, void **));
DLL_EXPORT void	*EXPORT_FUNCTION vm_calloc PROTO((int, void **));
DLL_EXPORT void	*EXPORT_FUNCTION vm_zalloc PROTO((int, void **));
DLL_EXPORT void	EXPORT_FUNCTION vm_free PROTO((void *, void **));
DLL_EXPORT void	EXPORT_FUNCTION vm_init PROTO((int, int, void **));
DLL_EXPORT void	EXPORT_FUNCTION vm_destroy PROTO((void **));
unsigned long	chk_get_info PROTO((int code));

void	*chk_swap_alloc PROTO((int));
void	chk_swap_free PROTO((void *, int));

void	chk_error PROTO(( void (*)() ));
void	*chk_pool_alloc(void **p, int size);
void	*chk_pool_zalloc(void **p, int size);
void	chk_pool_free(void *);
void	chk_resize(char **ptr, int *size, int n, int size_elem);

/**********************************************************************/
/*   Set  this  to  1 if you want to find out where things are being  */
/*   allocated from.						      */
/**********************************************************************/
# define	WHERE	0
			
/**********************************************************************/
/*   Set  following  to  1 if you need to allocate double quantities  */
/*   and  need  8-byte  alignment. This is more wasteful of space so  */
/*   only define it when absolutely necessary.			      */
/**********************************************************************/
# if !defined(NEED_DOUBLE_ALIGNMENT)
#	if defined(__sparc__) || defined(sparc) || CR_HPUX || CR_HPUX10
#		define	NEED_DOUBLE_ALIGNMENT	1
#	else
#		define	NEED_DOUBLE_ALIGNMENT	0
#	endif
# endif
/**********************************************************************/
/*   Set following to 0 if machine requires proper alignment.	      */
/**********************************************************************/
# if defined(i386)
# 	define	BYTE_ALIGNED	1
# else
# 	define	BYTE_ALIGNED	0
# endif

void	chk_init PROTO((void));
void	chk_exit PROTO((void));
void	chk_tail PROTO((void));

# if __cplusplus
}
# endif

# endif	/* _CHK_ALLOC_H */
