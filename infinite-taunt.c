#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>

#include "arraysize.h"
#include "mtwist.h"
#include "infinite-taunt.h"
#include "names.h"
#include "string-utils.h"

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
	"Schaazbaut",
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

static char *Beautiful[] = {
	"beautiful",
	"gorgeous",
	"very pretty",
	"amazing looking",
	"stunning",
	"striking",
};

static char *Gas_Giant[] = {
	"gas giant",
	"gaseous world",
	"gas planet",
	"world",
	"giant",
	"huge world",
	"giant planet",
};

static char *Ice_Giant[] = {
	"ice giant",
	"icy world",
	"ice planet",
	"world",
	"huge world",
	"giant planet",
};

static char *Rocky_Planet[] = {
	"rocky planet",
	"rocky world",
	"mountainous planet",
	"cratered world",
	"cratered planet",
	"pockmarked world",
	"rock",
	"sphere",
	"planet",
	"planet",
	"planet",
	"world",
};

static char *Barren[] = {
	"barren",
	"lifeless",
	"sterile",
	"desert",
	"bleak",
	"dead",
	"forlorn",
	"godforsaken",
	"empty",
	"infertile",
	"parched",
	"arid",
	"desolate",
	"empty",
	"bare",
	"cold",
	"fierce",
};

static char *Uninhabitable[] = {
	"uninhabitable",
	"not inhabitable",
	"extremely unwelcoming",
	"severe and harsh",
	"depressingly vacant of life",
	"harsh in the extreme, visited only by a few self-abasing monks of backwards religious sects",
	"optimally hostile, every aspect of the climate seems devised to prevent the emergence of even the hardiest of extremophiles.",
	"what might be called \"geologically enthusiastic\", as there's no stable place to stand while not enjoying the climate.",
};

static char *However[] = {
	"however",
	"but",
	"although",
	"yet nevertheless",
	"but even so",
	"yet nonetheless",
	"but in spite of that",
	"yet still",
	"but just the same",
	"yet all the same",
	"but despite this",
};

static char *Planet[] = {
	"planet",
	"system",
	"planet",
	"world",
	"place",
	"locale",
	"region",
};

static char *GasGiantClimate[] = {
	"vaporous",
	"gaseous",
	"cloudy",
	"heavy",
	"huge",
	"monstrous",
	"gigantic",
	"enormous",
	"ginormous",
	"gigantic",
	"gargantuan",
	"beautiful",
	"striking",
	"gorgeous",
	"wondrous",
	"amazing",
	"hydrogenous",
};

static char *IcyClimate[] = {
	"cold",
	"gigantic",
	"beautiful",
	"striking",
	"gorgeous",
	"ammonia laden",
	"watery",
};

static char *RockyClimate[] = {
	"dessicated",
	"dry",
	"hot",
	"cold",
	"cratered",
	"rocky",
	"mountainous",
	"rugged",
	"dry",
	"exposed",
};

static char *Climate[] = {
	"tropical",
	"pleasant",
	"not entirely unpleasant",
	"somewhat cold",
	"mostly desert",
	"watery",
	"mountainous",
	"stormy",
	"warm",
	"cold",
	"frozen",
	"hot",
	"snowy",
	"rainy",
	"temperate",
	"humid",
};
	
static char *Be_advised[] = {
	"warning,",
	"be advised,",
	"let it be known,",
	"you are advised to",
	"attention,",
	"this will be your only warning,",
	"you are warned to",
	"Word to the wise,",
	"you are strongly cautioned to",
	"Be warned,",
};

static char *Cease_fire[] = {
	"cease fire at once",
	"cease fire",
	"cease hostilities",
	"cool your lasers",
	"suspend fire",
	"desist hostilities",
	"chill your lasers",
	"suspend hostilities at once",
	"belay fire",
	"cease firing",
};

static char *You_will_be[] = {
	"you will be",
	"you'll be",
	"your ship will be",
	"you and your crew will be",
	"that garbage scow you call a ship will be",
	"your hull will be",
	"what's left of your ship will be",
	"your tin can of a ship will be",
	"your ship full of corpses will be",
	"your rattling hull will be",
};

static char *Get_lost[] = {
	"get lost",
	"get out of here",
	"leave immediately",
	"make yourself scarce",
	"make like a tree and get outta here",
	"clear the area",
	"remove yourself at once",
	"vacate this space immediately",
	"depart at once",
	"scram",
	"get outta here",
	"bug off",
	"back off",
	"retreat immediately",
	"leave the area at once",
	"withdraw at once",
};

static char *Destroyed[] = {
	"destroyed",
	"incinerated",
	"vaporized",
	"blown out of the sky",
	"blasted out of the sky",
	"blasted to embers",
	"charred to bits",
	"blown to smithereens",
	"fried to a crisp",
	"reduced to smoke",
	"burned to embers",
	"sent to meet Zarkon's ghost",
	"reduced to molten slag",
	"blasted to cinders",
	"a fun bit of target practice",
	"reduced to slag and cinders",
	"blasted to Nebulon seven",
	"scorched to ashes",
	"smoked like a kipper",
	"regrettably incinerated",
	"unseasonably warm for a few microseconds",
	"lit up like a Zarkon wedding party",
	"having your absorption spectrum rather severely tested",
	"eating way too many photons for breakfast",
};

static char *Title[] = {
	"MR",
	"MS",
	"MRS",
	"DR.",
	"CAPT",
	"CORPORAL",
	"AMBASSADOR",
	"PVT",
	"GENERAL",
	"ASTRONOMER",
	"PRINCE",
	"PRINCESS",
	"DUKE",
	"PRESIDENT",
	"PHAROAH",
	"ADMIRAL",
	"CHANCELLOR",
	"COUNT",
	"MARQUIS",
	"BARON",
	"SULTAN",
	"HIGHMAN",
	"CMDR",
	"COLONEL",
	"JAGGER",
	"MORKMAN",
	"ASTRONAUT",
	"SKYMAN",
	"SURVEYOR",
	"REVEREND",
	"PRIESTESS",
	"PRIESTMUNTY",
	"HOJON",
	"KUKE",
	"ARBER",
	"ZEGO",
	"KANTER",
	"POLITIKKER",
	"YEZPER",
	"QUONON",
	"CAVANOR",
	"PREFECT",
	"WEZBON",
	"GOZON",
	"ZABRONI",
};

static char *PostNominalLetters[] = {
	", DDS",
	", PhD",
	", JD",
	", Esq",
	", Sc",
	", JJZ",
	", AQN",
	", SN",
	", MX",
};

static char *SpaceWorthy[] = {
	"Magnifico",
	"Magna",
	"Silencio",
	"Darksky",
	"Celestial",
	"Silent",
	"Sky",
	"Heavenly",
	"Cosmic",
	"Cosmo",
	"Starry",
	"Stardust",
	"Comet",
	"Quarkion",
	"Rapido",
	"Swift",
	"Nightsky",
	"Night",
	"Stellar",
	"Starbound",
	"Starro",
	"Starman",
	"Starfall",
	"Starlight",
	"Starway",
	"Starlink",
	"Starward",
	"Galactic",
	"Redshift",
	"Nebula",
	"Asterion",
	"Galaxie",
	"Stella",
	"Galassia",
	"Esteron",
	"Esploratore",
	"Lontana",
	"Distant",
	"Fargone",
	"Early",
	"Gravity",
	"Graviton",
	"Superior",
	"Farther",
	"Stellar",
	"Stellaron",
	"Andromeda",
	"Cassiopeia",
	"Scorpius",
	"Taurus",
	"Perseus",
	"Draco",
	"Capricorn",
	"Carina",
	"Cetus",
	"Auriga",
	"Libra",
	"Antares",
	"Velocita",
	"Veloce",
	"Potente",
	"Velox",
	"Porta",
	"Volare",
	"Ascendre",
	"Serene",
	"Boundary",
	"Skyway",
	"Planet",
	"Cielo",
	"High",
	"Alta",
	"Deep",
	"Alto",
	"Space",
	"Void",
	"Voidsky",
	"Voidal",
	"Voidness",
	"Blacksky",
	"Blackness",
	"Darkness",
	"Wayward",
	"Zisto",
	"Karnach",
	"Tempo",
	"Tiempo",
	"Luce Stellare",
	"Lumiere",
	"Golden",
	"Shining",
	"Jade",
	"Topaz",
	"Hopeful",
	"Wandering",
	"Centaurus",
};

static char *Animal[] = {
	"Wrasse",
	"Tarpon",
	"Dolphin",
	"Wasp",
	"Hornet",
	"Lamprey",
	"Marlin",
	"Falcon",
	"Firebird",
	"Hellcat",
	"Hellcat",
	"Beast",
	"Starling",
	"Swan",
	"Dove",
	"Sapphire",
	"Beryl",
	"Celestite",
	"Blossom",
	"Mayflower",
	"Orchid",
	"Bounty",
	"Harbinger",
	"Harvester",
	"Fortune",
	"Fortuna",
	"Yirkon",
	"Norder",
	"Cygnus",
	"Tiburon",
	"Shark",
	"Octopus",
	"Cepheid",
	"Ceph",
	"Cuttlefish",
	"Lightray",
	"Ray",
	"Manta",
	"Bear",
	"Jaguar",
	"Wumpus",
	"Groo",
	"Salamander",
	"Barracuda",
	"Serpent",
	"Scorpion",
	"Dog",
	"Lynx",
	"Sparrow",
	"Kingfisher",
	"Faucon",
	"Hawk",
	"Panther",
	"Mongoose",
	"Wolf",
	"Garnau",
	"Quassannor",
	"Wuffer",
	"Bulldog",
	"Leopard",
	"Ocelot",
	"Bearcat",
	"Spitfire",
	"Orca",
	"Demon",
	"Gryphon",
	"Phoenix",
	"Valkyrie",
	"Walrus",
	"Lion",
	"Javalina",
	"Boar",
	"Stallion",
	"Gazelle",
	"Dragon",
	"Raptor",
	"Canis",
	"Tarantula",
	"Medusa",
	"Kraken",
	"Cerberus",
	"Charon",
	"Gorgon",
	"Scylla",
	"Pegasus",
};

static char *ShipThing[] = {
	"Voidmaster",
	"Frontier",
	"Pioneer",
	"Avenger",
	"Valiant",
	"Drifter",
	"Horizon",
	"Oort",
	"Globe",
	"Planeteer",
	"Ville",
	"Vega",
	"Sirius",
	"Constellation",
	"Montagne",
	"Volcan",
	"Europa",
	"Erth",
	"Nollion",
	"Jupitor",
	"Zeus",
	"Pilote",
	"Navigateur",
	"Winger",
	"Wing",
	"Wings",
	"Voyageur",
	"Traveller",
	"Aventurier",
	"Ranger",
	"Surveyor",
	"Roi",
	"King",
	"Queen",
	"Dame",
	"Lady",
	"Charmant",
	"Master",
	"Lighter",
	"Nova",
	"Supreme",
	"Argent",
	"Jewel",
	"Gem",
	"Whisper",
	"Wind",
	"Vista",
	"Orion",
	"Hunter",
	"Huntress",
	"Kraggon",
	"Regoon",
	"Altierbon",
	"Zarmion",
	"Tunstor",
	"Zephyr",
	"Challenger",
	"Zerker",
	"Ellion",
	"Jirion",
	"Edison",
	"Nightsky",
	"Starcrosser",
	"Stargazer",
	"Stargrazer",
	"Skyfire",
	"Skycrosser",
	"Starion",
	"Stareyes",
	"Starlorn",
	"Starfire",
	"Starborne",
	"Starward",
	"Vortex",
	"Ventura",
	"Sword",
	"Blade",
	"Scimitar",
	"Dagger",
	"Cutlass",
	"Fairsky",
	"Skydome",
	"Flame",
	"Fire",
	"Energon",
	"Bonzaar",
	"Stormer",
	"Vixen",
	"Heart",
	"Arrow",
	"Moongrazer",
	"Moongazer",
	"Moonbounder",
	"Moonfire",
	"Ocean",
	"Archer",
	"Theorem",
	"Axiom",
	"Concept",
	"Fury",
	"Passion",
	"Revenge",
	"Redemption",
	"Panzer",
	"Sleekness",
	"Forza",
	"Physics",
	"Astro",
	"Orbit",
	"Velo",
	"Lumino",
	"Photono",
	"Warpo",
	"Blackbird",
	"Espirit",
	"Spirit",
	"Photon",
	"Proton",
	"Wavelength",
	"Blazer",
	"Waymaker",
	"Bullet",
	"Pulsar",
	"Quasar",
	"Saturn",
	"Dasher",
	"Runner",
	"Cutter",
	"Spectron",
	"Focus",
	"Aries",
	"Polaris",
	"Altair",
	"Betelgeuse",
	"Electra",
	"Kastra",
	"Lucida",
	"Naos",
	"Spica",
	"Zeta",
	"Omega",
	"Alpha",
	"Sigma",
	"Epsilon",
	"Beta",
	"Delta",
};

static char *CrimeModifier[] = {
	"accessory to",
	"conspiracy to commit",
	"aggravated",
	"attempted",
	"capital",
	"first degree",
	"second degree",
	"premeditated",
	"willful",
	"negligent",
	"armed",
	"unauthorized",
	"inappropriate",
	"+with intent to sell",
	"+with a deadly weapon",
	"+with a minor",
	"+with a slave",
	"+with a pineapple",
	"+without a pineapple",
	"+without consent",
	"+without humor",
	"+while intoxicated",
	"+under the influence of religion",
};


static char *Crime[] = {
	"regicide",
	"homicide",
	"zarkicide",
	"murder",
	"kidnapping",
	"robbery",
	"assault",
	"piracy",
	"sabotage",
	"espionage",
	"treason",
	"rustling of livestock",
	"embezzling",
	"counterfeiting",
	"smuggling",
	"assassination",
	"acts of piracy",
	"spacing",
	"murder by spacing",
	"asteroid marooning",
	"blasphemy",
	"deicide",
	"flatulence",
	"slaughter",
	"reckless endangerment",
	"destruction of property",
	"destruction of the environment",
	"bad puns",
	"possession of stolen property",
	"possession of controlled substances",
	"possession of controlled firearms",
	"parole violation",
	"escape from prison",
	"escape from slavery",
	"insulting of majesty",
	"contempt of court",
	"smoking",
	"trespassing",
	"reincarnation without prior consent of deity",
	"copyright infringement",
	"desecration",
	"failure to desecrate",
	"transporting of stolen slaves",
	"transporting of stolen property",
	"wrongful enslavement",
	"insulting of a deity",
	"failure to insult a deity",
	"invention of religion",
	"disproving of religion",
	"proving of religion",
	"propagation of faith",
	"suppression of faith",
	"insufficiency of faith",
	"excessive faith",
	"time traveling",
	"resisting arrest",
	"loitering",
	"littering",
	"vagrancy",
	"failure to pay debts",
	"failure to incur sufficient debt",
	"tax evasion",
	"money laundering",
	"gambling",
	"fraud",
	"burglary",
	"theft",
	"grand theft spacecraft",
	"participation in a tontine",
	"conjuring",
	"sorcery",
	"witchcraft",
};

static char *random_word(struct mtwist_state *mt, char *w[], int nwords)
{
	int x;

	x = (int) (((double) mtwist_next(mt) / (double) (0xffffffff)) * nwords);
	return w[x];
}

#define SELECT_WORD_FN(word, wordarray) \
static char *word(struct mtwist_state *mt) \
{ \
	return random_word(mt, wordarray, ARRAYSIZE(wordarray)); \
}

SELECT_WORD_FN(you, You)
SELECT_WORD_FN(action, Action)
SELECT_WORD_FN(like, Like)
SELECT_WORD_FN(adjective, Adjective)
SELECT_WORD_FN(beast, Beast)
SELECT_WORD_FN(nationality, Nationality)
SELECT_WORD_FN(are, Are)
SELECT_WORD_FN(bad_thing, Bad_thing)
SELECT_WORD_FN(known_for, Known_For)
SELECT_WORD_FN(exceptional, Exceptional)
SELECT_WORD_FN(terrible, Terrible)
SELECT_WORD_FN(avoid, Avoid)
SELECT_WORD_FN(producing, Producing)
SELECT_WORD_FN(product, Product)
SELECT_WORD_FN(culture, Culture)
SELECT_WORD_FN(planet, Planet)
SELECT_WORD_FN(bring_your, Bring_Your)
SELECT_WORD_FN(traveling_accessory, Traveling_Accessory)
SELECT_WORD_FN(be_advised, Be_advised)
SELECT_WORD_FN(cease_fire, Cease_fire)
SELECT_WORD_FN(get_lost, Get_lost)
SELECT_WORD_FN(you_will_be, You_will_be)
SELECT_WORD_FN(destroyed, Destroyed)
SELECT_WORD_FN(character_title, Title)
SELECT_WORD_FN(beautiful, Beautiful)
SELECT_WORD_FN(gas_giant, Gas_Giant)
SELECT_WORD_FN(ice_giant, Ice_Giant)
SELECT_WORD_FN(uninhabitable, Uninhabitable)
SELECT_WORD_FN(however, However)
SELECT_WORD_FN(rocky_planet, Rocky_Planet)
SELECT_WORD_FN(barren, Barren)

static char *qnationality(struct mtwist_state *mt)
{
	int x;
	x = (int) (((double) mtwist_next(mt) / (double) (0xffffffff)) * 1000);
	if (x < 100)
		return nationality(mt);
	else
		return nothing;
}

void infinite_taunt0(struct mtwist_state *mt, char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s %s",
			you(mt), action(mt), like(mt), adjective(mt), beast(mt));
}

void infinite_taunt1(struct mtwist_state *mt, char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s %s %s",
			you(mt), action(mt), like(mt), adjective(mt), nationality(mt), beast(mt));
}

void infinite_taunt2(struct mtwist_state *mt, char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s",
			you(mt), are(mt), adjective(mt), bad_thing(mt));
}

void infinite_taunt3(struct mtwist_state *mt, char *buffer, int buflen)
{
	snprintf(buffer, buflen, "%s %s %s %s %s",
			you(mt), are(mt), adjective(mt), nationality(mt), bad_thing(mt));
}

void infinite_taunt(struct mtwist_state *mt, char *buffer, int buflen)
{
	switch (mtwist_next(mt) % 4) {
	case 0:
		infinite_taunt0(mt, buffer, buflen);
		break;
	case 1:
		infinite_taunt1(mt, buffer, buflen);
		break;
	case 2:
		infinite_taunt2(mt, buffer, buflen);
		break;
	case 3:
		infinite_taunt3(mt, buffer, buflen);
		break;
	default:
		infinite_taunt1(mt, buffer, buflen);
		break;
	}
}

static char *climate(struct mtwist_state *mt, enum planet_type ptype)
{
	switch (ptype) {
	case planet_type_gas_giant:
		return random_word(mt, GasGiantClimate, ARRAYSIZE(GasGiantClimate));
	case planet_type_earthlike:
		return random_word(mt, Climate, ARRAYSIZE(Climate));
	case planet_type_rocky:
		return random_word(mt, RockyClimate, ARRAYSIZE(RockyClimate));
	case planet_type_ice_giant:
		return random_word(mt, IcyClimate, ARRAYSIZE(IcyClimate));
	default:
		break;
	}
	return random_word(mt, Climate, ARRAYSIZE(Climate));
}

static void break_lines(char *buffer, int line_len)
{
	int i, so_far, len;

	if (!line_len)
		return;
	len = strlen(buffer);
	so_far = 0;
	for (i = 0; i < len; i++) {
		if (buffer[i] == ' ' && so_far > line_len) {
			buffer[i] = '\n';
			so_far = -1;
		}
		so_far++;
	}
}

static char *post_nominal_letters(struct mtwist_state *mt)
{
	static char *nothing = "";
	if (mtwist_int(mt, 100) < 85)
		return nothing; 
	return random_word(mt, PostNominalLetters, ARRAYSIZE(PostNominalLetters));
}

void planet_description(struct mtwist_state *mt, char *buffer, int buflen,
			int line_len, enum planet_type ptype, int has_starbase)
{
	char do_avoid[100];

	strcpy(do_avoid, avoid(mt));
	do_avoid[0] = toupper(do_avoid[0]);
	switch (ptype) {
	case planet_type_rocky:
		if (has_starbase) {
			snprintf(buffer, buflen,
			"This %s %s is %s %s there is an orbiting starbase which %s %s %s %s %s and %s %s %s %s. %s the %s %s. %s %s.\n",
			barren(mt), rocky_planet(mt), uninhabitable(mt), however(mt), known_for(mt),
			producing(mt), exceptional(mt), qnationality(mt), product(mt), known_for(mt),
			exceptional(mt), qnationality(mt), culture(mt),
			do_avoid, terrible(mt), product(mt), bring_your(mt), traveling_accessory(mt));
		} else {
			snprintf(buffer, buflen,
			"This %s %s is %s\n",
			barren(mt), rocky_planet(mt), uninhabitable(mt));
		}
		break;
	case planet_type_gas_giant:
		if (has_starbase) {
			snprintf(buffer, buflen,
			"This %s %s is %s %s there is an orbiting starbase which %s %s %s %s %s and %s %s %s %s. %s the %s %s. %s %s.\n",
			beautiful(mt), gas_giant(mt), uninhabitable(mt), however(mt), known_for(mt),
			producing(mt), exceptional(mt), qnationality(mt), product(mt), known_for(mt),
			exceptional(mt), qnationality(mt), culture(mt),
			do_avoid, terrible(mt), product(mt), bring_your(mt), traveling_accessory(mt));
		} else {
			snprintf(buffer, buflen,
				"This %s %s is %s\n",
				beautiful(mt), gas_giant(mt), uninhabitable(mt));
		}
		break;
	case planet_type_earthlike:
		snprintf(buffer, buflen, "This %s %s %s %s %s %s %s and %s %s %s %s. %s the %s %s.  %s %s.\n",
			climate(mt, ptype), planet(mt), known_for(mt), producing(mt),
				exceptional(mt), qnationality(mt), product(mt),
				known_for(mt), exceptional(mt), qnationality(mt),
				culture(mt),
				do_avoid, terrible(mt), product(mt),
				bring_your(mt), traveling_accessory(mt));
		break;
	case planet_type_ice_giant:
		if (has_starbase) {
			snprintf(buffer, buflen,
			"This %s %s is %s %s there is an orbiting starbase which %s %s %s %s %s and %s %s %s %s. %s the %s %s. %s %s.\n",
			beautiful(mt), ice_giant(mt), uninhabitable(mt), however(mt), known_for(mt),
			producing(mt), exceptional(mt), qnationality(mt), product(mt), known_for(mt),
			exceptional(mt), qnationality(mt), culture(mt),
			do_avoid, terrible(mt), product(mt), bring_your(mt), traveling_accessory(mt));
		} else {
			snprintf(buffer, buflen,
			"This %s %s is %s\n",
			beautiful(mt), ice_giant(mt), uninhabitable(mt));
		}
		break;
	}
	break_lines(buffer, line_len);
}

void starbase_attack_warning(struct mtwist_state *mt, char *buffer,
			int buflen, int line_len)
{
	char do_avoid[100];

	strcpy(do_avoid, avoid(mt));
	do_avoid[0] = toupper(do_avoid[0]);

	snprintf(buffer, buflen, "%s %s and %s or %s %s\n",
		be_advised(mt), cease_fire(mt), get_lost(mt), you_will_be(mt),
		destroyed(mt));
	break_lines(buffer, line_len);
}

void cop_attack_warning(struct mtwist_state *mt, char *buffer,
			int buflen, int line_len)
{
	snprintf(buffer, buflen, "You are in violation of spacefaring ordinance 773-za"
			" and %s %s" , you_will_be(mt), destroyed(mt));
	break_lines(buffer, line_len);
}

int random_initial(struct mtwist_state *mt)
{
	return 'A' + mtwist_int(mt, 26);
}

void character_name(struct mtwist_state *mt, char *buffer,
			int buflen)
{
	char name[3][100];

	random_name(mt, name[0], sizeof(name[0]));
	random_name(mt, name[1], sizeof(name[1]));
	random_name(mt, name[2], sizeof(name[2]));

	switch (mtwist_int(mt, 20)) {
	case 0:
		snprintf(buffer, buflen, "%s %s %s %s%s", character_title(mt),
				name[0], name[1], name[2], post_nominal_letters(mt));
		break;
	case 1:
		snprintf(buffer, buflen, "%s %c. %c. %s%s", character_title(mt),
				random_initial(mt), random_initial(mt), name[0],
				post_nominal_letters(mt));
		break;
	case 2:
	case 3:
		snprintf(buffer, buflen, "%s %c. %s %s%s", character_title(mt),
				random_initial(mt), name[0], name[1],
				post_nominal_letters(mt));
		break;
	case 4:
		snprintf(buffer, buflen, "%s %c. %s%s", character_title(mt),
				random_initial(mt), name[0], post_nominal_letters(mt));
		break;
	case 5:
		snprintf(buffer, buflen, "%s %s%s", character_title(mt),
				name[0], post_nominal_letters(mt));
		break;
	case 6:
		snprintf(buffer, buflen, "%s %s %s%s", character_title(mt),
				name[0], name[1],
				post_nominal_letters(mt));
		break;
	case 7:
	case 9:
		snprintf(buffer, buflen, "%s %c. %c. %s%s", character_title(mt),
				random_initial(mt), random_initial(mt), name[0],
				post_nominal_letters(mt));
		break;
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
		snprintf(buffer, buflen, "%s ", character_title(mt));
		int n = strlen(buffer);
		int remaining = buflen - n;
		npc_name(mt, &buffer[n], remaining);
		uppercase(buffer);
		break;
	default:
		npc_name(mt, buffer, buflen);
		uppercase(buffer);
		break;
	}
}

void ship_name(struct mtwist_state *mt, char *buffer, int buflen)
{
	char name[100];

	random_name(mt, name, sizeof(name));
	switch (mtwist_int(mt, 6)) {
	case 0:
		snprintf(buffer, buflen, "%s %s",
			random_word(mt, SpaceWorthy, ARRAYSIZE(SpaceWorthy)),
			random_word(mt, Animal, ARRAYSIZE(Animal)));
		break;
	case 1:
		snprintf(buffer, buflen, "%s %s",
			random_word(mt, Animal, ARRAYSIZE(Animal)),
			random_word(mt, SpaceWorthy, ARRAYSIZE(SpaceWorthy)));
		break;
	case 2:
		snprintf(buffer, buflen, "%s %s",
			random_word(mt, ShipThing, ARRAYSIZE(ShipThing)),
			random_word(mt, SpaceWorthy, ARRAYSIZE(SpaceWorthy)));
		break;
	case 3:
		snprintf(buffer, buflen, "%s %s",
			name, random_word(mt, SpaceWorthy, ARRAYSIZE(SpaceWorthy)));
		break;
	case 4:
		snprintf(buffer, buflen, "%s %s",
			random_word(mt, SpaceWorthy, ARRAYSIZE(SpaceWorthy)), name);
		break;
	default:
		snprintf(buffer, buflen, "%s %s",
			random_word(mt, SpaceWorthy, ARRAYSIZE(SpaceWorthy)),
			random_word(mt, ShipThing, ARRAYSIZE(ShipThing)));
		break;
	}
}

static char random_letter(struct mtwist_state *mt)
{
	int x = mtwist_next(mt) % 26 + 'A';
	return (char) x;
}

static char random_digit(struct mtwist_state *mt)
{
	int x = mtwist_next(mt) % 10 + '0';
	return (char) x;
}

void robot_name(struct mtwist_state *mt, char *buffer, int buflen)
{
	static char *robot_patterns[] = { "LNLN", "LNLL", "LNNL", "LLNL", "LLNN", "LNNN" };
	char *pattern = random_word(mt, robot_patterns, ARRAYSIZE(robot_patterns));
	int i;
	char name[20];

	memset(name, 0, sizeof(name));
	for (i = 0; pattern[i]; i++) {
		switch (pattern[i]) {
		case 'L':
			name[i] = random_letter(mt);
			break;
		case 'N':
			name[i] = random_digit(mt);
			break;
		}
	}
	strlcpy(buffer, name, buflen);
}

enum planet_type planet_type_from_string(char *s)
{
	if (strcmp(s, "gas-giant") == 0)
		return planet_type_gas_giant;
	if (strcmp(s, "earthlike") == 0)
		return planet_type_earthlike;
	if (strcmp(s, "rocky") == 0)
		return planet_type_rocky;
	if (strcmp(s, "ice-giant") == 0)
		return planet_type_ice_giant;
	return planet_type_rocky;
}

void generate_crime(struct mtwist_state *mt, char *buffer, int buflen)
{
	int x = (int) (((double) mtwist_next(mt) / (double) (0xffffffff)) * 1000);
	char *modifier, *act;

	switch (x % 3) {
	case 0:
		act = random_word(mt, Crime, ARRAYSIZE(Crime));
		snprintf(buffer, buflen, "%s", act);
		break;
	case 1:
	case 2:
		modifier = random_word(mt, CrimeModifier, ARRAYSIZE(CrimeModifier));
		act = random_word(mt, Crime, ARRAYSIZE(Crime));
		if (modifier[0] != '+')
			snprintf(buffer, buflen, "%s %s", modifier, act);
		else
			snprintf(buffer, buflen, "%s %s", act, modifier + 1);
		break;
	}
}

#ifdef TEST_TAUNT
#include "mtwist.h"
#include <sys/time.h>
static void set_random_seed(struct mtwist_state **mt)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	*mt = mtwist_init(tv.tv_usec);
}

enum planet_type PlanetType(struct mtwist_state *mt)
{
	int x;
	x = (int) (((double) mtwist_next(mt) / (double) (0xffffffff)) * 4);
	return (enum planet_type) x;
}

static struct option long_options[] = {
	{ "count", required_argument, NULL, 'c' },
	{ "crime", no_argument, NULL, 'C' },
	{ "npc", no_argument, NULL, 'n' },
	{ "planet", no_argument, NULL, 'p' },
	{ "robot", no_argument, NULL, 'r' },
	{ "ship", no_argument, NULL, 's' },
	{ "taunt", no_argument, NULL, 't' },
	{ "warning", no_argument, NULL, 'w' },
	{ 0, 0, 0, 0 },
};

static void usage(char *program)
{
	int i;

	fprintf(stderr, "%s: usage:\n", program);
	fprintf(stderr, "%s [ options ]\n", program);
	fprintf(stderr, "Options are:\n");

	for (i = 0; i < (int) ARRAYSIZE(long_options); i++)
		fprintf(stderr, "  --%s\n", long_options[i].name);
	exit(1);
}

int main(int argc, char *argv[])
{
	int i, c, rc;
	char buffer[1000];
	struct mtwist_state *mt;
	enum planet_type pt;
	int count = 1;
	int npc_mode = 0;
	int planet_mode = 0;
	int robot_mode = 0;
	int ship_mode = 0;
	int taunt_mode = 0;
	int warning_mode = 0;
	int crime_mode = 0;

	set_random_seed(&mt);

	while (1) {
		int option_index;

		c = getopt_long(argc, argv, "Cc:nprstw", long_options, &option_index);
		if (c < 0) {
			break;
		}
		switch (c) {
		case 'c':
			rc = sscanf(optarg, "%d", &count);
			if (rc != 1)
				count = 0;
			break;
		case 'C':
			crime_mode = 1;
			break;
		case 'n':
			npc_mode = 1;
			break;
		case 'p':
			planet_mode = 1;
			break;
		case 'r':
			robot_mode = 1;
			break;
		case 's':
			ship_mode = 1;
			break;
		case 't':
			taunt_mode = 1;
			break;
		case 'w':
			warning_mode = 1;
			break;
		default:
			fprintf(stderr, "%s: Unknown option.\n", argv[0]);
			usage(argv[0]);
		}
	}

	if (taunt_mode + planet_mode + warning_mode + npc_mode + robot_mode + ship_mode + crime_mode == 0)
		usage(argv[0]);

	for (i = 0; i < count; i++) {
		if (taunt_mode) {
			infinite_taunt(mt, buffer, sizeof(buffer) - 1);
			printf("%s\n", buffer);
		}
		if (planet_mode) {
			pt = PlanetType(mt);
			planet_description(mt, buffer, sizeof(buffer) - 1, 60, pt, i % 2);
			printf("%s\n", buffer);
		}
		if (warning_mode) {
			cop_attack_warning(mt, buffer, sizeof(buffer) - 1, 50);
			printf("%s\n", buffer);
		}
		if (npc_mode) {
			character_name(mt, buffer, sizeof(buffer) - 1);
			printf("%s\n", buffer);
		}
		if (robot_mode) {
			robot_name(mt, buffer, sizeof(buffer) - 1);
			printf("%s\n", buffer);
		}
		if (ship_mode) {
			ship_name(mt, buffer, sizeof(buffer) - 1);
			printf("%s\n", buffer);
		}
		if (crime_mode) {
			generate_crime(mt, buffer, sizeof(buffer) - 1);
			printf("%s\n", buffer);
		}
	}
	free(mt);
	return 0;
}
#endif
