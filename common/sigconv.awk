BEGIN		{
		    print "/* This file was automatically generated */"
		    print "/* by the awk script \"sigconv.awk\".      */\n"
		    print "struct map sigdesc[] = {\n"
		}

/^#define[ \t][ \t]*SIG[A-Z]+[ \t][0-9]+/	{
				if (substr($3, 1, 2) != "0x" && $3 < 256) {
					printf "    \"%s\",\t%s,\n", \
						substr($2, 4), $3
					}
				}

END				{
				    print "    (char *) NULL,\t 0\n};"
				}
