#include <stdio.h>
#include <sys/time.h>

#include "quat.h"
#define SNIS_SERVER_DATA 1
#include "snis.h"
#include "key_value_parser.h"

#include "snis_entity_key_value_specification.h"

int main(int argc, char *argv[])
{
	key_value_specification_print(snis_entity_kvs);
}
