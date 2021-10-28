/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          chkalloc2.c                                        */
/*  Author:        P. D. Fox                                          */
/*  Created:       26 Nov 2004                                        */
/*                                                                    */
/*  Copyright (c) 2004, Foxtrot Systems Ltd                           */
/*                All Rights Reserved.                                */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description: Lightweight chkalloc replacement.                    */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 05-Jan-2005 1.2 $ 			      */
/**********************************************************************/

# include	<stdio.h>
# include	<stdlib.h>
# include	<string.h>
# include	<strings.h>
# include	<memory.h>
# include	<stdarg.h>

#define DLL_EXPORT
#define EXPORT_FUNCTION
#define UNUSED_PARAMETER

void
chk_init()
{
}
DLL_EXPORT void *EXPORT_FUNCTION
check_alloc(unsigned int len, char *file)
{
	UNUSED_PARAMETER(file);

	return malloc(len);
}
DLL_EXPORT void *EXPORT_FUNCTION
chk_alloc(int size)
{	void	*ptr;

	ptr = malloc(size);
	if (ptr == (void *) NULL) {
		printf("CHK_ALLOC: Failed to allocate memory. Wanted 0x%x bytes\n", size);
		}
	return ptr;
}
DLL_EXPORT void *EXPORT_FUNCTION
chk_calloc(int size, int nel)
{
	return calloc(size, nel);
}
void *
chk_zalloc(int size)
{	void	*ptr;

	ptr = calloc(size, 1);
	if (ptr == (void *) NULL) {
		printf("CHK_ALLOC: Failed to zallocate memory. Wanted 0x%x bytes\n", size);
		}
	return ptr;
}
/**********************************************************************/
/*   Create a new string which is a concatenation.		      */
/**********************************************************************/
DLL_EXPORT char *
chk_makestr(const char *s1, ...)
{	int len;
	char *p, *q, *sn;
	va_list ap;

	len = strlen(s1);
	va_start(ap, s1);
	while (1) {
		sn = va_arg(ap, char *);
		if (!sn)
		    break;
		len += strlen(sn);
 		}
	va_end(ap);

	p = check_alloc(len + 1, "chk_makestr");
	strcpy(p, s1);
	q = p + strlen(p);

	va_start(ap, s1);
	while (1) {
		sn = va_arg(ap, char *);
		if (!sn)
		    break;
		strcpy(q, sn);
		q += strlen(q);
		}
	va_end(ap);

	return p;
}
void *
chk_realloc(char *ptr, int size)
{
	ptr = realloc(ptr, size);
	if (ptr == (void *) NULL) {
		printf("CHK_ALLOC: Failed to reallocate memory. Wanted 0x%x bytes\n", size);
		}
	return ptr;
}
void 
chk_free(void *ptr)
{
	free(ptr);
}
void
chk_free_ptr(void **ptr)
{
	if (*ptr) {
		chk_free(*ptr);
		*ptr = (void *) NULL;
		}
}
void
chk_exit()
{
}
void 
chk_free_safe(void *ptr)
{
	if (ptr)
		free(ptr);
}
/**********************************************************************/
/*   Checking version of the strdup() function.			      */
/**********************************************************************/
char *
chk_strdup(char	*str)
{
	return strdup(str);
}
char *
chk_strndup(char *str, int len)
{	char	*cp;

	cp = (char *) malloc(len);
	memcpy(cp, str, len);

	return cp;
}
/**********************************************************************/
/*   Called by main.c						      */
/**********************************************************************/
void
chk_error(func)
void	(*func)();
{
	UNUSED_PARAMETER(func);
}
/**********************************************************************/
/*   Called by crunch/crmain.c					      */
/**********************************************************************/
void
chk_dump()
{
}

void *
vm_alloc(int sz, void **pool)
{
	return chk_alloc(sz);
}

void
vm_free(void *m, void **pool)
{
	chk_free(m);
}
