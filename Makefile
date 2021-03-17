malloc.so: memory.c memory.h utility.c utility.h
	gcc -Wall -Werror -shared -fPIC -g -o malloc.so utility.c memory.h
