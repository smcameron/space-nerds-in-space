#ifndef INFINITE_TAUNT_H__
#define INFINITE_TAUNT_H__

extern void infinite_taunt(struct mtwist_state *mt, char *buffer, int buflen);
extern void planet_description(struct mtwist_state *mt, char *buffer, int buflen, int line_len);
extern void starbase_attack_warning(struct mtwist_state *mt, char *buffer, int buflen, int line_len);
extern void cop_attack_warning(struct mtwist_state *mt, char *buffer, int buflen, int line_len);
extern void character_name(struct mtwist_state *mt, char *buffer, int buflen);
extern void robot_name(struct mtwist_state *mt, char *buffer, int buflen);

#endif 
