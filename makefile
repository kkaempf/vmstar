# Makefile for Sid Penstone's VMS tar reader program
# [18-Oct-87]
O = .obj
X = .exe

vmstar:	vmstar$(X)

vmstar$(X):	vmstar$(O) utime$(O)
	link vmstar$(O),utime$(O),sys$$library:vaxcrtl/lib
