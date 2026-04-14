PROG = 7zPasswordPlugins.dll
DEF_FILE = PasswordPlugin.def
NO_DEFAULT_RES = 1

CURRENT_OBJS = \
  $O\PasswordPluginExports.obj \
  $O\PasswordPluginCore.obj \

COMMON_OBJS = \
  $O\IntToString.obj \
  $O\MyString.obj \
  $O\MyVector.obj \
  $O\NewHandler.obj \
  $O\StringConvert.obj \
  $O\StringToInt.obj \
  $O\UTFConvert.obj \

WIN_OBJS = \
  $O\ErrorMsg.obj \
  $O\FileDir.obj \
  $O\FileFind.obj \
  $O\FileIO.obj \
  $O\FileName.obj \
  $O\Registry.obj \
  $O\TimeUtils.obj \

C_OBJS = \
  $O\Md5.obj \
  $O\sqlite3.obj \

!include "../../7zip.mak"

$O\resource.res: resource.rc
	rc $(RFLAGS) -fo$@ $**

$O\PasswordPluginExports.obj: .\PasswordPluginExports.cpp
	$(CC) $(CFLAGS_O1) -Yu"StdAfx.h" -Fp$O/a.pch $**

$O\PasswordPluginCore.obj: .\PasswordPluginCore.cpp
	$(CC) $(CFLAGS_O1) -wd5264 -wd4505 -Yu"StdAfx.h" -Fp$O/a.pch $**

$O\sqlite3.obj: ../../../../C/sqlite3.c
	$(CC) $(CFLAGS_COMMON) -Gr -nologo -c -Fo$O/ -MT -O2 -W0 -wd5045 -wd4711 -DSQLITE_THREADSAFE=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_TEMP_STORE=2 $**
