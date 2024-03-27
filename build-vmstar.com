$ cc utime.c
$ cc vmstar.c
$ link /exec=vmstar vmstar.obj,utime.obj,sys$library:vaxcrtl.olb/lib
