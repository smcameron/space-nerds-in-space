clear_all();
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	x,y,z = get_object_location(player_ids[1]);
	id = add_derelict("ZARKUS", x + 14000, y, z, 1, 1);
	derelict_set_ships_log(id, "STARDATE 7233.7. SITUATION DIRE. SPACE MONSTERS EVERYWHERE.");
	id = add_derelict("ORION", x, y + 14000, z, 2, 1);
end

