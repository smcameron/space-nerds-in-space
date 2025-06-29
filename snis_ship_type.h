#ifndef SNIS_SHIP_TYPE_H__
#define  SNIS_SHIP_TYPE_H__

#define SHIP_MESH_SCALE (0.5)

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
#define SHIP_CLASS_VANQUISHER 19
#define SHIP_CLASS_ENFORCER 20
#define SHIP_CLASS_ESCAPE_POD 21
#define SHIP_CLASS_MANTIS 22

struct ship_type_entry {
	char *class;
	char *model_file;
	char *thrust_attachment_file;
	double toughness;
	int warpchance;
	int crew_max;
	double max_speed;
	int ncargo_bays;
	int nrotations;
	float angle[3];
	int has_lasers;
	int has_torpedoes;
	int has_missiles;
	int rts_unit_type;
	float extra_scaling;
	uint8_t manufacturer; /* index into corporations in corporations.c */
	char axis[3];
	float relative_mass;
	float mass_kg;
	uint8_t max_shield_strength;
	float scrap_value;
};

struct ship_type_entry *snis_read_ship_types(char *filename, int *count);
void snis_free_ship_type(struct ship_type_entry *st, int count);
void setup_rts_unit_type_to_ship_type_map(struct ship_type_entry *st, int count);

#endif
