ifeq ($(OS),Windows_NT)
	FILE=Makefile.mingw
else
	ifneq ($(DJGPP),)
		FILE=Makefile.dos
	else
	OS=$(shell uname)

	ifeq ($(OS), MiNT)
		FILE=Makefile.mint
	endif

	ifeq ($(OS), Linux)
		FILE=Makefile.linux
		endif
	endif
endif
ifeq ($(FILE), )
	$(error Your platform ${OS} is not supported at this time.)
endif

include $(FILE)
