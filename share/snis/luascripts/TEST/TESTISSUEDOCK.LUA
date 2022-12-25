-- TEST ISSUE DOCKING CLEARANCE

player_ids = get_player_ship_ids();

starbase = add_starbase(0, 0, 0, 20);

if (player_ids[1]) then
	issue_docking_clearance(starbase, player_ids[1], "SB-20", 0);
end

