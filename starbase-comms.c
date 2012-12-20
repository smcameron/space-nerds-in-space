#include "starbase-comms.h"
#include "stdlib.h"

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

static char *under_attack[] = {
	"WE ARE UNDER ATTACK",
	"UNDER ATTACK",
	"WE ARE BEING ATTACKED",
	"WE ARE TAKING DAMAGE FROM ENEMY WEAPONS",
	"TAKING DAMAGE FROM ENEMY AGGRESSION",
	"ENEMY SHIPS ARE ATTACKING US",
	"TAKING PHASER FIRE",
	"TAKING DAMAGE FROM TORPEDOES",
};

static char *random_string(char *array[], int arraysize)
{
	return array[(rand() % arraysize)];
}	

char *starbase_comm_under_attack(void)
{
	return random_string(under_attack, ARRAYSIZE(under_attack));
}

