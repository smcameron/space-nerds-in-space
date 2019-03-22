-- add a cargo container near the player's ship

player_ids = get_player_ship_ids();
if player_ids[1] then
	x,y,z = get_object_location(player_ids[1]);
	cargocontainer = add_cargo_container(x + 150, y + 190, z + 500, 0, 0, 0);
end

