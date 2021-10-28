
/* This is not a real crc32 - but it is ok for the current purpose.
*/

int
crc32(char *s, int len)
{	int	sum = 0;

	while (len-- > 0)
		sum += *s++;
	return sum;
}
