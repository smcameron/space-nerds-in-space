-- test adding an explosion near player ship

player_ids = get_player_ship_ids();
if (player_ids[1]) then
	x,y,z = get_object_location(player_ids[1]);
	add_explosion(x + 10000, y, z, 300, 60, 300, 9, 1);
	-- add_explosion(x + 2000, y + 200, z, 300, 60, 300, 9, 0);
end

