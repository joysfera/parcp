DEBUG =
#DEBUG = -DDEBUG
#DEBUG = -DDEBUG -DLOWDEBUG
#DEBUG = -DDEBUG -DLOWDEBUG -DIODEBUG

FLAGS = $(EXT_FLAGS) -O2 -DATARI $(DEBUG) -Wall -fomit-frame-pointer -fno-strength-reduce
FLAGS030 = -m68020-40 $(FLAGS)

PARCLIENT =  parcp.c parcp.h cfgopts.h parcpkey.h parcplow.h parcplow.c crc32.c parcommo.c parftpcl.c sort.c

ZAKL_OBJS = match.o cfgopts.o parcpkey.o parcp68k.o global.o
ZAKL_OBJS030 = match030.o cfgopts030.o parcpkey030.o parcp68k.o global.o

OBJS = parcp.o $(ZAKL_OBJS)
OBJS030 = parcp030.o $(ZAKL_OBJS030)
SEROBJS = server_parcp.o $(ZAKL_OBJS)
SEROBJS030 = server_parcp030.o $(ZAKL_OBJS030)

SHELL_FLAGS = -DSHELL $(FLAGS)
SHELL_FLAGS030 = -DSHELL $(FLAGS030)
SHELL_OBJS = shell_parcp.o shell.o box.o menu.o viewer.o $(ZAKL_OBJS)
SHELL_OBJS030 = shell_parcp030.o shell030.o box030.o menu030.o viewer030.o $(ZAKL_OBJS030)

parshell : $(SHELL_OBJS)
	gcc $(SHELL_FLAGS) -o parcp $(SHELL_OBJS) -lpanel -lncurses
	strip parcp
	fixstk 64k parcp

parshell030 : $(SHELL_OBJS030)
	gcc $(SHELL_FLAGS030) -o parcp030 $(SHELL_OBJS030) -lpanel -lncurses -L/usr/lib/m68020
	strip parcp030
	fixstk 64k parcp030

parcp : $(OBJS)
	gcc $(FLAGS) -o parcp $(OBJS)
	strip parcp
	fixstk 64k parcp

parcp030 : $(OBJS030)
	gcc $(FLAGS030) -o parcp030 $(OBJS030) -L/usr/lib/m68020
	strip parcp030
	fixstk 64k parcp030

parserve : $(SEROBJS)
	gcc $(FLAGS) -o parserve $(SEROBJS)
	strip parserve
	fixstk 32k parserve

parserve030 : $(SEROBJS030)
	gcc $(FLAGS030) -o parser30 $(SEROBJS030) -L/usr/lib/m68020
	strip parser30
	fixstk 32k parser30

par_io : par_io.c
	gcc $(FLAGS) par_io.c -o par_io
	strip par_io

par_in : parinout.c parcp.h
	gcc $(FLAGS) -DIN parinout.c -o par_in
	strip par_in
	fixstk 16384 par_in

par_out : parinout.c parcp.h
	gcc $(FLAGS) -DOUT parinout.c -o par_out
	strip par_out
	fixstk 16384 par_out

parcpkey : parcpkey.c parcpkey.h
	gcc $(FLAGS) -DGENERATE parcpkey.c -o parcpkey
	strip parcpkey
	fixstk 16384 parcpkey

partest : partest.o $(ZAKL_OBJS)
	gcc $(FLAGS) -o partest partest.o $(ZAKL_OBJS)
	strip partest
	fixstk 64k partest

#all : parcp parcp030 par_in par_out
all : parshell parshell030 parserve parserve030 par_in

clean :
	rm -f *.o

realclean :
	rm -f parcp parcp030 parserve parser30 par_in par_out parcpkey par_io

###############################

parcp.o : $(PARCLIENT)
	gcc $(FLAGS) -c parcp.c

partest.o : partest.c $(PARCLIENT)
	gcc $(FLAGS) -c partest.c

shell_parcp.o : $(PARCLIENT)
	gcc $(SHELL_FLAGS) -c parcp.c -o shell_parcp.o

server_parcp.o : $(PARCLIENT)
	gcc -DPARCP_SERVER $(FLAGS) -c parcp.c -o server_parcp.o

shell.o : shell.c shell.h parcp.h cfgopts.h parcpkey.h
	gcc $(SHELL_FLAGS) -c shell.c

box.o : box.c box.h shell.h
	gcc $(SHELL_FLAGS) -c box.c

menu.o : menu.c menu.h shell.h
	gcc $(SHELL_FLAGS) -c menu.c

viewer.o : viewer.c
	gcc $(SHELL_FLAGS) -c viewer.c

global.o : global.c global.h
	gcc $(SHELL_FLAGS) -c global.c

parcp68k.o : parcp68k.asm
	xgen -L1 parcp68k.o parcp68k.asm
	gst2gcc gcc parcp68k.o

match.o : match.c match.h
	gcc $(FLAGS) -c match.c

cfgopts.o : cfgopts.c cfgopts.h
	gcc $(FLAGS) -c cfgopts.c

parcpkey.o : parcpkey.c parcpkey.h
	gcc $(FLAGS) -c parcpkey.c

parcp030.o : $(PARCLIENT)
	gcc $(FLAGS030) -c parcp.c -o parcp030.o
	
shell_parcp030.o : $(PARCLIENT)
	gcc $(SHELL_FLAGS030) -c parcp.c -o shell_parcp030.o

shell030.o : shell.c parcp.h cfgopts.h parcpkey.h menu.c viewer.c box.c
	gcc $(SHELL_FLAGS030) -c shell.c -o shell030.o

box030.o : box.c box.h
	gcc $(SHELL_FLAGS030) -c box.c -o box030.o

menu030.o : menu.c menu.h
	gcc $(SHELL_FLAGS030) -c menu.c -o menu030.o

viewer030.o : viewer.c viewer.h
	gcc $(SHELL_FLAGS030) -c viewer.c -o viewer030.o

server_parcp030.o : $(PARCLIENT)
	gcc -DPARCP_SERVER $(FLAGS030) -c parcp.c -o server_parcp030.o

match030.o : match.c match.h
	gcc $(FLAGS030) -c match.c -o match030.o

cfgopts030.o : cfgopts.c cfgopts.h
	gcc $(FLAGS030) -c cfgopts.c -o cfgopts030.o

parcpkey030.o : parcpkey.c
	gcc $(FLAGS030) -c parcpkey.c -o parcpkey030.o
