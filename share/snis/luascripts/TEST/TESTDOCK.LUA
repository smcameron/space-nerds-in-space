-- test docking player to starbase from Lua script

clear_all();
starbase = add_starbase(100000, 100000, 100000, 0);
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	set_player_damage(player_ids[1], "shields", 128, 0.6666, 0.0, 0.3333);
	set_player_damage(player_ids[1], "warp", 255, 0.3333, 0.3333, 0.3333);
	set_player_damage(player_ids[1], "maneuvering", 200, 0.3333, 0.3333, 0.3333);
	dock_player_to_starbase(player_ids[1], starbase);
end

