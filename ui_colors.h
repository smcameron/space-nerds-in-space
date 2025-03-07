#ifndef UI_COLORS_H__
#define UI_COLORS_H__
/*
	Copyright (C) 2015 Stephen M. Cameron
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
#include "snis_graph.h"

#undef GLOBAL

#ifdef DEFINE_UI_COLORS_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

/* BLUE, CYAN, and GREEN are ugly special cases due do being dynamically assigned,
 * so they cannot be used as constant initializers.  So we need a way to fix them up
 * at runtime, so we have this ugly hack.  This happened as part of the nav
 * trident implementation.
 */

#define BLUE_FIXUP -99
#define CYAN_FIXUP -100
#define GREEN_FIXUP -101

struct ui_color_entry {
	int index;
	char *name;
};

#define UI_COLOR(x) (ui_color.u.map.x.index)

struct ui_color_map {
	struct ui_color_entry first_color;

	struct ui_color_entry common_border;
	struct ui_color_entry common_red_alert;
	struct ui_color_entry common_text;

	struct ui_color_entry slider_good;
	struct ui_color_entry slider_caution;
	struct ui_color_entry slider_warning;
	struct ui_color_entry slider_black;

	struct ui_color_entry lobby_connecting;
	struct ui_color_entry lobby_selected_server;
	struct ui_color_entry lobby_connect_ok;
	struct ui_color_entry lobby_connect_not_ok;
	struct ui_color_entry lobby_cancel;
	struct ui_color_entry lobby_server_heading;

	struct ui_color_entry network_setup_text;
	struct ui_color_entry network_setup_logo;
	struct ui_color_entry network_setup_active;
	struct ui_color_entry network_setup_inactive;
	struct ui_color_entry network_setup_selected;
	struct ui_color_entry network_setup_input;
	struct ui_color_entry network_setup_role;

	struct ui_color_entry nav_gauge;
	struct ui_color_entry nav_gauge_needle;
	struct ui_color_entry nav_heading_ring;
	struct ui_color_entry nav_mark_ring;
	struct ui_color_entry nav_button;
	struct ui_color_entry nav_reverse_button;
	struct ui_color_entry nav_slider;
	struct ui_color_entry nav_text;
	struct ui_color_entry nav_warning;
	struct ui_color_entry nav_ring;
	struct ui_color_entry nav_altitude_line_pos; /* altitude lines above nav ring */
	struct ui_color_entry nav_altitude_line_neg; /* altitude lines below nav ring */
	struct ui_color_entry nav_projected_ring;
	struct ui_color_entry nav_self;
	struct ui_color_entry nav_entity;
	struct ui_color_entry nav_asteroid;
	struct ui_color_entry nav_ship;
	struct ui_color_entry nav_static;
	struct ui_color_entry nav_forward_vector;
	struct ui_color_entry nav_torpedo;
	struct ui_color_entry nav_laser;
	struct ui_color_entry nav_heading_vector;
	struct ui_color_entry nav_weapon_vector;
	struct ui_color_entry nav_science_vector;
	struct ui_color_entry nav_comms_curr_vector;
	struct ui_color_entry nav_comms_desired_vector;
	struct ui_color_entry nav_entity_label;
	struct ui_color_entry nav_science_select;
	struct ui_color_entry nav_trident_ship;
	struct ui_color_entry nav_planet;
	struct ui_color_entry nav_starbase;
	struct ui_color_entry nav_warpgate;

	struct ui_color_entry weap_radar;
	struct ui_color_entry weap_gauge;
	struct ui_color_entry weap_gauge_needle;
	struct ui_color_entry weap_slider;
	struct ui_color_entry weap_radar_blip;
	struct ui_color_entry weap_sci_selection;
	struct ui_color_entry weap_torpedoes_loading;
	struct ui_color_entry weap_torpedoes_loaded;
	struct ui_color_entry weap_warning;
	struct ui_color_entry weap_gunsight;
	struct ui_color_entry weap_targeting;
	struct ui_color_entry weap_in_range;

	struct ui_color_entry eng_gauge;
	struct ui_color_entry eng_gauge_needle;
	struct ui_color_entry eng_power_meter;
	struct ui_color_entry eng_coolant_meter; /* not used */
	struct ui_color_entry eng_temperature;
	struct ui_color_entry eng_button;
	struct ui_color_entry eng_selected_button;
	struct ui_color_entry eng_warning;
	struct ui_color_entry eng_disabled;
	struct ui_color_entry eng_science_graph;
	struct ui_color_entry eng_good_status;
	struct ui_color_entry eng_caution_status;
	struct ui_color_entry eng_warning_status;

	struct ui_color_entry science_graph_plot_strong;
	struct ui_color_entry science_graph_plot_weak;
	struct ui_color_entry science_graph_grid;
	struct ui_color_entry science_data_label;
	struct ui_color_entry science_annotation;

	struct ui_color_entry damcon_arena_border;
	struct ui_color_entry damcon_robot;
	struct ui_color_entry damcon_system;
	struct ui_color_entry damcon_part;
	struct ui_color_entry damcon_socket;
	struct ui_color_entry damcon_good;
	struct ui_color_entry damcon_caution;
	struct ui_color_entry damcon_warning;
	struct ui_color_entry damcon_wall;
	struct ui_color_entry damcon_button;
	struct ui_color_entry damcon_selected_button;

	struct ui_color_entry sci_plane_ring;
	struct ui_color_entry sci_basis_ring_1;
	struct ui_color_entry sci_basis_ring_2;
	struct ui_color_entry sci_basis_ring_3;
	struct ui_color_entry sci_coords;
	struct ui_color_entry sci_button;
	struct ui_color_entry sci_slider;
	struct ui_color_entry sci_ball_ring;
	struct ui_color_entry sci_ball_beam;
	struct ui_color_entry sci_ball_default_blip;
	struct ui_color_entry sci_ball_ship;
	struct ui_color_entry sci_ball_player;
	struct ui_color_entry sci_ball_asteroid;
	struct ui_color_entry sci_ball_derelict;
	struct ui_color_entry sci_ball_planet;
	struct ui_color_entry sci_ball_black_hole;
	struct ui_color_entry sci_ball_starbase;
	struct ui_color_entry sci_ball_warpgate;
	struct ui_color_entry sci_ball_energy;
	struct ui_color_entry sci_ball_monster;
	struct ui_color_entry sci_plane_ship_vector;
	struct ui_color_entry sci_plane_weapon_vector;
	struct ui_color_entry sci_plane_beam;
	struct ui_color_entry sci_plane_self;
	struct ui_color_entry sci_plane_default;
	struct ui_color_entry sci_plane_popout_arc;
	struct ui_color_entry sci_plane_npc_laser;
	struct ui_color_entry sci_plane_player_laser;
	struct ui_color_entry sci_wireframe;
	struct ui_color_entry sci_details_text;
	struct ui_color_entry sci_waypoint;
	struct ui_color_entry sci_selected_waypoint;
	struct ui_color_entry sci_warning;
	struct ui_color_entry sci_pull_down_menu;
	struct ui_color_entry sci_pull_down_menu_selection;

	struct ui_color_entry comms_button;
	struct ui_color_entry comms_slider;
	struct ui_color_entry comms_text;
	struct ui_color_entry comms_plaintext;
	struct ui_color_entry comms_red_alert;
	struct ui_color_entry comms_warning;
	struct ui_color_entry comms_neutral;
	struct ui_color_entry comms_good_status;
	struct ui_color_entry comms_encrypted;
	struct ui_color_entry comms_computer_output;

	struct ui_color_entry main_sci_selection;
	struct ui_color_entry main_text;
	struct ui_color_entry main_warning;

	struct ui_color_entry demon_default;
	struct ui_color_entry demon_default_dead;
	struct ui_color_entry demon_axes;
	struct ui_color_entry demon_group_text;
	struct ui_color_entry demon_asteroid;
	struct ui_color_entry demon_planet;
	struct ui_color_entry demon_black_hole;
	struct ui_color_entry demon_derelict;
	struct ui_color_entry demon_nebula;
	struct ui_color_entry demon_starbase;
	struct ui_color_entry demon_wormhole;
	struct ui_color_entry demon_spacemonster;
	struct ui_color_entry demon_victim_vector;
	struct ui_color_entry demon_cross;
	struct ui_color_entry demon_selection_box;
	struct ui_color_entry demon_patrol_route;
	struct ui_color_entry demon_selected_button;
	struct ui_color_entry demon_deselected_button;
	struct ui_color_entry demon_self;
	struct ui_color_entry demon_input;
	struct ui_color_entry demon_pull_down_menu;
	struct ui_color_entry demon_pull_down_menu_selection;

	struct ui_color_entry death_text;

	struct ui_color_entry special_options;
	struct ui_color_entry help_text;
	struct ui_color_entry tooltip;
	struct ui_color_entry sciplane_tooltip;

	struct ui_color_entry warp_hash;

	struct ui_color_entry starmap_star;
	struct ui_color_entry starmap_home_star;
	struct ui_color_entry starmap_warp_lane;
	struct ui_color_entry starmap_far_warp_lane;
	struct ui_color_entry starmap_grid_color;

	struct ui_color_entry quit_text;
	struct ui_color_entry quit_border;
	struct ui_color_entry quit_selection;
	struct ui_color_entry quit_unselected;

	struct ui_color_entry launcher_button;
	struct ui_color_entry launcher_text;
	struct ui_color_entry launcher_gauge;
	struct ui_color_entry launcher_gauge_needle;
	struct ui_color_entry launcher_missing_assets;

	struct ui_color_entry rotating_wombat;

	struct ui_color_entry last_color;
};

GLOBAL struct ui_color_map_accessor {
	union {
		struct ui_color_map map;
		struct ui_color_entry entry[sizeof(struct ui_color_map) / sizeof(struct ui_color_entry)];
	} u;
} ui_color
#ifdef DEFINE_UI_COLORS_GLOBALS
	= {
	.u.map.first_color		= { GREEN_FIXUP,	"first color" } ,

	.u.map.common_border		= { BLUE_FIXUP,		"common-border" },
	.u.map.common_red_alert		= { RED,		"common-red-alert" },
	.u.map.common_text		= { GREEN_FIXUP,	"common-text" },

	.u.map.slider_good		= { DARKGREEN,		"slider-good" },
	.u.map.slider_caution		= { AMBER,		"slider-caution" },
	.u.map.slider_warning		= { RED,		"slider-warning" },
	.u.map.slider_black		= { BLACK,		"slider-black" },

	.u.map.lobby_connecting		= { WHITE,		"lobby-connecting" },
	.u.map.lobby_selected_server	= { GREEN_FIXUP,	"lobby-selected-server" },
	.u.map.lobby_connect_ok		= { GREEN_FIXUP,	"lobby-connect-ok" },
	.u.map.lobby_connect_not_ok	= { RED,		"lobby-connect-not-ok" },
	.u.map.lobby_cancel		= { WHITE,		"lobby-cancel" },
	.u.map.lobby_server_heading	= { DARKGREEN,		"lobby-server-heading" },

	.u.map.network_setup_text	= { GREEN_FIXUP,	"network-setup-text" },
	.u.map.network_setup_logo	= { DARKGREEN,		"network-setup-logo" },
	.u.map.network_setup_active	= { GREEN_FIXUP,	"network-setup-active" },
	.u.map.network_setup_inactive	= { RED,		"network-setup-inactive" },
	.u.map.network_setup_selected	= { WHITE,		"network-setup-selected" },
	.u.map.network_setup_input	= { GREEN_FIXUP,	"network-setup-input" },
	.u.map.network_setup_role	= { GREEN_FIXUP,	"network-setup-role" },

	.u.map.nav_gauge		= { AMBER,		"nav-gauge" },
	.u.map.nav_gauge_needle		= { RED,		"nav-gauge-needle" },
	.u.map.nav_heading_ring		= { ORANGERED,		"nav-heading-ring" },
	.u.map.nav_mark_ring		= { DARKRED,		"nav-mark-ring" },
	.u.map.nav_button		= { AMBER,		"nav-button" },
	.u.map.nav_reverse_button	= { RED,		"nav-reverse-button" },
	.u.map.nav_slider		= { AMBER,		"nav-slider" },
	.u.map.nav_text			= { GREEN_FIXUP,	"nav-text" },
	.u.map.nav_warning		= { RED,		"nav-warning" },
	.u.map.nav_ring			= { DARKRED,		"nav-ring" },
	.u.map.nav_altitude_line_pos	= { DARKRED,		"nav-altitude-line-pos" },
	.u.map.nav_altitude_line_neg	= { DARKRED,		"nav-altitude-line-neg" },
	.u.map.nav_projected_ring	= { DARKRED,		"nav-projected-ring" },
	.u.map.nav_self			= { CYAN_FIXUP,		"nav-self" },
	.u.map.nav_entity		= { GREEN_FIXUP,	"nav-entity" },
	.u.map.nav_asteroid		= { DARKGREEN,		"nav-asteroid" },
	.u.map.nav_ship			= { LIMEGREEN,		"nav-ship" },
	.u.map.nav_static		= { -1,			"nav-static" },
	.u.map.nav_forward_vector	= { WHITE,		"nav-forward-vector" },
	.u.map.nav_torpedo		= { YELLOW,		"nav-torpedo" },
	.u.map.nav_laser		= { YELLOW,		"nav-laser" },
	.u.map.nav_heading_vector	= { WHITE,		"nav-heading-vector" },
	.u.map.nav_weapon_vector	= { CYAN_FIXUP,		"nav-weapon-vector" },
	.u.map.nav_science_vector	= { GREEN_FIXUP,	"nav-science-vector" },
	.u.map.nav_comms_curr_vector	= { AMBER,		"nav-comms-vector" },
	.u.map.nav_comms_desired_vector	= { YELLOW,		"nav-comms-vector" },
	.u.map.nav_entity_label		= { GREEN_FIXUP,	"nav-entity-label" },
	.u.map.nav_science_select	= { GREEN_FIXUP,	"nav-science-select" },
	.u.map.nav_trident_ship		= { AMBER,		"nav-trident-ship" },
	.u.map.nav_planet		= { DARKTURQUOISE,	"nav-planet" },
	.u.map.nav_starbase		= { ORANGE,		"nav-starbase" },
	.u.map.nav_warpgate		= { ORANGE,		"nav-warpgate" },

	.u.map.weap_radar		= { AMBER,		"weap-radar" },
	.u.map.weap_gauge		= { AMBER,		"weap-gauge" },
	.u.map.weap_gauge_needle	= { RED,		"weap-gauge-needle" },
	.u.map.weap_slider		= { AMBER,		"weap-slider" },
	.u.map.weap_radar_blip		= { ORANGERED,		"weap-radar-blip" },
	.u.map.weap_sci_selection	= { GREEN_FIXUP,	"weap-sci-selection" },
	.u.map.weap_torpedoes_loading	= { RED,		"weap-torpedoes_loading" },
	.u.map.weap_torpedoes_loaded	= { AMBER,		"weap-torpedoes_loaded" },
	.u.map.weap_warning		= { RED,		"weap-warning" },
	.u.map.weap_gunsight		= { GREEN_FIXUP,	"weap-gunsight" },
	.u.map.weap_targeting		= { BLUE_FIXUP,		"weap-targeting" },
	.u.map.weap_in_range		= { ORANGERED,		"weap-in-range" },

	.u.map.eng_gauge		= { AMBER,		"eng-gauge" },
	.u.map.eng_gauge_needle		= { RED,		"eng-gauge-needle" },
	.u.map.eng_power_meter		= { AMBER,		"eng-power-meter" },
	.u.map.eng_coolant_meter	= { BLUE_FIXUP,		"eng-coolant-meter" },
	.u.map.eng_temperature		= { AMBER,		"eng-temperature" },
	.u.map.eng_button		= { AMBER,		"eng-button" },
	.u.map.eng_selected_button	= { WHITE,		"eng-selected-button" },
	.u.map.eng_warning		= { RED,		"eng-warning" },
	.u.map.eng_disabled		= { RED,		"eng-disabled" },
	.u.map.eng_science_graph	= { AMBER,		"eng-science-graph" },
	.u.map.eng_good_status		= { GREEN_FIXUP,	"eng-good-status" },
	.u.map.eng_caution_status	= { YELLOW,		"eng-caution-status" },
	.u.map.eng_warning_status	= { ORANGERED,		"eng-warning-status" },

	.u.map.science_graph_plot_strong = { LIMEGREEN,		"science-graph-plot-strong" },
	.u.map.science_graph_plot_weak	= { RED,		"science-graph-plot-weak" },
	.u.map.science_graph_grid	= { DARKGREEN,		"science-graph-grid" },
	.u.map.science_data_label	= { GREEN_FIXUP,	"science-data-label" },
	.u.map.science_annotation	= { CYAN_FIXUP,		"science-annotation" },

	.u.map.damcon_arena_border	= { RED,		"damcon-arena-border" },
	.u.map.damcon_robot		= { GREEN_FIXUP,	"damcon-robot" },
	.u.map.damcon_system		= { AMBER,		"damcon-system" },
	.u.map.damcon_part		= { YELLOW,		"damcon-part" },
	.u.map.damcon_socket		= { WHITE,		"damcon-socket" },
	.u.map.damcon_good		= { GREEN_FIXUP,	"damcon-good" },
	.u.map.damcon_caution		= { YELLOW,		"damcon-caution" },
	.u.map.damcon_warning		= { RED,		"damcon-warning" },
	.u.map.damcon_wall		= { AMBER,		"damcon-wall" },
	.u.map.damcon_button		= { AMBER,		"damcon-button" },
	.u.map.damcon_selected_button	= { WHITE,		"damcon-selected-button" },

	.u.map.sci_plane_ring		= { DARKRED,		"sci-plane-ring" },
	.u.map.sci_basis_ring_1		= { RED,		"sci-basis-ring-1" },
	.u.map.sci_basis_ring_2		= { DARKGREEN,		"sci-basis-ring-2" },
	.u.map.sci_basis_ring_3		= { BLUE_FIXUP,		"sci-basis-ring-3" },
	.u.map.sci_coords		= { CYAN_FIXUP,		"sci-coords" },
	.u.map.sci_button		= { GREEN_FIXUP,	"sci-button" },
	.u.map.sci_slider		= { DARKGREEN,		"sci-slider" },
	.u.map.sci_ball_ring		= { DARKRED,		"sci-ball-ring" },
	.u.map.sci_ball_beam		= { CYAN_FIXUP,		"sci-ball-beam" },
	.u.map.sci_ball_default_blip	= { GREEN_FIXUP,	"sci-ball-default-blip" },
	.u.map.sci_ball_ship		= { LIMEGREEN,		"sci-ball-ship" },
	.u.map.sci_ball_player		= { DARKGREEN,		"sci-ball-ship" },
	.u.map.sci_ball_asteroid	= { AMBER,		"sci-ball-asteroid" },
	.u.map.sci_ball_derelict	= { ORANGERED,		"sci-ball-derelict" },
	.u.map.sci_ball_planet		= { BLUE_FIXUP,		"sci-ball-planet" },
	.u.map.sci_ball_black_hole	= { CYAN_FIXUP,		"sci-ball-black-hole" },
	.u.map.sci_ball_starbase	= { WHITE,		"sci-ball-starbase" },
	.u.map.sci_ball_warpgate	= { WHITE,		"sci-ball-warpgate" },
	.u.map.sci_ball_energy		= { LIMEGREEN,		"sci-ball-energy" },
	.u.map.sci_ball_monster		= { ORANGERED,		"sci-ball-monster" },
	.u.map.sci_plane_ship_vector	= { RED,		"sci-plane-ship-vector" },
	.u.map.sci_plane_weapon_vector	= { CYAN_FIXUP,		"sci-plane-weapon-vector" },
	.u.map.sci_plane_beam		= { CYAN_FIXUP,		"sci-plane-beam" },
	.u.map.sci_plane_self		= { CYAN_FIXUP,		"sci-plane-self" },
	.u.map.sci_plane_default	= { GREEN_FIXUP,	"sci-plane-default" },
	.u.map.sci_plane_popout_arc	= { DARKTURQUOISE,	"sci-plane-popout-arc" },
	.u.map.sci_plane_npc_laser	= { ORANGERED,		"sci-plane-npc-laser" },
	.u.map.sci_plane_player_laser	= { LIMEGREEN,		"sci-plane-player-laser" },
	.u.map.sci_wireframe		= { GREEN_FIXUP,	"sci-wireframe" },
	.u.map.sci_details_text		= { GREEN_FIXUP,	"sci-details-text" },
	.u.map.sci_waypoint		= { CYAN_FIXUP,		"sci-waypoint" },
	.u.map.sci_selected_waypoint	= { YELLOW,		"sci-selected-waypoint" },
	.u.map.sci_warning		= { RED,		"sci-warning" },
	.u.map.sci_pull_down_menu	= { GREEN_FIXUP,	"sci-pull-down-menu" },
	.u.map.sci_pull_down_menu_selection
					= { YELLOW,		"sci-pull-down-menu-selection" },

	.u.map.comms_button		= { GREEN_FIXUP,	"comms-button" },
	.u.map.comms_slider		= { GREEN_FIXUP,	"comms-slider" },
	.u.map.comms_text		= { GREEN_FIXUP,	"comms-text" },
	.u.map.comms_plaintext		= { WHITE,		"comms-plaintext" },
	.u.map.comms_red_alert		= { RED,		"comms-red-alert" },
	.u.map.comms_warning		= { RED,		"comms-warning" },
	.u.map.comms_neutral		= { DARKGREEN,		"comms-neutral" },
	.u.map.comms_good_status	= { LIMEGREEN,		"comms-good-status" },
	.u.map.comms_encrypted		= { CYAN_FIXUP,		"comms-encrypted" },
	.u.map.comms_computer_output	= { AMBER,		"comms-computer-output" },

	.u.map.main_sci_selection	= { GREEN_FIXUP,	"main-sci-selection" },
	.u.map.main_text		= { GREEN_FIXUP,	"main-text" },
	.u.map.main_warning		= { RED,		"main-warning" },

	.u.map.demon_default		= { GREEN_FIXUP,	"demon-default" },
	.u.map.demon_default_dead	= { RED,		"demon-default-dead" },
	.u.map.demon_axes		= { DARKGREEN,		"demon-axes" },
	.u.map.demon_group_text		= { GREEN_FIXUP,	"demon-group-text" },
	.u.map.demon_asteroid		= { AMBER,		"demon-asteroid" },
	.u.map.demon_planet		= { GREEN_FIXUP,	"demon-planet" },
	.u.map.demon_black_hole		= { BLUE_FIXUP,		"demon-black-hole" },
	.u.map.demon_derelict		= { BLUE_FIXUP,		"demon-derelict" },
	.u.map.demon_nebula		= { MAGENTA,		"demon-nebula" },
	.u.map.demon_starbase		= { RED,		"demon-starbase" },
	.u.map.demon_wormhole		= { WHITE,		"demon-wormhole" },
	.u.map.demon_spacemonster	= { GREEN_FIXUP,	"demon-spacemonster" },
	.u.map.demon_victim_vector	= { RED,		"demon-victim-vector" },
	.u.map.demon_cross		= { BLUE_FIXUP,		"demon-cross" },
	.u.map.demon_selection_box	= { WHITE,		"demon-selection-box" },
	.u.map.demon_patrol_route	= { WHITE,		"demon-patrol-route" },
	.u.map.demon_selected_button	= { WHITE,		"demon-selected-button" },
	.u.map.demon_deselected_button	= { GREEN_FIXUP,	"demon-deselected-button" },
	.u.map.demon_self		= { RED,		"demon-self" },
	.u.map.demon_input		= { GREEN_FIXUP,	"demon-input" },
	.u.map.demon_pull_down_menu	= { GREEN_FIXUP,	"demon-pull-down-menu" },
	.u.map.demon_pull_down_menu_selection
					= { YELLOW,		"demon-pull-down-menu-selection" },

	.u.map.death_text		= { RED,		"death-text" },

	.u.map.special_options		= { WHITE,		"special-options" },
	.u.map.help_text		= { WHITE,		"help-text" },
	.u.map.tooltip			= { LIMEGREEN,		"tooltip" },
	.u.map.sciplane_tooltip		= { LIMEGREEN,		"sciplane-tooltip" },

	.u.map.warp_hash		= { WHITE,		"warp-hash" },

	.u.map.starmap_star		= { LIMEGREEN,		"starmap-star" },
	.u.map.starmap_home_star	= { WHITE,		"starmap-home-star" },
	.u.map.starmap_warp_lane	= { CYAN_FIXUP,		"starmap-warp-lane" },
	.u.map.starmap_far_warp_lane	= { AMBER,		"starmap-far-warp-lane" },
	.u.map.starmap_grid_color	= { DARKRED,		"starmap-grid-color" },

	.u.map.quit_text		= { WHITE,		"quit-text"},
	.u.map.quit_border		= { RED,		"quit-border"},
	.u.map.quit_selection		= { WHITE,		"quit-selection"},
	.u.map.quit_unselected		= { RED,		"quit-unselected"},

	.u.map.launcher_button		= { LIMEGREEN,		"launcher-button" },
	.u.map.launcher_text		= { LIMEGREEN,		"launcher-text" },
	.u.map.launcher_gauge		= { AMBER,		"launcher-gauge" },
	.u.map.launcher_gauge_needle	= { RED,		"launcher-gauge-needle" },
	.u.map.launcher_missing_assets	= { RED,		"launcher-missing-assets" },

	.u.map.rotating_wombat		= { DARKGREEN,		"rotating-wombat" },

	.u.map.last_color		= { GREEN_FIXUP,	"last-color" },
	}
#endif
;

extern void fixup_ui_color(int old_color, int new_color);
extern void modify_ui_color(char *ui_component, int new_color);

#undef GLOBAL
#endif
