/**********************************************************************/
/*                                                                    */
/*	CRiSP - Programmable editor                                   */
/*	===========================                                   */
/*                                                                    */
/*  File:          hash.c                                             */
/*  Author:        P. D. Fox                                          */
/*  Created:       11 Oct 1993                     		      */
/*                                                                    */
/*  Copyright (c) 1993-2016, Paul David Fox                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Hash table library                                  */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 11-May-2016 1.13 $ 			      */
/**********************************************************************/

//# include	"../include/machine.h"
typedef short u_int32;
# include	<stdio.h>
# include	"../include/chkalloc.h"
# include	"../include/hash.h"
# include	"../include/crc32.h"
# 	include	<stdlib.h>
#	include	<unistd.h>
# include	<string.h>

# if !defined(TRUE)
#	define	TRUE	1
#	define	FALSE	0
# endif

# define	NEXT_HIGHEST_POWER_OF_2(x) \
	(x=x-1,x|=x>>1,x|=x>>2,x|=x>>4,x|=x>>8,x|=x>>16,x+1)
# define	HASH_MODULO(hp, x)	(int) (((int) (long) x) & ((hp)->h_size - 1))

# define CALLBACK_ATTRIBUTE
# define GET_32bit(x) ((x) & 0xffffffff)

static struct __hash_stats {
	unsigned long	clears;
	unsigned long	creates;
	unsigned long	destroys;
	unsigned long	add_string;
	unsigned long	add_string_loops;
	unsigned long	inserts;
	unsigned long	insert_loops;
	unsigned long	lookups;
	unsigned long	replaces;
	unsigned long	replace_loops;
	unsigned long	lookup_loops;
	unsigned long	removes;
	unsigned long	removes_int;
	unsigned long	resizes;
	unsigned long	inserts_int;
	unsigned long	insert_loops_int;
	unsigned long	lookups_int;
	unsigned long	lookup_loops_int;
	} __hash_stats;

# define	HASH_INCR(x)	__hash_stats.x++

static void	*hd_hash;

/**********************************************************************/
/*   Prototypes.						      */
/**********************************************************************/
int hash_insert2(hash_t *hp, char *key, void *data, int flag);

/**********************************************************************/
/*   Create a brand new hash table.				      */
/**********************************************************************/
DLL_EXPORT hash_t *
hash_create(int size, int incr)
{	hash_t	*hp;

	HASH_INCR(creates);

	hp = (hash_t *) chk_alloc(sizeof(hash_t));
	size = NEXT_HIGHEST_POWER_OF_2(size);
	hp->h_limit = 8 * size;
	hp->h_size = size;
	hp->h_incr = incr;
	hp->h_count = 0;

	hp->h_table = (hash_element_t **) chk_zalloc(
		hp->h_size * sizeof(hash_element_t *));
	return hp;
}
/**********************************************************************/
/*   Like  hash_int_insert, but optimised to avoid needing to lookup  */
/*   and then insert on failed lookup.				      */
/**********************************************************************/
int
hash_add_string(hash_t *hp, int flags, char *key, void *data)
{	unsigned long	cksum;
	hash_element_t	*hep;
	int	i;
	u_int32	c;

	HASH_INCR(add_string);
	c = crc32((unsigned char *) key, (int) strlen(key));
	cksum = GET_32bit(c);
	i = HASH_MODULO(hp, cksum);
	hep = hp->h_table[i];

	if (hep != (hash_element_t *) NULL) {
		/***********************************************/
		/*   Check  the  chain  to  see  if we have a  */
		/*   duplicate entry.			       */
		/***********************************************/
		while (hep) {
			if (hep->he_hash == cksum &&
			    strcmp(key, hep->he_key) == 0)
			    	return FALSE;
			HASH_INCR(add_string_loops);
			hep = hep->he_next;
			}
		}
	hep = (hash_element_t *) vm_alloc(sizeof(hash_element_t), &hd_hash);
	hep->he_next = hp->h_table[i];
	if (flags & HF_STRDUP_KEY)
		key = chk_strdup(key);
	hep->he_key = key;	      
	hep->he_data = data;
	hep->he_hash = cksum;
	hp->h_table[i] = hep;

	/***********************************************/
	/*   if table getting too large then its time  */
	/*   to resize it.			       */
	/***********************************************/
	if (++hp->h_count >= hp->h_limit)
		hash_resize(hp);
	return TRUE;
}
/**********************************************************************/
/*   Clear a hash table.					      */
/**********************************************************************/
void
hash_clear(hash_t *hp, int flags)
{	int	i;
	hash_element_t	*hep, *hep1;

	HASH_INCR(clears);

	for (i = 0; i < hp->h_size; i++) {
		if ((hep = hp->h_table[i]) == (hash_element_t *) NULL)
			continue;
		hp->h_table[i] = NULL;
		while (hep) {
			hep1 = hep->he_next;
			if (flags & HF_FREE_KEY)
				chk_free((void *) hep->he_key);
			if (flags & HF_FREE_DATA)
				chk_free((void *) hep->he_data);
			vm_free((void *) hep, &hd_hash);
			hep = hep1;
			}
		}
	hp->h_count = 0;
}
/**********************************************************************/
/*   Free a hash table.						      */
/**********************************************************************/
void
hash_destroy(hash_t *hp, int flags)
{
	HASH_INCR(destroys);

	hash_clear(hp, flags);
	chk_free((void *) hp->h_table);
	chk_free((void *) hp);
}
/**********************************************************************/
/*   Like hash_lookup but a non-null terminated string.		      */
/**********************************************************************/
hash_element_t *
hash_find_str(hash_t *hp, char *key)
{
	return hash_findn(hp, key, (int) strlen(key));
}
hash_element_t *
hash_findn(hash_t *hp, char *key, int len)
{	unsigned long	cksum;
	hash_element_t	*hep;
	u_int32	c;

	HASH_INCR(lookups);

	/***********************************************/
	/*   If  table not initialised then we cannot  */
	/*   find it.				       */
	/***********************************************/
	if (hp == NULL)
		return NULL;

	c = crc32((unsigned char *) key, len);
	cksum = GET_32bit(c);
	hep = hp->h_table[HASH_MODULO(hp, cksum)];
	if (hep == NULL)
		return NULL;

	/***********************************************/
	/*   Is it located on this list entry.	       */
	/***********************************************/
	while (hep) {
		if (hep->he_hash == cksum &&
		    strncmp(key, hep->he_key, len) == 0 &&
		    hep->he_key[len] == '\0') {
		    	return hep;
			}
		HASH_INCR(lookup_loops);
		hep = hep->he_next;
		}
	return NULL;
}
/**********************************************************************/
/*   Add or count how many are being inserted.			      */
/**********************************************************************/
int
hash_increment(hash_t *hp, char *key)
{
	return hash_insert2(hp, key, (void *) 1, 1);
}
/**********************************************************************/
/*   Insert a new element into the hash table. Don't allow duplicate  */
/*   keys. Return success/failure to caller.			      */
/**********************************************************************/
int
hash_insert(hash_t *hp, char *key, void *data)
{
	return hash_insert2(hp, key, data, 0);
}
int
hash_insert2(hash_t *hp, char *key, void *data, int flag)
{	unsigned long	cksum;
	hash_element_t	*hep;
	int	i;
	u_int32	c;

	HASH_INCR(inserts);
	c = crc32((unsigned char *) key, (int) strlen(key));
	cksum = GET_32bit(c);
	i = HASH_MODULO(hp, cksum);
	hep = hp->h_table[i];

	if (hep != (hash_element_t *) NULL) {
		/***********************************************/
		/*   Check  the  chain  to  see  if we have a  */
		/*   duplicate entry.			       */
		/***********************************************/
		while (hep) {
			if (hep->he_hash == cksum &&
			    strcmp(key, hep->he_key) == 0) {
			    	if (flag)
					hep->he_data = (void *) ((long) hep->he_data + 1);
			    	return FALSE;
				}
			HASH_INCR(insert_loops);
			hep = hep->he_next;
			}
		}
	hep = (hash_element_t *) vm_alloc(sizeof(hash_element_t), &hd_hash);
	hep->he_next = hp->h_table[i];
	hep->he_key = key;
	hep->he_data = data;
	hep->he_hash = cksum;
	hp->h_table[i] = hep;

	/***********************************************/
	/*   if table getting too large then its time  */
	/*   to resize it.			       */
	/***********************************************/
	if (++hp->h_count >= hp->h_limit)
		hash_resize(hp);
	return TRUE;
}
/**********************************************************************/
/*   Insert a new element into the hash table. Don't allow duplicate  */
/*   keys. Return success/failure to caller. Key is a plain integer.  */
/**********************************************************************/
int
hash_int_insert(hash_t *hp, long key, void *data)
{
	return hash_ptr_insert(hp, (void *) key, data);
}
/**********************************************************************/
/*   Lookup an entry in the table.				      */
/**********************************************************************/
DLL_EXPORT void *
hash_int_lookup(hash_t *hp, long key)
{
	return hash_ptr_lookup(hp, (void *) key);
}

/**********************************************************************/
/*   Remove an entry from the hash table.			      */
/**********************************************************************/
DLL_EXPORT int
hash_int_remove(hash_t *hp, long key, int flags)
{
	return hash_ptr_remove(hp, (void *) key, flags);

}
/**********************************************************************/
/*   Lookup an entry in the table.				      */
/**********************************************************************/
void *
hash_lookup(hash_t *hp, char *key)
{
	/***********************************************/
	/*   If  table not initialised then we cannot  */
	/*   find it.				       */
	/***********************************************/
	if (hp == (hash_t *) NULL)
		return (void *) NULL;

	return hash_lookupn(hp, key, (int) strlen(key));
}
int
hash_lookup2(hash_t *hp, char *key, void **retp)
{	unsigned long	cksum;
	hash_element_t	*hep;
	u_int32	c;

	HASH_INCR(lookups);

	c = crc32((unsigned char *) key, (int) strlen(key));
	cksum = GET_32bit(c);
	hep = hp->h_table[HASH_MODULO(hp, cksum)];
	if (hep == (hash_element_t *) NULL) {
		return FALSE;
		}
	/***********************************************/
	/*   Is it located on this list entry.	       */
	/***********************************************/
	while (hep) {
		if (hep->he_hash == cksum &&
		    strcmp(key, hep->he_key) == 0) {
		    	*retp = hep->he_data;
		    	return TRUE;
			}
		HASH_INCR(lookup_loops);
		hep = hep->he_next;
		}
	return FALSE;
}
/**********************************************************************/
/*   Like hash_lookup but a non-null terminated string.		      */
/**********************************************************************/
void *
hash_lookupn(hash_t *hp, char *key, int len)
{	unsigned long	cksum;
	hash_element_t	*hep;
	u_int32	c;

	HASH_INCR(lookups);

	/***********************************************/
	/*   If  table not initialised then we cannot  */
	/*   find it.				       */
	/***********************************************/
	if (hp == (hash_t *) NULL)
		return (void *) NULL;

	c = crc32((unsigned char *) key, len);
	cksum = GET_32bit(c);
	hep = hp->h_table[HASH_MODULO(hp, cksum)];
	if (hep == (hash_element_t *) NULL) {
		return (void *) NULL;
		}
	/***********************************************/
	/*   Is it located on this list entry.	       */
	/***********************************************/
	while (hep) {
		if (hep->he_hash == cksum &&
		    strncmp(key, hep->he_key, len) == 0 &&
		    hep->he_key[len] == '\0') {
		    	return hep->he_data;
			}
		HASH_INCR(lookup_loops);
		hep = hep->he_next;
		}
	return (void *) NULL;
}
int
hash_ptr_insert(hash_t *hp, void *key, void *data)
{	hash_element_t	*hep;
	int	i;

	HASH_INCR(inserts_int);
	i = HASH_MODULO(hp, key);
	hep = hp->h_table[i];
	if (hep != (hash_element_t *) NULL) {
		/***********************************************/
		/*   Check  the  chain  to  see  if we have a  */
		/*   duplicate entry.			       */
		/***********************************************/
		while (hep) {
			if (hep->he_key == key)
			    	return FALSE;
			HASH_INCR(insert_loops_int);
			hep = hep->he_next;
			}
		}
	hep = (hash_element_t *) vm_alloc(sizeof(hash_element_t), &hd_hash);
	hep->he_next = hp->h_table[i];
	hep->he_key = key;
	hep->he_data = data;
	hep->he_hash = (unsigned long) key;
	hp->h_table[i] = hep;

	/***********************************************/
	/*   if table getting too large then its time  */
	/*   to resize it.			       */
	/***********************************************/
	if (++hp->h_count >= hp->h_limit)
		hash_resize(hp);
	return TRUE;
}
/**********************************************************************/
/*   Handle opaque pointers as keys.				      */
/**********************************************************************/
DLL_EXPORT void *
hash_ptr_lookup(hash_t *hp, void *key)
{	hash_element_t	*hep;

	HASH_INCR(lookups_int);

	hep = hp->h_table[HASH_MODULO(hp, key)];
	if (hep == (hash_element_t *) NULL)
		return (void *) NULL;
	/***********************************************/
	/*   Is it located on this list entry.	       */
	/***********************************************/
	while (hep) {
		if (hep->he_hash == (unsigned long) key)
		    	return hep->he_data;
		HASH_INCR(lookup_loops_int);
		hep = hep->he_next;
		}
	return (void *) NULL;
}
/**********************************************************************/
/*   Handle generic ptr removal.				      */
/**********************************************************************/
DLL_EXPORT int
hash_ptr_remove(hash_t *hp, void *key, int flags)
{	unsigned long	cksum;
	hash_element_t	*hep;
	hash_element_t	*hep_prev;
	int	i;

	HASH_INCR(removes_int);

	cksum = (unsigned long) key;
	i = HASH_MODULO(hp, cksum);
	hep = hp->h_table[i];
	if (hep == (hash_element_t *) NULL)
		return 0;
	/***********************************************/
	/*   First entry on the list?		       */
	/***********************************************/
	if (hep->he_hash == cksum) {
	    	hp->h_table[i] = hep->he_next;
		if (flags & HF_FREE_KEY)
			chk_free((void *) hep->he_key);
		if (flags & HF_FREE_DATA)
			chk_free((void *) hep->he_data);
		vm_free((void *) hep, &hd_hash);
		hp->h_count--;
		return 1;
	    	}
	hep_prev = hep;
	hep = hep->he_next;
	/***********************************************/
	/*   Is it located on this list entry.	       */
	/***********************************************/
	while (hep) {
		if (hep->he_hash == cksum) {
		    	hep_prev->he_next = hep->he_next;
			if (flags & HF_FREE_KEY)
				chk_free((void *) hep->he_key);
			if (flags & HF_FREE_DATA)
				chk_free((void *) hep->he_data);
			vm_free((void *) hep, &hd_hash);
			hp->h_count--;
		    	return 1;
			}
		hep_prev = hep;
		hep = hep->he_next;
		}
	return 0;
}
/**********************************************************************/
/*   Remove an entry from the hash table.			      */
/**********************************************************************/
int
hash_remove_entry(hash_t *hp, char *key, int flags)
{	unsigned long	cksum;
	hash_element_t	*hep;
	hash_element_t	*hep_prev;
	int	i;
	u_int32	c;

	HASH_INCR(removes);

	c = crc32((unsigned char *) key, (int) strlen(key));
	cksum = GET_32bit(c);
	i = HASH_MODULO(hp, cksum);
	hep = hp->h_table[i];
	if (hep == (hash_element_t *) NULL)
		return 0;
	/***********************************************/
	/*   First entry on the list?		       */
	/***********************************************/
	if (hep->he_hash == cksum &&
	    strcmp(hep->he_key, key) == 0) {
	    	hp->h_table[i] = hep->he_next;
		if (flags & HF_FREE_KEY)
			chk_free((void *) hep->he_key);
		if (flags & HF_FREE_DATA)
			chk_free((void *) hep->he_data);
		vm_free((void *) hep, &hd_hash);
		hp->h_count--;
		return 1;
	    	}
	hep_prev = hep;
	hep = hep->he_next;
	/***********************************************/
	/*   Is it located on this list entry.	       */
	/***********************************************/
	while (hep) {
		if (hep->he_hash == cksum &&
		    strcmp(key, hep->he_key) == 0) {
		    	hep_prev->he_next = hep->he_next;
			if (flags & HF_FREE_KEY)
				chk_free((void *) hep->he_key);
			if (flags & HF_FREE_DATA)
				chk_free((void *) hep->he_data);
			vm_free((void *) hep, &hd_hash);
			hp->h_count--;
		    	return 1;
			}
		hep_prev = hep;
		hep = hep->he_next;
		}
	return 0;
}
/**********************************************************************/
/*   Replace  an  existing  item,  but  return the old value because  */
/*   caller may need to free it.				      */
/**********************************************************************/
long 
hash_int_replace(hash_t *hp, char *key, long val)
{
	return (long) hash_replace(hp, key, (void *) val);
}
void *
hash_replace(hash_t *hp, char *key, void *data)
{	unsigned long	cksum;
	hash_element_t	*hep;
	int	i;
	u_int32	c;

	HASH_INCR(replaces);
	c = crc32((unsigned char *) key, (int) strlen(key));
	cksum = GET_32bit(c);
	i = HASH_MODULO(hp, cksum);
	hep = hp->h_table[i];

	if (hep != (hash_element_t *) NULL) {
		/***********************************************/
		/*   Check  the  chain  to  see  if we have a  */
		/*   duplicate entry.			       */
		/***********************************************/
		while (hep) {
			if (hep->he_hash == cksum &&
			    strcmp(key, hep->he_key) == 0) {
			    	void *old_data = hep->he_data;
				hep->he_data = data;
				return old_data;
				}
			HASH_INCR(replace_loops);
			hep = hep->he_next;
			}
		}
	hep = (hash_element_t *) vm_alloc(sizeof(hash_element_t), &hd_hash);
	hep->he_next = hp->h_table[i];
	hp->h_table[i] = hep;

	hep->he_key = key;
	hep->he_data = data;
	hep->he_hash = cksum;

	/***********************************************/
	/*   if table getting too large then its time  */
	/*   to resize it.			       */
	/***********************************************/
	if (++hp->h_count >= hp->h_limit)
		hash_resize(hp);
	return NULL;
}
/**********************************************************************/
/*   Function  to  resize  the  hash table because it looks like its  */
/*   getting too large.						      */
/**********************************************************************/
void
hash_resize(hash_t *hp)
{	hash_element_t	*hep, *hep1;
	hash_t	new_hash;
	int	i, j;
	int	t;

	HASH_INCR(resizes);

	/***********************************************/
	/*   We do this by creating a brand new table  */
	/*   and  moving  elements from the old table  */
	/*   to the new one.			       */
	/***********************************************/
	new_hash = *hp;
	if (new_hash.h_incr < 0)
		new_hash.h_size *= -new_hash.h_incr;
	else
		new_hash.h_size += new_hash.h_incr;
	/***********************************************/
	/*   Be careful of side effects on next line.  */
	/***********************************************/
	t = NEXT_HIGHEST_POWER_OF_2(new_hash.h_size);
	new_hash.h_size = t;
	new_hash.h_limit = 8 * new_hash.h_size;
	new_hash.h_table = (hash_element_t **) chk_zalloc(
		new_hash.h_size * sizeof(hash_element_t *));

	for (i = 0; i < hp->h_size; i++) {
		if ((hep = hp->h_table[i]) == (hash_element_t *) NULL)
			continue;
		while (hep) {
			hep1 = hep->he_next;
			j = HASH_MODULO(&new_hash, hep->he_hash);
			hep->he_next = new_hash.h_table[j];
			new_hash.h_table[j] = hep;
			hep = hep1;
			}
		}
	chk_free((void *) hp->h_table);
	*hp = new_hash;
}
/**********************************************************************/
/*   Return a linear array of items in the hash table. This makes it  */
/*   easier  to  perform  a  'for'  loop  on  each item in the table  */
/*   without recourse to callbacks.				      */
/**********************************************************************/
static int compare_flags;

static int CALLBACK_ATTRIBUTE
hash_compare(hash_element_t **hp1, hash_element_t **hp2)
{	char	*p1 = (*hp1)->he_key;
	char	*p2 = (*hp2)->he_key;

	if (compare_flags & (HASH_UINT_COMPARE | HASH_UINT_COMPARE_DATA | 
	    HASH_INT_COMPARE | HASH_INT_COMPARE_DATA)) {
		if (compare_flags & HASH_UINT_COMPARE) {
			unsigned long u1 = (unsigned long) p1;
			unsigned long u2 = (unsigned long) p2;
			return u1 == u2 ? 0 :
				u1 < u2 ? -1 : 1;
			}
		if (compare_flags & HASH_INT_COMPARE) {
			long i1 = (long) p1;
			long i2 = (long) p2;
			return i1 == i2 ? 0 :
				i1 < i2 ? -1 : 1;
			}
		return (long) (*hp1)->he_data - (long) (*hp2)->he_data;
		}

	if (*p1 != *p2)
		return *p1 - *p2;
	return strcmp(p1, p2);
}

hash_element_t **
hash_linear(hash_t *hp, int flags)
{	hash_element_t	**tbl;
	hash_element_t	*hep;
	int	i, j;

	tbl = (hash_element_t **) chk_alloc((hp->h_count + 1) * sizeof(hash_element_t *));
	j = 0;
	for (i = 0; i < hp->h_size; i++) {
		for (hep = hp->h_table[i]; hep; hep = hep->he_next) {
			tbl[j++] = hep;
			}
		}
	if (flags & (HASH_SORTED | HASH_UINT_COMPARE | 
	      HASH_UINT_COMPARE_DATA | HASH_INT_COMPARE | HASH_INT_COMPARE_DATA)) {
		compare_flags = flags;
		qsort(tbl, hp->h_count, sizeof(hash_element_t *),
			(int (CALLBACK_ATTRIBUTE *)()) hash_compare);
		}
	return tbl;
}

hash_iter_t *
hash_iterate_start(hash_t *h, int flags)
{	hash_iter_t *hi = chk_zalloc(sizeof *hi);

	hi->hi_hash = h;
	hi->hi_array = hash_linear(h, flags);
	hi->hi_n = 0;
	hi->hi_size = hash_size(h);

	return hi;
}
hash_element_t *
hash_iterate_next(hash_iter_t *hi)
{
	if (hi->hi_n >= hi->hi_size)
		return NULL;

	return hi->hi_array[hi->hi_n++];
}
void
hash_iterate_finish(hash_iter_t *hi)
{
	chk_free(hi->hi_array);
	chk_free(hi);
}


