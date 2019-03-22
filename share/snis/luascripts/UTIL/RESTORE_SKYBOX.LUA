-- restore default skybox (obsolete?)
player_ids = get_player_ship_ids();

if (player_ids[1]) then
	load_skybox(player_ids[1], "restore-default");
end

