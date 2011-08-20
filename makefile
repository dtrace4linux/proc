#! /bin/make
CRISP=../../../crisp

PLATFORM_FLAGS=-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
CC=gcc -fno-builtin -fno-inline-functions-called-once -fno-inline -g -O -W -I../common -I. -I$(CRISP)/include -I$(PLATFORM) -D$(CR_TYPE) $(PLATFORM_FLAGS)
#CC=cc -I$(CRISP)/include -D$(CR_TYPE) -D_KMEMUSER
SIGNAL_H = /usr/include/sys/signal.h
CRISP_LIB=$(CRISP)/bin/foxlib.a 

H	= ../common/psys.h ../linux/graph.h include.h
OBJ=	\
	$(OBJDIR)/coldisp.o \
	$(OBJDIR)/commands.o \
	$(OBJDIR)/cpu.o \
	$(OBJDIR)/dev.o \
	$(OBJDIR)/disk.o \
	$(OBJDIR)/graph.o \
	$(OBJDIR)/graphs.o \
	$(OBJDIR)/kernel.o \
	$(OBJDIR)/meminfo.o \
	$(OBJDIR)/monitor.o \
	$(OBJDIR)/netstat.o \
	$(OBJDIR)/proc.o \
	$(OBJDIR)/screen.o \
	$(OBJDIR)/streams.o \
	$(OBJDIR)/softirqs.o \
	$(OBJDIR)/subs.o \
	$(OBJDIR)/vmstat.o \
	$(OBJDIR)/chkalloc.o

linux: the-world
	cd linux ; $(MAKE) OBJDIR=../bin.linux \
		KSTAT= \
		PLATFORM=linux CR_TYPE=CR_LINUX_GLIBC_X86_32 \
		LIBS="-lncurses -ldl"  -f ../makefile all

the-world:

sol2-intel:
	cd solaris ; $(MAKE) OBJDIR=../bin.sol2-intel \
		LIBS="-ltermcap -lsocket -lnsl -lkstat -lkvm -lelf" \
		KSTAT=../bin.sol2-intel/kstat \
		PLATFORM=solaris \
		PLATFORM_FLAGS=-D_KMEMUSER \
		CR_TYPE=CR_SOLARIS2_INTEL -f ../makefile all

sol2-sparc:
	cd solaris ; $(MAKE) OBJDIR=../bin.sol2-sparc \
		LIBS="-ltermcap -lsocket -lnsl -lkstat -ldl -lkvm -lelf" \
		KSTAT=../bin.sol2-sparc/kstat \
		PLATFORM=solaris \
		PLATFORM_FLAGS="-D_KMEMUSER -D_STRUCTURED_PROC" \
		CR_TYPE=CR_SOLARIS2_SPARC -f ../makefile all

all:	sigdesc.h $(OBJDIR)/proc $(KSTAT)

sigdesc.h: ../common/sigconv.awk $(SIGNAL_H)
	awk -f ../common/sigconv.awk $(SIGNAL_H) >sigdesc.h

$(OBJDIR)/proc: $(OBJDIR) $(OBJ)
#	setuid root rm -f $(OBJDIR)/proc
	$(PURIFY) $(CC) -o $(OBJDIR)/proc $(OBJ) $(CRISP_LIB) $(LIBS) 
	$(CC) -o $(OBJDIR)/monlist -DMAIN=1 monitor.c $(CRISP_LIB) $(LIBS)
	rm -f $(OBJDIR)/procmon ; ln -s proc $(OBJDIR)/procmon
#	setuid root chown root $(OBJDIR)/proc
#	setuid root chmod 4755 $(OBJDIR)/proc

$(OBJDIR)/kstat: $(OBJDIR) kstat.c
	$(CC) -o $(OBJDIR)/kstat kstat.c -lkstat

$(OBJDIR):
	-mkdir $(OBJDIR)

$(OBJDIR)/chkalloc.o:	../common/chkalloc.c $(H)
	$(CC) -c ../common/chkalloc.c
	mv chkalloc.o $(OBJDIR)

$(OBJDIR)/coldisp.o:	../common/coldisp.c $(H)
	$(CC) -c ../common/coldisp.c
	mv coldisp.o $(OBJDIR)

$(OBJDIR)/commands.o:	../common/commands.c $(H)
	$(CC) -c ../common/commands.c
	mv commands.o $(OBJDIR)

$(OBJDIR)/cpu.o:	cpu.c $(H)
	$(CC) -c cpu.c
	mv cpu.o $(OBJDIR)

$(OBJDIR)/dev.o:	dev.c $(H)
	$(CC) -c dev.c
	mv dev.o $(OBJDIR)

$(OBJDIR)/disk.o:	disk.c $(H)
	$(CC) -c disk.c
	mv disk.o $(OBJDIR)

$(OBJDIR)/graph.o:	graph.c $(H)
	$(CC) -c graph.c
	mv graph.o $(OBJDIR)

$(OBJDIR)/graphs.o:	graphs.c $(H)
	$(CC) -c graphs.c
	mv graphs.o $(OBJDIR)

$(OBJDIR)/kernel.o:	kernel.c $(H)
	$(CC) -c kernel.c
	mv kernel.o $(OBJDIR)

$(OBJDIR)/meminfo.o:	meminfo.c $(H)
	$(CC) -c meminfo.c
	mv meminfo.o $(OBJDIR)

$(OBJDIR)/monitor.o:	monitor.c $(H)
	$(CC) -c monitor.c
	mv monitor.o $(OBJDIR)

$(OBJDIR)/netstat.o:	netstat.c $(H)
	$(CC) -c netstat.c
	mv netstat.o $(OBJDIR)

$(OBJDIR)/proc.o:	proc.c $(H)
	$(CC) -c proc.c
	mv proc.o $(OBJDIR)

$(OBJDIR)/screen.o:	../common/screen.c $(H)
	$(CC) -c ../common/screen.c
	mv screen.o $(OBJDIR)

$(OBJDIR)/streams.o:	streams.c $(H)
	$(CC) -I. -c streams.c
	mv streams.o $(OBJDIR)

$(OBJDIR)/softirqs.o:	../linux/softirqs.c $(H)
	$(CC) -c ../linux/softirqs.c
	mv softirqs.o $(OBJDIR)

$(OBJDIR)/subs.o:	../common/subs.c $(H)
	$(CC) -c ../common/subs.c
	mv subs.o $(OBJDIR)

$(OBJDIR)/vmstat.o:	../linux/vmstat.c $(H)
	$(CC) -c ../linux/vmstat.c
	mv vmstat.o $(OBJDIR)

clean:
	-rm -f sigdesc.h bin.*/*
newf:
	tar cvf - `find . -type f -newer TIMESTAMP ! -name tags ! -name y.tab.c ! -name config.def | grep -v /bin ` | \
		gzip -9 > $(HOME)/tmp/src.proc-`date +%Y%m%d`.tar.gz

release:
#	strip bin/proc bin/monlist
	elfrewrite bin/proc 
	elfrewrite bin/monlist
	build=b`grep build_no include/build.h | sed -e 's/^.* \([0-9][0-9]*\).*$$/\1/'` ; \
	cd .. ; mv proc proc-$$build ; \
	tar cvf - proc-$$build/bin/proc \
		proc-$$build/bin/procmon \
		proc-$$build/bin/monlist \
		proc-$$build/README* \
		proc-$$build/Changes proc-$$build/proc.pl | gzip -9 > /tmp/proc-$$build.tar.gz ; \
	mv proc-$$build proc ; \
	scp /tmp/proc-$$build.tar.gz minny:release/website/tools ; \
	ssh minny rm -f release/website/tools/proc-current.tar.gz ; \
	ssh minny ln -s proc-$$build.tar.gz release/website/tools/proc-current.tar.gz

