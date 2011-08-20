# if 0
chk_free(void *p)
{
	free(p);
}
chk_free_ptr(void **p)
{
	if (*p) {
		free(*p);
		*p = 0;
	}
}
chk_strdup(char *str)
{
	return strdup(str);
}
chk_alloc(void *p)
{
	return malloc(p);
}
chk_realloc(int p, int s)
{
	return realloc(p, s);
}
# endif
