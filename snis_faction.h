#ifndef SNIS_FACTION_H__
#define SNIS_FACTION_H__

#ifndef DEFINE_FACTION_GLOBALS
#define GLOBAL extern
#else
#define GLOBAL
#endif

GLOBAL char *faction[];
GLOBAL int nfactions;

GLOBAL int snis_read_factions(char *filename);
GLOBAL void snis_free_factions(void);

#undef GLOBAL
#endif
