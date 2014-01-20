#ifndef container_of

#define container_of(ptr, type, member) __extension__ ( { \
		const typeof( ((type *)0)->member ) *__mptr = (ptr); \
		(type *)( (char *)__mptr - offsetof(type,member) ); } )

#endif
