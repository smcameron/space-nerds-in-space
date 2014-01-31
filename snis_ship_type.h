#ifndef SNIS_SHIP_TYPE_H__
#define  SNIS_SHIP_TYPE_H__

#define SHIP_CLASS_CRUISER 0
#define SHIP_CLASS_DESTROYER 1
#define SHIP_CLASS_FREIGHTER 2
#define SHIP_CLASS_TANKER 3
#define SHIP_CLASS_TRANSPORT 4
#define SHIP_CLASS_BATTLESTAR 5
#define SHIP_CLASS_STARSHIP 6
#define SHIP_CLASS_ASTEROIDMINER 7
#define SHIP_CLASS_SCIENCE 8
#define SHIP_CLASS_SCOUT 9
#define SHIP_CLASS_DRAGONHAWK 10 
#define SHIP_CLASS_SKORPIO 11 
#define SHIP_CLASS_DISRUPTOR 12 
#define SHIP_CLASS_RESEARCH_VESSEL 13 
#define SHIP_CLASS_CONQUERER 14 
#define SHIP_CLASS_SCRAMBLER 15 
#define SHIP_CLASS_SWORDFISH 16
#define SHIP_CLASS_WOMBAT 17
#define SHIP_CLASS_DREADKNIGHT 18

struct ship_type_entry {
	char *class;
	double max_speed;
	int crew_max;
};

struct ship_type_entry *snis_read_ship_types(char *filename, int *count);
void snis_free_ship_type(struct ship_type_entry *st, int count);

#endif
