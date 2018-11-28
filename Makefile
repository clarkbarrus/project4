CC = gcc
EXECUTABLES = zinspect zformat zmkdir zrmdir zfilez ztouch zcreate zappend
FLAGS = -Wall

all:$(EXECUTABLES)

ztouch: vdisk.o ztouch.o oufs_lib_support_files.o
	$(CC) $(FLAGS) vdisk.o ztouch.o oufs_lib_support_files.o oufs_lib_support.o -o ztouch

zcreate: vdisk.o zcreate.o oufs_lib_support_files.o
	$(CC) $(FLAGS) vdisk.o zcreate.o oufs_lib_support_files.o oufs_lib_support.o -o zcreate

zappend: vdisk.o zappend.o oufs_lib_support_files.o
	$(CC) $(FLAGS) vdisk.o zappend.o oufs_lib_support_files.o oufs_lib_support.o -o zappend

zmore: vdisk.o zmore.o oufs_lib_support_files.o
	$(CC) $(FLAGS) vdisk.o zmore.o oufs_lib_support_files.o oufs_lib_support.o -o zmore

zinspect: vdisk.o zinspect.o oufs_lib_support.o
	$(CC) $(FLAGS) vdisk.o zinspect.o oufs_lib_support.o -o zinspect

zmkdir: vdisk.o zmkdir.o oufs_lib_support.o
	$(CC) $(FLAGS) vdisk.o zmkdir.o oufs_lib_support.o -o zmkdir

zrmdir: vdisk.o zrmdir.o oufs_lib_support.o
	$(CC) $(FLAGS) vdisk.o zrmdir.o oufs_lib_support.o -o zrmdir

zformat: zformat.o vdisk.o oufs_lib_support.o
	$(CC) $(FLAGS) zformat.o vdisk.o oufs_lib_support.o -o zformat

zfilez: zfilez.o vdisk.o oufs_lib_support.o
	$(CC) $(FLAGS) zfilez.o vdisk.o oufs_lib_support.o -o zfilez

ztouch.o: ztouch.c
	$(CC) $(FLAGS) -c ztouch.c -o ztouch.o

zcreate.o: zcreate.c
	$(CC) $(FLAGS) -c zcreate.c -o zcreate.o

zappend.o: zappend.c
	$(CC) $(FLAGS) -c zappend.c -o zappend.o

zmore.o: zmore.c
	$(CC) $(FLAGS) -c zmore.c -o zmore.o

zinspect.o: zinspect.c
	$(CC) $(FLAGS) -c zinspect.c -o zinspect.o

zfilez.o: zfilez.c
	$(CC) $(FLAGS) -c zfilez.c -o zfilez.o

zformat.o: zformat.c
	$(CC) $(FLAGS) -c zformat.c -o zformat.o

zmkdir.o: zmkdir.c
	$(CC) $(FLAGS) -c zmkdir.c -o zmkdir.o

zrmdir.o: zrmdir.c
	$(CC) $(FLAGS) -c zrmdir.c -o zrmdir.o

oufs_lib_support.o: oufs_lib_support.c
	$(CC) $(FLAGS) -c oufs_lib_support.c -o oufs_lib_support.o

oufs_lib_support_files.0: oufs_lib_support_files.c
	$(CC) $(FLAGS) -c oufs_lib_support_files.c -o oufs_lib_support.o

vdisk.o:  vdisk.c
	$(CC) $(FLAGS) -c vdisk.c -o vdisk.o


clean:
	rm $(EXECUTABLES) *.o
