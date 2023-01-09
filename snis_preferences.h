#ifndef SNIS_CLIENT_PREFERENCES_H__
#define SNIS_CLIENT_PREFERENCES_H__
/*
	Copyright (C) 2010 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "xdg_base_dir_spec.h"
#include "snis_tweak.h"

char *snis_prefs_read_default_ship_name(struct xdg_base_context *cx);
void snis_prefs_save_default_ship_name(struct xdg_base_context *cx, char *name);
void snis_prefs_save_checkbox_defaults(struct xdg_base_context *cx, int role_main_v, int role_nav_v, int role_weap_v,
					int role_eng_v, int role_damcon_v, int role_sci_v,
					int role_comms_v, int role_sound_v, int role_projector_v, int role_demon_v,
					int role_text_to_speech_v, int create_ship_v, int join_ship_v);
void snis_prefs_read_checkbox_defaults(struct xdg_base_context *cx, int *role_main_v, int *role_nav_v, int *role_weap_v,
					int *role_eng_v, int *role_damcon_v, int *role_sci_v,
					int *role_comms_v, int *role_sound_v, int *role_projector_v, int *role_demon_v,
					int *role_text_to_speech_v, int *create_ship_v, int *join_ship_v);
void snis_prefs_read_client_tweaks(struct xdg_base_context *cx,
		int (*tweak_fn)(char *cmd, int suppress_unknown_var_error));
void snis_prefs_save_client_tweaks(struct xdg_base_context *cx, struct tweakable_var_descriptor *tweaks, int count);

#endif
