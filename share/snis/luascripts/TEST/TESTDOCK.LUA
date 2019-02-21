clear_all();
starbase = add_starbase(1000, 1000, 1000, 0);
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	dock_player_to_starbase(player_ids[1], starbase);
end

