/**********************************************************************/
/*                                                                    */
/*	CRiSP - Programmable editor                                   */
/*	===========================                                   */
/*                                                                    */
/*  File:          hash.h                                             */
/*  Author:        P. D. Fox                                          */
/*  Created:       11 Oct 1993                     		      */
/*                                                                    */
/*  Copyright (c) 1993-2010, Paul David Fox                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Hash table library                                  */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 02-Jul-2010 1.8 $ 			      */
/**********************************************************************/

# if !defined(HASH_H_INCLUDE_FILE)
# define	HASH_H_INCLUDE_FILE

# if defined(__cplusplus)
extern "C" {
# endif

/**********************************************************************/
/*   Following  macro  used  to  allow  compilation  with ANSI C and  */
/*   non-ANSI C compilers automatically.			      */
/**********************************************************************/
# if !defined(PROTO)
# 	if	defined(__STDC__) || HAVE_PROTOTYPES
#		define	PROTO(x)	x
#	else
#		define	PROTO(x)	()
#	endif
# endif

/**********************************************************************/
/*   Hash table element.					      */
/**********************************************************************/
typedef struct hash_element_t {
	struct hash_element_t	*he_next;	/* Next in list.	*/
	char	*he_key;
	void	*he_data;
	unsigned long	he_hash;		/* Hash value		*/
	} hash_element_t;
/**********************************************************************/
/*   Header structure for a hash table.				      */
/**********************************************************************/
typedef struct hash_t {
	int	h_limit;	/* When h_count exceeds this, increase  */
				/* table size.				*/
	int	h_incr;		/* When need to resize table use this value */
				/* If value is < 0 then abs(h_incr) is  */
				/* multiplier factor.			*/
	int	h_size;		/* Current size of table.		*/
	int	h_count;	/* Current number of elements in table	*/

	hash_element_t	**h_table; /* Actual hash table.			*/
	} hash_t;

/**********************************************************************/
/*   Iteration support.						      */
/**********************************************************************/
typedef struct hash_iter_t {
	hash_t		*hi_hash;
	hash_element_t **hi_array;
	int	hi_n;
	int	hi_size;
	} hash_iter_t;

/**********************************************************************/
/*   Flags for hash_destroy().					      */
/**********************************************************************/
# define	HF_FREE_KEY	0x01
# define	HF_FREE_DATA	0x02
# define	HF_STRDUP_KEY	0x04

/**********************************************************************/
/*   Flags for hash_linear()					      */
/**********************************************************************/
# define	HASH_SORTED		0x01
# define	HASH_INT_COMPARE	0x02
# define	HASH_INT_COMPARE_DATA	0x04
# define	HASH_UINT_COMPARE	0x08
# define	HASH_UINT_COMPARE_DATA	0x10

#if !defined(DLL_EXPORT)
# define DLL_EXPORT
#endif

DLL_EXPORT hash_t	*hash_create(int, int);
void	hash_clear(hash_t *, int);
void	hash_destroy(hash_t *, int);
int	hash_remove_entry(hash_t *, char *, int);
int	hash_increment(hash_t *, char *);
int	hash_insert(hash_t *, char *, void *);
long	hash_int_replace(hash_t *, char *, long);
void	*hash_replace(hash_t *, char *, void *);
void	hash_resize(hash_t *);
hash_element_t *hash_find_str(hash_t *, char *);
hash_element_t *hash_findn(hash_t *, char *, int);
void	*hash_lookup(hash_t *, char *);
void	*hash_lookupn(hash_t *, char *, int);
int	hash_lookup2(hash_t *, char *, void **);
DLL_EXPORT int	hash_int_insert(hash_t *, long, void *);
DLL_EXPORT void	*hash_int_lookup(hash_t *, long);
DLL_EXPORT int	hash_int_remove(hash_t *, long, int);
DLL_EXPORT int	hash_ptr_insert(hash_t *, void *, void *);
DLL_EXPORT void	*hash_ptr_lookup(hash_t *, void *);
DLL_EXPORT int hash_ptr_remove(hash_t *hp, void *key, int flags);
hash_element_t	**hash_linear(hash_t *, int);
int	hash_add_string(hash_t *, int, char *, void *);
hash_iter_t	*hash_iterate_start(hash_t *h, int flags);
hash_element_t	*hash_iterate_next(hash_iter_t *hi);
void		hash_iterate_finish(hash_iter_t *hi);

# define	hash_size(hp)	(hp)->h_count
# define	hash_key(hep)	(hep)->he_key
# define	hash_data(hep)	(hep)->he_data

# if defined(__cplusplus)
}
# endif

# endif /* !defined(HASH_H_INCLUDE_FILE) */
