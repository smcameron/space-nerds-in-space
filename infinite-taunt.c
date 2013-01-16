#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "infinite-taunt.h"

static char *nothing = "";

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
	"the father of your hatchlings", 
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

static char *Known_For[] = {
	"is known for",
	"is renowned for",
	"has a reputation for",
	"is celebrated for",
	"is lauded for",
	"is widely known for",
	"is fabled for",
	"is famous for",
	"is most famous for",
	"is famed for",
	"is infamous for",
	"is legendary for",
	"is highly regarded for",
	"enjoys a reputation for",
	"is responsible for",
	"is notable for",
	"is most notable for",
	"is noted for",
	"is recognized for",
	"is feared for",
	"is beloved for",
};

static char *Producing[] = {
	"supplying",
	"being a source of",
	"producing",
	"exporting",
	"manufacturing",
	"creating",
	"fabricating",
	"constructing",
	"making",
	"shipping",
	"transporting",
	"reselling",
	"selling",
};

static char *Exceptional[] = {
	"exceptional",
	"fearsome",
	"fine",
	"wonderful",
	"great",
	"exciting",
	"exhilarating",
	"awesome",
	"astounding",
	"astonishing",
	"top flight",
	"the best",
	"ridiculously good",
	"amazing",
	"fantastic",
	"improbable",
	"fanciful",
	"comical",
	"unlikely",
	"really great",
	"very good",
	"good",
	"superb",
	"excellent",
	"good quality",
	"exceptionally good",
	"high quality",
	"top notch",
	"top shelf",
	"exceedingly good",
	"frighteningly good",
	"really excellent",
	"top of the line",
	"first class",
	"outstanding",
	"best of class",
	"state of the art",
	"staggering",
	"stupendous",
	"stunning",
	"breathtaking",
	"scandalous",
	"magnificent",
	"flawless",
	"phenomenal",
	"remarkable",
	"marvelous",
	"transcendant",
	"complicated",
	"complex",
	"stellar",
};

static char *Avoid[] = {
	"avoid",
	"skip",
	"don't bother with",
	"resist",
	"shun",
	"stay away from",
	"steer clear of",
	"evade",
	"eschew",
	"abstain from",
	"bypass",
	"dodge",
};

static char *Terrible[] = {
	"awful",
	"incomprehensible",
	"mediocre",
	"sub par",
	"deficient",
	"defective",
	"damaged",
	"insipid",
	"suspect",
	"sketchy",
	"borderline",
	"terrible",
	"worthless",
	"wretched",
	"shameful",
	"bad",
	"poor",
	"poor quality",
	"regrettable",
	"less than stellar",
	"painful",
	"dreadful",
	"boring",
	"tepid",
	"banal",
	"disgusting",
	"icky",
	"loathesome",
	"tedious",
	"pretentious",
	"revolting",
	"terrifying",
	"poisonous",
	"noxious",
	"horrifying",
	"egregious",
	"lame",
	"scandalous",
	"embarassing",
	"disenchanting",
	"underwhelming",
	"crappy",
	"shoddy",
	"disappointing",
};

static char *Product[] = {
	"weapons",
	"hand weapons",
	"textiles",
	"recreational drugs of various types",
	"dilithium crystals",
	"iron",
	"tax preparation software",
	"encryption algorithms",
	"sporting goods",
	"cylindric diamacron",
	"styrofoam cups",
	"coal",
	"corks",
	"positronics",
	"pancakes",
	"robots",
	"curries",
	"biscuits",
	"tuning forks",
	"glass bottles",
	"razor blades",
	"whisky",
	"ale",
	"philosophical theories",
	"mathematical formulae",
	"works of fiction",
	"fireworks and pyrotechnics",
	"ink",
	"beadwork",
	"flatware",
	"rocks",
	"dirt",
	"succulent plants",
	"songbirds",
	"boots",
	"spear points",
	"joot",
	"rocket boosters",
	"thrusters",
	"navigation computers",
	"dubious financial instruments",
	"tax shelters",
	"tasty sauces",
	"complex legal documents",
	"intoxicating beverages",
	"wood carvings",
	"engravings",
	"coffee mugs",
	"telephones",
	"carburetors",
	"kettles",
	"corn fritters",
	"conspiracy theories",
	"axioms",
	"random numbers",
	"carbonated sodas",
	"sandwiches",
	"cured meats and cheeses",
	"wood burning computers",
	"bizarre economic theories",
	"airtight religions",
	"lack of humor",
	"spices",
	"onions",
	"towels",
	"zarkons",
	"pajamas",
	"language lessons",
	"cybernetic interpreters",
	"poetry lessons",
	"musical instruction",
	"backrubs",
	"manifestos",
	"religious pamphlets",
	"dubious metaphors",
	"double entendres",
	"jokes",
	"recipes",
	"furniture",
};

static char *Culture[] = {
	"humorous anecdotes",
	"suggestive stories",
	"beverages of unusual size",
	"scalding hot teas",
	"unusual theories of ethics",
	"fractal music",
	"folk tunes",
	"epic poems",
	"humorous doodles", 
	"poetry",
	"choreography",
	"horticulture",
	"art",
	"music",
	"heavy metal",
	"extravagant hairstyles",
	"flamboyant footwear",
	"feasts",
	"high performance gustation",
	"racing ships",
	"ceremonies",
	"parties",
	"festivals",
	"carnivals",
	"fairs",
	"shindigs",
	"concerts",
	"television dramas",
	"horoscopes",
	"predictions",
	"prognostications",
	"nostrums",
	"beverages",
	"delectables",
	"paintings",
	"tapestries",
	"sculptures",
	"wines",
	"racy novellas",
	"computer programming languages",
	"nightclubs",
	"night life",
	"soups",
	"spiritual hogwash",
	"solid engineering",
	"fine workmanship",
};

static char *Traveling_Accessory[] = {
	"pair of sunglasses",
	"bottle of sun screen",
	"sense of humor",
	"patience",
	"stash of bribe money",
	"towel",
	"container of bug spray",
	"bathing suit",
	"pair of hiking shoes",
	"camera",
	"good blaster",
	"phaser rifle",
	"space suit",
	"supply of food",
	"supply of water",
	"vial of salt tablets",
	"vial of antibiotics",
	"big pile of money",
	"big wads of cash",
	"cybernetic translator",
	"set of earplugs",
	"zoort net",
	"water purifier",
	"dispenser of scag repellent",
	"space marine escort", 
	"cold weather gear",
	"bullshit detector",
	"strong stomach",
	"load of cash",
	"strong line of credit",
	"complete set of vaccination documentation",
	"letter of recommendation",
	"good suit of clothes",
	"flashlight",
	"sense of smell",
	"set of noseplugs",
	"rational state of mind",
	"skeptical bent of mind",
	"way with words",
	"silver tongue",
	"way with a blaster",
	"set of fishing tackle",
	"vial of anti-venom",
	"mind altering substances",
};

static char *Bring_Your[] = {
	"Be sure to bring your",
	"Don't forget your",
	"Don't leave home without your",
	"Bring a",
	"You'll be sorry if you fail to bring your",
	"Travel with a",
	"Don't even think of coming here without your",
	"Consider it mandatory to bring your",
	"A fate worse than death awaits those forget to bring their",
	"Do not bother to bring your",
	"It is forbidden to bring a",
	"Be warned to bring a",
	"You will definitely need a",
	"You can't get by without a",
	"You will not enjoy yourself without your",
	"It is not permitted to bring your",
	"Bring your",
	"Visitors are advised to bring a",
	"Travelers are advised to bring their",
	"It is inadvisable to bring your",
	"The wise traveler will bring a",
	"Word to the wise, bring a",
	"The seasoned traveler knows to bring a",
	"Be advised, bring your",
	"Well worthwhile to bring your",
	"Absolutely do bring your",
	"Under no circumstances forget to bring your",
};

static char *Planet[] = {
	"planet",
	"system",
	"planet",
	"world",
	"planetoid",
	"place",
	"locale",
	"region",
};

static char *Climate[] = {
	"tropical",
	"icy",
	"desert",
	"aqueous",
	"gaseous",
	"mountainous",
	"featureless",
	"stormy",
	"pleasant",
	"warm",
	"cold",
	"frozen",
	"hot",
	"dry",
	"snowy",
	"rainy",
	"temperate",
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

static char *qnationality()
{
	int x;
	x = (int) (((double) rand() / (double) (RAND_MAX)) * 1000);
	if (x < 100)
		return nationality();
	else
		return nothing;
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

static char *known_for(void)
{
	return random_word(Known_For, ARRAYSIZE(Known_For));
}

static char *exceptional(void)
{
	return random_word(Exceptional, ARRAYSIZE(Exceptional));
}

static char *terrible(void)
{
	return random_word(Terrible, ARRAYSIZE(Terrible));
}

static char *avoid(void)
{
	return random_word(Avoid, ARRAYSIZE(Avoid));
}

static char *producing(void)
{
	return random_word(Producing, ARRAYSIZE(Producing));
}

static char *product(void)
{
	return random_word(Product, ARRAYSIZE(Product));
}

static char *culture(void)
{
	return random_word(Culture, ARRAYSIZE(Culture));
}

static char *planet(void)
{
	return random_word(Planet, ARRAYSIZE(Planet));
}

static char *climate(void)
{
	return random_word(Climate, ARRAYSIZE(Climate));
}

static char *bring_your(void)
{
	return random_word(Bring_Your, ARRAYSIZE(Bring_Your));
}

static char *traveling_accessory(void)
{
	return random_word(Traveling_Accessory,
				ARRAYSIZE(Traveling_Accessory));
}

void planet_description(char *buffer, int buflen)
{
	char do_avoid[100];

	strcpy(do_avoid, avoid());
	do_avoid[0] = toupper(do_avoid[0]);

	snprintf(buffer, buflen, "This %s %s %s %s %s %s %s and %s %s %s %s. %s the %s %s.  %s %s.\n",
		climate(), planet(), known_for(), producing(),
			exceptional(), qnationality(), product(),
			known_for(), exceptional(), qnationality(),
			culture(),
			do_avoid, terrible(), product(),
			bring_your(), traveling_accessory());
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
		//infinite_taunt(buffer, sizeof(buffer) - 1);
		planet_description(buffer, sizeof(buffer) - 1);
		printf("%s\n", buffer);
	}
	return 0;
}
#endif
