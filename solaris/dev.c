# include	"psys.h"
# include	<sys/sysmacros.h>
# include	<sys/stat.h>
# include	<dirent.h>
# if CR_SOLARIS2_INTEL || CR_SOLARIS2_SPARC
#	include	<sys/fs/ufs_inode.h>
#	include	<sys/dnlc.h>
# endif
# define	NAME_SIZE	4096

char	*cache_file = "/tmp/proc.cache";

char	*name_buf;
struct dev_cache {
	dev_t	dev;
	char	*name;
	};
struct dev_cache *dev_tbl;
int	clone_dev = -1;
int	cache_size = 0;
char	*dev_name1();
int	cache_filled = FALSE;

char	*get_mount_device();
void	dev_cache_fill();

char	*
dev_name(dev)
dev_t	dev;
{
	if (!cache_filled) {
		cache_filled = TRUE;
		dev_cache_fill();
		}
		
	if (clone_dev == -1) {
		struct stat sbuf;
		if (stat("/dev/ticots", &sbuf) >= 0)
			clone_dev = major(sbuf.st_rdev);
		}
	if (clone_dev >= 0) {
		char *cp = dev_name1(makedev(clone_dev, major(dev)));
		if (*cp && cp[0] == '/')
			return cp;
		}
	return dev_name1(dev);
}
char *
dev_name1(dev)
register dev_t	dev;
{	register int i;
	static char buf[64];
	
 	for (i = 0; i < cache_size; i++)
 		if (dev_tbl[i].dev == dev) {
 			sprintf(buf, "/dev/%s", dev_tbl[i].name);
 			return buf;
 			}
	sprintf(buf, "%d,%d", major(dev), minor(dev));
	return buf;
}
void
dev_cache_fill()
{	struct stat sbuf;
	char	*np;
	DIR	*dirp;
	struct dirent *de;
	int	csize = 512;
	char	buf[64];
	int	c = 0;
	FILE	*fp;
	int	i;

	if (name_buf == (char *) NULL)
		name_buf = malloc(NAME_SIZE);
	np = name_buf;
	*np = '\0';

	/***********************************************/
	/*   See if we have a cache file first.	       */
	/***********************************************/
	if ((fp = fopen(cache_file, "r")) != (FILE *) NULL) {
		fread(name_buf, NAME_SIZE, 1, fp);
		dev_tbl = (struct dev_cache *) malloc(csize * sizeof(struct dev_cache));
		fread(dev_tbl, sizeof dev_tbl, 1, fp);
		fread(&cache_size, sizeof cache_size, 1, fp);
		for (i = 0; i < cache_size; i++)
			dev_tbl[i].name += (int) name_buf;
		fclose(fp);
		return;
		}

	if ((dirp = opendir("/dev")) == NULL)
		return;

	dev_tbl = (struct dev_cache *) malloc(csize * sizeof(struct dev_cache));
	strcpy(buf, "/dev/");
	while ((de = readdir(dirp)) != NULL) {
		if (np + strlen(de->d_name) + 2 >= &name_buf[NAME_SIZE])
			break;
		if (strcmp(de->d_name, ".") == 0 ||
		    strcmp(de->d_name, "..") == 0)
			continue;
		strcpy(buf + 5, de->d_name);
		if (stat(buf, &sbuf) < 0)
			continue;

		if (c + 1 >= csize)  {
			csize += 256;
			dev_tbl = (struct dev_cache *) realloc(dev_tbl, 
				csize * sizeof(struct dev_cache));
			}
		strcpy(np, de->d_name);

		dev_tbl[c].dev = sbuf.st_rdev;
		dev_tbl[c].name = np;
		np += strlen(np) + 1;
		c++;
		}
	closedir(dirp);
	cache_size = c;

	/***********************************************/
	/*   Save the cache for the next time we run.  */
	/***********************************************/
	if ((fp = fopen(cache_file, "w")) == (FILE *) NULL)
		return;
	fwrite(name_buf, NAME_SIZE, 1, fp);
	for (i = 0; i < cache_size; i++) {
		struct dev_cache d;
		d.dev = dev_tbl[i].dev;
		d.name = (char *) (dev_tbl[i].name - name_buf);
		fwrite(&d, sizeof d, 1, fp);
		}
	fwrite(&cache_size, sizeof cache_size, 1, fp);
	fclose(fp);
}
char *
filename_lookup(vno)
struct vnode *vno;
{
# if CR_LINUX_LIBC6
	return "";
# else
	static time_t last_time;
	time_t	tnow;
	static struct ncache	*ncache;
	static struct ncache *ncaddr;
	static int ncsize;
	struct ncache	*np, *npend;

	/***********************************************/
	/*   Read  DNLC  cache if not already read or  */
	/*   more than 1 second gone by.	       */
	/***********************************************/
	tnow = time((time_t *) NULL);
	if (ncsize == 0 || tnow != last_time) {
		if (ncsize == 0) {
			ncsize = get_ksym("*ncsize");
			ncache = (struct ncache *) malloc(ncsize * sizeof(ncache[0]));
			ncaddr = (struct ncache *) get_ksym("ncache");
			}
		get_struct((long) ncaddr, ncache, ncsize * sizeof(ncache[0]));
		last_time = tnow;
		}
	npend = &ncache[ncsize];
	for (np = ncache; np < npend; np++) {
		if (np->vp == vno && (np->namlen != 1 && np->name[0] != '.')) {
			np->name[np->namlen] = '\0';
			return np->name;
			}
		}
	return "";
# endif
}
void
print_filename(addr, vno)
long	addr;
struct vnode *vno;
{
# if CR_LINUX_LIBC6
# else
	char	*cp;
	struct inode inode;
	char	*dname;
	char	*mnt_device;

	get_struct((long) vno->v_data, &inode, sizeof inode);

	dname = dev_name(inode.i_dev);
	mnt_device = get_mount_device(dname);
	print(" [%s]", dname);
	cp = filename_lookup(addr);
	if (cp && *cp)
		print(" %s/.../%s", mnt_device, cp);
	else
		print(" %s/INODE#%d", mnt_device, inode.i_number);
# endif
}
char *
get_mount_device(name)
char	*name;
{	static int	fd = -1;
	char	buf[1024];
	static char buf1[64];
	char	*bufend;
	int	n;
	char	*cp, *cp1;
	int	len = strlen(name);

	if (fd < 0 && (fd = open("/etc/mnttab", O_RDONLY)) < 0)
		return name;
	lseek(fd, 0, 0);
	n = read(fd, buf, sizeof buf);
	if (n <= 0)
		return name;

	bufend = buf + n;
	cp = buf;
	while (cp < bufend) {
		if (strncmp(cp, name, len) != 0 ||
			(cp[len] != ' ' && cp[len] != '\t')) {
			while (cp < bufend && *cp != '\n')
				cp++;
			cp++;
			continue;
			}
		cp += len;
		strcpy(buf1, strtok(cp, " \t\n"));
		return buf1;
		}
	return name;
}

