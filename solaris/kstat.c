# include	"psys.h"
# include	<stdarg.h>
# include	<sys/stat.h>
# include	<kstat.h>

typedef struct map_t {
	int	val;
	char	*name;
	} map_t;

kstat_ctl_t	*kc;
map_t	types_tbl[] = {
	{KSTAT_TYPE_RAW,	"KSTAT_TYPE_RAW"},
	{KSTAT_TYPE_NAMED,	"KSTAT_TYPE_NAMED"},
	{KSTAT_TYPE_INTR,	"KSTAT_TYPE_INTR"},
	{KSTAT_TYPE_IO,		"KSTAT_TYPE_IO"},
	{KSTAT_TYPE_TIMER,	"KSTAT_TYPE_TIMER"},
	{0, 0}
	};

char	*map(map_t *, int);
void	list_stats(void);

int
main(int argc, char **argv)
{
	kc = kstat_open();
	if (kc == (kstat_ctl_t *) NULL) {
		perror("kstat_open");
		exit(1);
		}
	list_stats();
}

void
list_stats()
{	kstat_t	*ksp;

	kstat_chain_update(kc);
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		printf("\n%.*s.%.*s.%.*s:	%s\n",
			KSTAT_STRLEN, ksp->ks_module,
			KSTAT_STRLEN, ksp->ks_name,
			KSTAT_STRLEN, ksp->ks_class,
			map(types_tbl, ksp->ks_type));
		printf("   ");
		if (ksp->ks_flags & KSTAT_FLAG_VIRTUAL)
			printf("VIRTUAL ");
		if (ksp->ks_flags & KSTAT_FLAG_VAR_SIZE)
			printf("VAR_SIZE ");
		if (ksp->ks_flags & KSTAT_FLAG_WRITABLE)
			printf("WRITABLE ");
		if (ksp->ks_flags & KSTAT_FLAG_PERSISTENT)
			printf("PERSISTENT ");
		if (ksp->ks_flags & KSTAT_FLAG_DORMANT)
			printf("DORMANT ");
		if (ksp->ks_flags & KSTAT_FLAG_INVALID)
			printf("INVALID ");
		printf("\n");
		printf("	Size: 0x%08lx  Addr: 0x%08lx Prvaddr: 0x%08lx\n", 
			ksp->ks_data_size, ksp->ks_data, ksp->ks_private);

		switch (ksp->ks_type) {
		  case KSTAT_TYPE_NAMED: {
			kstat_named_t	*namep;
			int	n;

		  	kstat_read(kc, ksp, (void *) NULL);
			namep = (kstat_named_t *) ksp->ks_data;
			for (n = 0; n++ < ksp->ks_ndata; namep++) {
				printf("	%-16s ", namep->name);
				switch (namep->data_type) {
				  case KSTAT_DATA_CHAR:
				  	printf("\"%s\"", namep->value.c);
					break;
				  case KSTAT_DATA_LONG:
				  	printf("%8ldL", namep->value.l);
					break;
				  case KSTAT_DATA_ULONG:
				  	printf("%8luUL", namep->value.ul);
					break;
				  case KSTAT_DATA_LONGLONG:
				  	printf("%8ldLL", namep->value.l);
					break;
				  case KSTAT_DATA_ULONGLONG:
				  	printf("%8luULL", namep->value.ul);
					break;
				  case KSTAT_DATA_FLOAT:
				  	printf("%8f (float)", namep->value.f);
					break;
				  case KSTAT_DATA_DOUBLE:
				  	printf("%8f (double)", namep->value.d);
					break;
				  }

				printf("\n");
				}
		  	break;
			}
		  }
		}

	exit(0);
}

char *
map(map_t *mp, int val)
{
	static char buf[16];

	for ( ; mp->name; mp++) {
		if (mp->val == val)
			return mp->name;
		}

	sprintf(buf, "0x%08lx", val);
	return buf;
}
