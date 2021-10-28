/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          dstr.c                                             */
/*  Author:        P. D. Fox                                          */
/*  Created:       14 Nov 1998                                        */
/*                                                                    */
/*  Copyright (c) 1998-2012, Foxtrot Systems Ltd                      */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Dynamic string library                              */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 29-Dec-2012 1.7 $ 			      */
/**********************************************************************/

# include	<stdio.h>
# include	<stdlib.h>
# include	<string.h>
# include	<assert.h>
# include	"../include/dstr.h"

# define DLL_EXPORT
# define EXPORT_FUNCTION
typedef unsigned char LIST;
typedef unsigned long long u_int64;

# define sizeof_FLOAT	9
# define	F_HALT 		0
# define	F_NULL 		1
# define	F_FLOAT		2
# define	F_CONST_64	3
# define	F_INT		4
# define	F_CONST_8	5

# define LPUT_ID(p, v) LPUT16(p, v)
# define LPUT8(p, v) *(p) = v
# define LPUT16(p, v) *((short *) p) = v
# define LPUT24(p, v) memcpy(p, &v, 3)
# define LPUT32(p, v) *(int *) (p) = v
# define LPUT_PTR(p, v) *(void **) (p) = v
# define LPUT64(p, v) *((long *) p) = v
# define LPUT_FLOAT(p, v) *((float *) p) = v
# define CM_POINTER_SIZE sizeof(void *)
# define	TYPE_uint64 u_int64

void *chk_alloc();
void *chk_realloc();
void	*chk_zalloc();
void	chk_free_ptr();


DLL_EXPORT void EXPORT_FUNCTION
dstr_init(dstr_t *strp, size_t size)
{
	strp->dl_str = chk_alloc(size);
	strp->dl_size = size;
	strp->dl_offset = 0;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_static_init(dstr_t *strp, void *ptr, int size)
{
	strp->dl_str = ptr;
	strp->dl_size = size;
	strp->dl_offset = size;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_clear(dstr_t *dstr)
{
	dstr->dl_offset = 0;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_zinit(dstr_t *strp, int size)
{
	strp->dl_str = chk_zalloc(size);
	strp->dl_size = size;
	strp->dl_offset = 0;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_free(dstr_t *strp)
{
	chk_free_ptr((void **) &strp->dl_str);
	strp->dl_offset = 0;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_add_char(dstr_t *dstr, int ch)
{
	if (dstr->dl_offset + 1 >= dstr->dl_size) {
		dstr->dl_size += 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	dstr->dl_str[dstr->dl_offset++] = ch;
}
/**********************************************************************/
/*   Add a builtin opcode reference.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_id(dstr_t *dstr, int val)
{	char	*cp;

	if (dstr->dl_offset + 2 >= dstr->dl_size) {
		dstr->dl_size += 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	LPUT_ID((LIST *) cp, (int) val);
	dstr->dl_offset += 2;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_int(dstr_t *dstr, long val)
{	int	size;
	int	type;
	char	*cp;

	if (val >= -127 && val <= 127) {
		type = F_CONST_8;
		size = 2;
		}
	else {
		type = F_INT;
		size = 5;
		}
	if (dstr->dl_offset + size >= dstr->dl_size) {
		dstr->dl_size += 64 + size;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	*cp = type;
	if (type == F_CONST_8)
		LPUT8((LIST *) cp+1, (char) val);
	else
		LPUT32((LIST *) cp+1, val);
	dstr->dl_offset += size;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_int_2(dstr_t *dstr, long val1, long val2)
{
	dstr_add_int(dstr, val1);
	dstr_add_int(dstr, val2);
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_int16(dstr_t *dstr, long val)
{	char	*cp;

	if (dstr->dl_offset + 2 >= dstr->dl_size) {
		dstr->dl_size += 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	LPUT16((LIST *) cp, (int) val);
	dstr->dl_offset += 2;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_add_int24(dstr_t *dstr, long val)
{	char	*cp;

	if (dstr->dl_offset + 3 >= dstr->dl_size) {
		dstr->dl_size += 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	LPUT24((LIST *) cp, val);
	dstr->dl_offset += 3;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_add_int32(dstr_t *dstr, long val)
{	char	*cp;

	if (dstr->dl_offset + 4 >= dstr->dl_size) {
		dstr->dl_size += 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	LPUT32((LIST *) cp, val);
	dstr->dl_offset += 4;
}
/**********************************************************************/
/*   Add a 64-bit int to the obstack.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_int64(dstr_t *dstr, TYPE_int64 val)
{	char	*cp;

	if (dstr->dl_offset + 8 >= dstr->dl_size) {
		dstr->dl_size += 8;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	LPUT64((LIST *) cp, val);
	dstr->dl_offset += 8;
}
/**********************************************************************/
/*   Add a variable sized integer.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_integer(dstr_t *dstr, TYPE_int64 val)
{	char	*cp;

	/***********************************************/
	/*   A   number   like   3,000,000,000  looks  */
	/*   negative  in  32-bits  but  is +ve. Also  */
	/*   optimise for small negative numbers.      */
	/***********************************************/
  	if ((val & ~(TYPE_uint64) 0x7fffffff) == 0 &&
	    val > -1000000) {
		dstr_add_int(dstr, (long) val);
		return;
		}
	if (dstr->dl_offset + 1 + 8 >= dstr->dl_size) {
		dstr->dl_size += 1 + 8;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	*cp = F_CONST_64;
	LPUT64((LIST *) cp + 1, val);
	dstr->dl_offset += 9;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_add_float(dstr_t *dstr, double val)
{	char	*cp;

	if (dstr->dl_offset + sizeof_FLOAT >= dstr->dl_size) {
		dstr->dl_size += 64 + sizeof_FLOAT;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	*cp = F_FLOAT;
	LPUT_FLOAT((LIST *) cp+1, val);
	dstr->dl_offset += sizeof_FLOAT;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_add_mem(dstr_t *dstr, char *mem, size_t size)
{
	if (dstr->dl_offset + size >= dstr->dl_size) {
		dstr->dl_size += dstr->dl_size / 10 + 64 + size;
		if (dstr->dl_str == NULL)
			dstr->dl_str = (char *) chk_alloc(dstr->dl_size);
		else
			dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	memcpy(&dstr->dl_str[dstr->dl_offset], mem, size);
	dstr->dl_offset += size;
}
/**********************************************************************/
/*   Add a bunch of chars in one go.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_nchar(dstr_t *dstr, int ch, int num)
{
	if (dstr->dl_offset + num >= dstr->dl_size) {
		dstr->dl_size += num + 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	while (num-- > 0) {
		dstr->dl_str[dstr->dl_offset++] = ch;
		}
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_null(dstr_t *dstr)
{
	if (dstr->dl_offset + 2 >= dstr->dl_size) {
		dstr->dl_size += 8;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	dstr->dl_str[dstr->dl_offset++] = F_NULL;
}
/**********************************************************************/
/*   Add a pointers. Pointers are machine dependent in size.	      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_ptr(dstr_t *dstr, int type, char *str)
{	char	*cp;

	if (dstr->dl_offset + CM_POINTER_SIZE >= dstr->dl_size) {
		dstr->dl_size += 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	cp = &dstr->dl_str[dstr->dl_offset];
	cp[0] = type;
	LPUT_PTR((LIST *) cp+1, str);
	dstr->dl_offset += CM_POINTER_SIZE;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_add_string(dstr_t *dstr, char *str, int len)
{
	if (dstr->dl_offset + len >= dstr->dl_size) {
		dstr->dl_size += len + 64;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	memcpy(&dstr->dl_str[dstr->dl_offset], str, len);
	dstr->dl_offset += len;
}

/**********************************************************************/
/*   Remove stuff at front of buffer.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_remove(dstr_t *dstr, int num)
{
	if (dstr->dl_offset <= num) {
		dstr->dl_offset = 0;
		return;
		}
	memmove(dstr->dl_str, dstr->dl_str + num, dstr->dl_offset - num);
	dstr->dl_offset -= num;
}

DLL_EXPORT void EXPORT_FUNCTION
dstr_reserve(dstr_t *dstr, int size)
{
	if (dstr->dl_offset + size >= dstr->dl_size) {
		dstr->dl_size += 64 + size;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	dstr->dl_offset += size;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_reserve_zero(dstr_t *dstr, int size)
{
	if (dstr->dl_offset + size >= dstr->dl_size) {
		dstr->dl_size += 64 + size;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	memset(&dstr->dl_str[dstr->dl_offset], 0, size);
	dstr->dl_offset += size;
}

/**********************************************************************/
/*   Make sure we have enough room for the space allocated.	      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_resize(dstr_t *dstr, int size)
{
	if (dstr->dl_size >= size)
		return;

	if (dstr->dl_str == (char *) NULL)
		dstr->dl_str = (char *) chk_zalloc(size);
	else {
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, size);
		memset(&dstr->dl_str[dstr->dl_size], 0, size - dstr->dl_size);
		}
	dstr->dl_size = size;
}
/**********************************************************************/
/*   Prune back the size of the object.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_set_size(dstr_t *dstr, int size)
{
	assert(size <= dstr->dl_size);
	dstr->dl_offset = size;
}
/**********************************************************************/
/*   Allow us to set the various fields.			      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_setup(dstr_t *dstr, void *ptr, int used)
{
	dstr->dl_str = ptr;
	dstr->dl_offset = used;
	dstr->dl_size = used;
}
/**********************************************************************/
/*   Ensure buffer is a specific size.				      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_set_space(dstr_t *dstr, int size)
{
	if (dstr->dl_size == size)
		return;

	dstr->dl_size = size;
	dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
	dstr->dl_offset = 0;
}

/**********************************************************************/
/*   Ensure block is at least size bytes long.			      */
/**********************************************************************/
DLL_EXPORT void EXPORT_FUNCTION
dstr_size(dstr_t *dstr, int size)
{
	if (dstr->dl_size >= size)
		return;

	dstr_resize(dstr, size);
	dstr->dl_offset = size;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_terminate(dstr_t *dstr)
{
	if (dstr->dl_offset + 1 >= dstr->dl_size) {
		dstr->dl_size += 8;
		dstr->dl_str = (char *) chk_realloc(dstr->dl_str, dstr->dl_size);
		}
	dstr->dl_str[dstr->dl_offset++] = F_HALT;
}
DLL_EXPORT void EXPORT_FUNCTION
dstr_add_cstring(dstr_t *dstr, char *str, int len)
{	char	*spec = "\n\t\r\\\f\b";
	static char spec_map[] = "ntr\\fb";
	char	*cp;
	char	buf[20];

	while (len-- > 0) {
		switch (*str) {
		  case '"':
		  case '\\':
		  	dstr_add_char(dstr, '\\');
		  	dstr_add_char(dstr, *str);
			break;
		  default:
		  	if (*str == '\0')
		  		dstr_add_mem(dstr, "\\000", 4);
			else if ((cp = strchr(spec, *str)) != (char *) NULL) {
		  		dstr_add_char(dstr, spec_map[cp-spec]);
			  	dstr_add_char(dstr, *str);
				}
			else if (*str < ' ') {
				sprintf(buf, "\\x%02x", *str & 0xff);
				dstr_add_mem(dstr, buf, 4);
				}
			else
			  	dstr_add_char(dstr, *str);
			break;
		  }
		str++;
		}
}

