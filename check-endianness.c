#include <stdio.h>

int main(int argc, char *argv[])
{
	int x = 0x1234abcd;
	unsigned char *c = (unsigned char *) &x;
	char *bitness, *endianness;

	/* Eh, this will work on linux but will break on windows. */
	if (sizeof(long) == 4)
		bitness = "32-bit";
	else if (sizeof(long) == 8)
		bitness = "64-bit";
	else
		bitness = "unknown wordsize";
	if (*c == 0x12)
		endianness = "big-endian";
	else
		endianness = "little-endian";

	printf("%s %s\n", bitness, endianness);
	return 0;
}

