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
void *
chk_strdup(char *str)
{
	return strdup(str);
}
void *
chk_alloc(void *p)
{
	return malloc(p);
}
void *
chk_realloc(void *p, int s)
{
	return realloc(p, s);
}
# endif
