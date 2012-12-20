#include <stdio.h>
#include <stdlib.h>

#include "infinite-taunt.h"

static char *You[] = {
	"you",
	"your pilot",
	"your technology",
	"your mother",
	"your father",
	"your engineer",
	"your weapons officer",
	"your scientist",
	"you humans",
	"your sister",
	"your captain",
	"your navigator",
	"your spaceship",
	"your gamete donors",
	"your species",
	"your culture", 
	"your grandmother", 
	"your grandfather", 
	"your wife", 
	"the mother of your hatchlings", 
	"your mama", 
	"your mama", 
	"your garbage scow",
	"your brood of hatchlings",
	"you meatbags",
	"you meat robots",
	"you organic droids",
	"your hatchlings",
	"your spacefaring scrap heap",
	"your spacegoing garbage heap",
	"your spacefaring contraption",
};

static char *Are[] = {
	"are",
	"is",
	"be",
	"are being",
	"was",
	"are one of those",
};

static char *Action[] = {
	"drive",
	"pilot",
	"shoot",
	"fire torpedoes",
	"fire phasers",
	"weild phasers",
	"aim",
	"smell",
	"fart",
	"fail",
	"will fail",
	"will not succeed",
	"embarasses me",
	"will cry",
	"taste",
	"will beg for mercy",
	"cry",
	"run away",
	"does not succeed",
	"fails to succeed",
	"performs unacceptably",
	"performs inadequately",
	"fails to perform acceptably",
	"fails to perform adequately",
	"severely underperforms and",
	"achieve negative success",
	"reailizes failure",
	"exist",
	"communicates",
	"walks",
	"talks",
	"eats",
	"drinks",
	"thinks",
	"cogitates",
	"snorks",
	"swoggs",
	"kniddles",
	"kones",
	"drools",
	"twitches",
	"will burn",
	"will be destroyed",
	"will be blasted",
	"will be annihilated",
	"will be terminated",
	"will be enzarked",
	"will be engorfed",
	"are dishonest",
	"tell lies",
	"make me sick",
	"disgusts",
	"offends",
	"sucks",
	"emits fumes",
	"leaks fluids",
	"looks",
};

static char *Like[] = {
	"like",
	"similar to",
	"in the manner of",
	"in a way that reminds me of",
	"in a way that recalls",
	"as though emulating",
	"resembling",
	"just like",	
	"exactly like",
	"as",
	"as if he is",
	"as if she is",
	"comparable to",
	"worse than",
	"even worse than",
	"almost as bad as",
	"as badly as",
	"in a manner that exceeds in poor quality even",
	"like it is always as bad as",
	"is as terrible as",
	"is as terrible as",
	"is as incompetent as",
	"is as unschooled as",
	"is as inproficient as",
	"as awful as",
	"as horrible as",
	"as torkulant as",
	"as gonky as",
	"as durtinous as",
	"more rotten than",
	"more disgusting than",
};
	

static char *Adjective[] = {
	"drunken",
	"wasted",
	"emboozled",
	"addled",
	"crazy",
	"insane",
	"froomy",
	"borgulent",
	"flatulent",
	"gas-filled",
	"odorous",
	"noxious",
	"fuming",
	"smelly",
	"farty",
	"over inflated",
	"reeking",
	"stinky",
	"sweaty",
	"greasy",
	"antique",
	"ancient",
	"bipedal",
	"pathetic",
	"pitiful",
	"disgusting",
	"repellant",
	"orgnacious",
	"redognant",
	"unintelligent",
	"stupid",
	"idiotic",
	"scorched",
	"orbular",
	"spherical",
	"toothless",
	"mostly harmless",
	"de-clawed",
	"scrambled",
	"wounded",
	"halfwit",
	"brain damaged",
	"half brained",
	"intoxicated",
	"sedated",
	"sleepy",
	"useless",
	"broken",
	"out of order",
	"borked",
	"fractured",
	"cromulent",
	"blasted",
	"earth-schooled",
	"ignorant",
	"backwater",
	"uneducated",
	"earthman",
	"loaded",
	"fried",
	"high",
	"stoned",
	"sleeping",
	"half-conscious",
	"robotic",
	"droidly",
	"malfunctioning",
	"stunned",
	"beheaded",
	"lobotomized",
	"angry",
	"furious",
	"enraged",
	"frightened",
	"scared",
	"timid",
	"terrified",
	"startled",
	"rotten",
	"crying",
	"decayed",
	"old",
	"slime covered",
	"slime encrusted",
	"slimy",
	"bug infested",
	};

static char *Nationality[] = {
	"Vogon",
	"Nowwhattian",
	"Huttian",
	"Frubian",
	"Wallunian",
	"Hybonian",
	"Purkrani",
	"Furtian",
	"Barkonian",
	"Gordouni",
	"Albonian",
	"Neruvian",
	"Skonkulan",
	"Byntian",
	"Zurtian",
	"Tralfamadorian",
	"Traalian",
	"Earthling",
	"Arcturan",
	"Deneban",
	"Cepheid",
	"Kastran",
	"Nekkari",
	"Okulan",
	"Okulan",
	"Vegan",
	"Vegan",
	"Zosmani",
	"Rigelian",
	"Solian",
	"Fractulan",
	"Porgacine",
	"Nurtian",
	"Cappellan",
};

static char *Beast[] = {
	"lizard",
	"snake",
	"eel",
	"boghog",
	"horkbeast",
	"jebbok",
	"gervuza",
	"beast",
	"cow",
	"toad",
	"mushroom",
	"pigdog",
	"dog",
	"pig",
	"zorb",
	"slug",
	"turtle",
	"tortoise",
	"mollusc",
	"tuber",
	"vegetable",
	"turnip",
	"tentacle beast",
	"toilet",
	"waste disposal unit",
	"garbage heap",
	"waste digester",
	"waste organism",
	"sewage eater",
	"computer",
	"iceborer",
	"pustule",
	"witch doctor",
	"fortune teller",
	"space priest",
	"space pirate",
	"space hobo",
	"space vagrant",
	"space bum",
	"space butt",
	"space slug",
	"space beetle",
	"beetle",
	"snaprat",
	"vornalox",
	"porridge",
	"cabbage",
	"egg capsule",
	"teabags",
	"muskbats",
	"machinery",
	"deceleraptor",
	"space wanker",
	"space punk",
	"space ruffian",
	"melonhead",
	"rock dweller",
	"asteroid miner",
	"outer planet dweller",
	"garbage dweller",
	"buttless normal"
};

static char *Bad_thing[] = {
	"moron",
	"butthead",
	"buttface",
	"butt crack",
	"idiot",
	"fool",
	"joke",
	"space garbage",
	"rocks for brains",
	"Borgon",
	"simpleton",	
	"outer rim planet dweller",
	"Terran",
	"Earthling",
	"crazy Kroznor",
	"dogulent dog",
	"horkbeast",
	"wormatozoa",
	"skuggulorp",
	"forpulous driggotron",
	"space refuse",
	"ejected garbage capsule",
	"dung beast",
	"source of stink",
	"stinkbeast larvae",
	"vornalox eggs",
	"olfactory offender",
	"nobbett seed",
	"horkbeast seed",
	"artist",
};

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))

static char *random_word(char *w[], int nwords)
{
	int x;

	x = (int) (((double) rand() / (double) (RAND_MAX)) * nwords);
	return w[x];
}

static char *you(void)
{
	return random_word(You, ARRAYSIZE(You));
}

static char *action(void)
{
	return random_word(Action, ARRAYSIZE(Action));
}

static char *like()
{
	return random_word(Like, ARRAYSIZE(Like));
}

static char *adjective()
{
	return random_word(Adjective, ARRAYSIZE(Adjective));
}

static char *beast()
{
	return random_word(Beast, ARRAYSIZE(Beast));
}

static char *nationality()
{
	return random_word(Nationality, ARRAYSIZE(Nationality));
}

static char *are()
{
	return random_word(Are, ARRAYSIZE(Are));
}

static char *bad_thing()
{
	return random_word(Bad_thing, ARRAYSIZE(Bad_thing));
}

void infinite_taunt0(char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s %s",
			you(), action(), like(), adjective(), beast());
}

void infinite_taunt1(char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s %s %s",
			you(), action(), like(), adjective(), nationality(), beast());
}

void infinite_taunt2(char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s",
			you(), are(), adjective(), bad_thing());
}

void infinite_taunt3(char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s %s",
			you(), are(), adjective(), nationality(), bad_thing());
}

void infinite_taunt(char *buffer, int buflen)
{
	switch(rand() % 4) {
	case 0:
		infinite_taunt0(buffer, buflen);
		break;
	case 1:
		infinite_taunt1(buffer, buflen);
		break;
	case 2:
		infinite_taunt2(buffer, buflen);
		break;
	case 3:
		infinite_taunt3(buffer, buflen);
		break;
	default:
		infinite_taunt1(buffer, buflen);
		break;
	}
}

#ifdef TEST_TAUNT
#include <sys/time.h>
static void set_random_seed(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	srand(tv.tv_usec);
}

int main(int argc, char *argv[])
{
	int i;
	char buffer[1000];

	set_random_seed();

	for (i = 0; i < 1000; i++) {
		infinite_taunt(buffer, sizeof(buffer) - 1);
		printf("%s\n", buffer);
	}
	return 0;
}
#endif
