SYS := $(shell $(CC) -dumpmachine)
ifneq (, $(findstring linux, $(SYS)))
	EXE_EXT:=
else ifneq (, $(findstring mingw, $(SYS)))
	EXE_EXT:=.exe
endif

qubide$(EXE_EXT) : qubide.c qubide.h
	$(CC) -Wall -o qubide$(EXE_EXT) qubide.c

