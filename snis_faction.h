#ifndef SNIS_FACTION_H__
#define SNIS_FACTION_H__

#ifndef DEFINE_FACTION_GLOBALS
#define GLOBAL extern
#else
#define GLOBAL
#endif

#define FACTION_HOSTILITY_THRESHOLD 0.25f
#define MAX_FACTIONS (8)

GLOBAL int snis_read_factions(char *filename);
GLOBAL void snis_free_factions(void);
GLOBAL int nfactions(void);

GLOBAL char *faction_name(int faction_number);
GLOBAL union vec3 faction_center(int faction_number);
GLOBAL int nearest_faction(union vec3 v);
GLOBAL float faction_hostility(int f1, int f2);

#undef GLOBAL
#endif
