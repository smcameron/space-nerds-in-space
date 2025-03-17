#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "arraysize.h"
#include "names.h"
#include "mtwist.h"

static char *vowel[] = {
	"a",
	"e",
	"i",
	"o",
	"u",
};

static char *consonant[] = {
	"b", "ch", "d", "f", "g", "j", "k", "l", "m", "n", "p", "r", "s",
	"t", "v", "x", "z", "ph", "sh", "th", "sp", "st",
};

static char *ending[] = {
	"on", "ino", "ion", "an", "ach", "tar", "ron", "o", "son", "berg",
	"ton", "ides", "ski", "nova", "petra", "aster", "ius", "ipha", "ian"
};

static char *pattern[] = {
	"vcvc",
	"cvcvc",
	"cvcv",
	"vcv",
	"cvc",
	"cvv",
	"cvcvv",
	"vccvv",
	"vccvvc",
	"cvccvvc",
	"cvvccvc",
	"cvvccvc",
	"cvvccvc",
	"vcvcd",
	"cvcvcd",
	"cvcvd",
	"vcvd",
	"cvcd",
	"cvvd",
	"cvcvvd",
	"vccvvd",
	"vccvvcd",
	"cvccvvcd",
	"cvvccvcd",
	"cvvccvcd",
	"cvvccvcd",
};

static char *male_name[] = {
	"James",
	"Michael",
	"Robert",
	"John",
	"David",
	"William",
	"Richard",
	"Joseph",
	"Thomas",
	"Christopher",
	"Charles",
	"Daniel",
	"Matthew",
	"Anthony",
	"Mark",
	"Donald",
	"Steven",
	"Andrew",
	"Paul",
	"Joshua",
	"Kenneth",
	"Kevin",
	"Brian",
	"Timothy",
	"Ronald",
	"George",
	"Jason",
	"Edward",
	"Jeffrey",
	"Ryan",
	"Jacob",
	"Nicholas",
	"Gary",
	"Eric",
	"Jonathan",
	"Stephen",
	"Larry",
	"Justin",
	"Scott",
	"Brandon",
	"Benjamin",
	"Samuel",
	"Gregory",
	"Alexander",
	"Patrick",
	"Frank",
	"Raymond",
	"Jack",
	"Dennis",
	"Jerry",
	"Tyler",
	"Aaron",
	"Jose",
	"Adam",
	"Nathan",
	"Henry",
	"Zachary",
	"Douglas",
	"Peter",
	"Kyle",
	"Noah",
	"Ethan",
	"Jeremy",
	"Christian",
	"Walter",
	"Keith",
	"Austin",
	"Roger",
	"Terry",
	"Sean",
	"Gerald",
	"Carl",
	"Dylan",
	"Harold",
	"Jordan",
	"Jesse",
	"Bryan",
	"Lawrence",
	"Arthur",
	"Gabriel",
	"Bruce",
	"Logan",
	"Billy",
	"Joe",
	"Alan",
	"Juan",
	"Elijah",
	"Willie",
	"Albert",
	"Wayne",
	"Randy",
	"Mason",
	"Vincent",
	"Liam",
	"Roy",
	"Bobby",
	"Caleb",
	"Bradley",
	"Russell",
	"Lucas",
};

static char *female_name[] = {
	"Mary",
	"Patricia",
	"Jennifer",
	"Linda",
	"Elizabeth",
	"Barbara",
	"Susan",
	"Jessica",
	"Karen",
	"Sarah",
	"Lisa",
	"Nancy",
	"Sandra",
	"Betty",
	"Ashley",
	"Emily",
	"Kimberly",
	"Margaret",
	"Donna",
	"Michelle",
	"Carol",
	"Amanda",
	"Melissa",
	"Deborah",
	"Stephanie",
	"Rebecca",
	"Sharon",
	"Laura",
	"Cynthia",
	"Dorothy",
	"Amy",
	"Kathleen",
	"Angela",
	"Shirley",
	"Emma",
	"Brenda",
	"Pamela",
	"Nicole",
	"Anna",
	"Samantha",
	"Katherine",
	"Christine",
	"Debra",
	"Rachel",
	"Carolyn",
	"Janet",
	"Maria",
	"Olivia",
	"Heather",
	"Helen",
	"Catherine",
	"Diane",
	"Julie",
	"Victoria",
	"Joyce",
	"Lauren",
	"Kelly",
	"Christina",
	"Ruth",
	"Joan",
	"Virginia",
	"Judith",
	"Evelyn",
	"Hannah",
	"Andrea",
	"Megan",
	"Cheryl",
	"Jacqueline",
	"Madison",
	"Teresa",
	"Abigail",
	"Sophia",
	"Martha",
	"Sara",
	"Gloria",
	"Janice",
	"Kathryn",
	"Ann",
	"Isabella",
	"Judy",
	"Charlotte",
	"Julia",
	"Grace",
	"Amber",
	"Alice",
	"Jean",
	"Denise",
	"Frances",
	"Danielle",
	"Marilyn",
	"Natalie",
	"Beverly",
	"Diana",
	"Brittany",
	"Theresa",
	"Kayla",
	"Alexis",
	"Doris",
	"Lori",
	"Tiffany",
};

static char *surname[] = {
	"Smith",
	"Johnson",
	"Williams",
	"Brown",
	"Jones",
	"Garcia",
	"Miller",
	"Davis",
	"Rodriguez",
	"Martinez",
	"Hernandez",
	"Lopez",
	"Gonzales",
	"Wilson",
	"Anderson",
	"Thomas",
	"Taylor",
	"Moore",
	"Jackson",
	"Martin",
	"Lee",
	"Perez",
	"Thompson",
	"White",
	"Harris",
	"Sanchez",
	"Clark",
	"Ramirez",
	"Lewis",
	"Robinson",
	"Walker",
	"Young",
	"Allen",
	"King",
	"Wright",
	"Scott",
	"Torres",
	"Nguyen",
	"Hill",
	"Flores",
	"Green",
	"Adams",
	"Nelson",
	"Baker",
	"Hall",
	"Rivera",
	"Campbell",
	"Mitchell",
	"Carter",
	"Roberts",
	"Gomez",
	"Phillips",
	"Evans",
	"Turner",
	"Diaz",
	"Parker",
	"Cruz",
	"Edwards",
	"Collins",
	"Reyes",
	"Stewart",
	"Morris",
	"Morales",
	"Murphy",
	"Cook",
	"Rogers",
	"Gutierrez",
	"Ortiz",
	"Morgan",
	"Cooper",
	"Peterson",
	"Bailey",
	"Reed",
	"Kelly",
	"Howard",
	"Ramos",
	"Kim",
	"Cox",
	"Ward",
	"Richardson",
	"Watson",
	"Brooks",
	"Chavez",
	"Wood",
	"James",
	"Bennet",
	"Gray",
	"Mendoza",
	"Ruiz",
	"Hughes",
	"Price",
	"Alvarez",
	"Castillo",
	"Sanders",
	"Patel",
	"Myers",
	"Long",
	"Ross",
	"Foster",
	"Jimenez",
};

static void append_stuff(struct mtwist_state *mt, char buffer[], int bufsize, char *a[], int asize)
{
	int e;

	e = mtwist_next(mt) % asize;
	int len = strlen(buffer) + strlen(a[e]);
	int remaining_chars = bufsize - strlen(buffer) - 1;
	if (remaining_chars > 1)
		strncat(buffer, a[e], remaining_chars);
	if (len < bufsize - 1)
		buffer[len] = '\0';
	else
		buffer[bufsize - 1] = '\0';
}

void random_name(struct mtwist_state *mt, char buffer[], int bufsize)
{
	char *i;
	int  p;

	buffer[0] = '\0';
	p = mtwist_next(mt) % ARRAYSIZE(pattern);

	for (i = pattern[p]; *i; i++) {
		if (*i == 'v')
			append_stuff(mt, buffer, bufsize, vowel, ARRAYSIZE(vowel));
		else if (*i == 'c')
			append_stuff(mt, buffer, bufsize, consonant, ARRAYSIZE(consonant));
		else
			append_stuff(mt, buffer, bufsize, ending, ARRAYSIZE(ending));
		/* printf("zzz buffer = %s, pattern = %s, *i = %c\n", buffer, pattern[p], *i); */
	}
	for (i = buffer; *i; i++)
		*i = toupper(*i);
}

void npc_name(struct mtwist_state *mt, char buffer[], int bufsize)
{
	int s = mtwist_next(mt) % 2;

	int fn;
	if (s == 0)
		fn = mtwist_next(mt) % ARRAYSIZE(male_name);
	else
		fn = mtwist_next(mt) % ARRAYSIZE(female_name);
	int sn = mtwist_next(mt) % ARRAYSIZE(surname);
	snprintf(buffer, bufsize, "%s %s", s ? male_name[fn] : female_name[fn], surname[sn]);
}

#ifdef TEST_NAMES 
#include <stdio.h>
int main(int argc, char *argv[])
{
	int i, seed;
	char buffer[100];
	struct mtwist_state *mt;

	if (argc > 1) {
		sscanf(argv[1], "%d", &seed);
		mt = mtwist_init(seed);
	} else { 
		mt = mtwist_init(12345);
	}

	for (i = 0; i < 1000; i++) {
		random_name(mt, buffer, sizeof(buffer));
		printf("%s\n", buffer);
	}
	return 0;
}
#endif
