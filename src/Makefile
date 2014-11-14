ifeq ($(OS),Windows_NT)
	FILE=Makefile.mingw
else
	OS=$(shell uname)

	ifeq ($(OS), Darwin)
		FILE=Makefile.mac
	endif

	ifeq ($(OS), Linux)
		FILE=Makefile.linux
	endif
endif
ifeq ($(FILE), )
	$(error Your platform ${OS} is not supported at this time.)
endif

include $(FILE)
