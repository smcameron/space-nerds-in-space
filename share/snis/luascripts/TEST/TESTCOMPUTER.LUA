-- BASIC TEST OF SHIP's COMPUTER

player_ids = get_player_ship_ids();
if (player_ids[1]) then
	computer_command(player_ids[1], "describe the nearest planet");
end

