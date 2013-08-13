#------------------
# CAPTURE make file
#------------------

all : pmcap.exe pmcapdll.dll

pmcap.exe : pmcap.obj pmcap.def pmcap.res pmcapdll.lib
     wlink sys os2 pm f pmcap l os2,pmcapdll
     rc pmcap.res

pmcap.obj : pmcap.c pmcap.h pmcapdll.h
     wcc -ms -2 -s pmcap.c

pmcap.res : pmcap.rc pmcap.dlg pmcap.h
     rc -r pmcap

pmcapdll.dll : pmcapdll.obj pmcapdll.def
     wlink sys os2 dll segment type data shared f pmcapdll l os2 exp INITDLL,CLOSEDLL

pmcapdll.obj : pmcapdll.c pmcapdll.h
     wcc -ml -2 -s -zu -zl pmcapdll.c

pmcapdll.lib : pmcapdll.def
     implib pmcapdll.lib pmcapdll.def
