/**********************************************************************/
/*                                                                    */
/*      CRiSP - Programmable editor                                   */
/*      ===========================                                   */
/*                                                                    */
/*  File:          hexdigits.c                                        */
/*  Author:        P. D. Fox                                          */
/*  Created:       16 May 2001                                        */
/*                                                                    */
/*   Copyright (c) 2001-2021, Foxtrot Systems Ltd		      */
/*                All Rights Reserved.                                */
/*                                                                    */
/*                                                                    */
/*--------------------------------------------------------------------*/
/*  Description:  Simple routine to get two hexadecimal digits like in \xXX */
/*--------------------------------------------------------------------*/
/*  $Header: Last edited: 28-Oct-2021 1.2 $ 			      */
/**********************************************************************/

# include	<ctype.h>

/**********************************************************************/
/*   Function  to  decode one or two hex digits which tend to follow  */
/*   the backslash-x escape sequence.				      */
/**********************************************************************/
int
get_two_hex_digits(char **strp)
{
	char	*str = *strp;
	int	ch;
	int	byte;

	byte = *str++ & 0xff;
	if (isdigit(byte))
		byte -= '0';
	else if (byte >= 'A' && byte <= 'F')
		byte = byte - 'A' + 10;
	else if (byte >= 'a' && byte <= 'f')
		byte = byte - 'a' + 10;
	else
		return -1;
	/*--------------------------
	 *   Second digit.
	 *--------------------------*/
	ch = *str++ & 0xff;
	if (isdigit(ch))
		byte = (byte << 4) + ch - '0';
	else if (ch >= 'A' && ch <= 'F')
		byte = (byte << 4) + ch - 'A' + 10;
	else if (ch >= 'a' && ch <= 'f')
		byte = (byte << 4) + ch - 'a' + 10;
	else
		str--;

	*strp = str;
	return byte;
}

