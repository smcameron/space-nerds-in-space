#ifndef INFINITE_TAUNT_H__
#define INFINITE_TAUNT_H__

#include "mtwist.h"

enum planet_type {
	planet_type_rocky,
	planet_type_earthlike,
	planet_type_gas_giant,
	planet_type_ice_giant,
};

extern enum planet_type planet_type_from_string(char *s);
extern void infinite_taunt(struct mtwist_state *mt, char *buffer, int buflen);
extern void planet_description(struct mtwist_state *mt, char *buffer, int buflen,
				int line_len, enum planet_type ptype, int has_starbase);
extern void starbase_attack_warning(struct mtwist_state *mt, char *buffer, int buflen, int line_len);
extern void cop_attack_warning(struct mtwist_state *mt, char *buffer, int buflen, int line_len);
extern void character_name(struct mtwist_state *mt, char *buffer, int buflen);
extern void robot_name(struct mtwist_state *mt, char *buffer, int buflen);
extern void ship_name(struct mtwist_state *mt, char *buffer, int buflen);
extern void generate_crime(struct mtwist_state *mt, char *buffer, int buflen);

#endif 
