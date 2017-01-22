
clear_all();

player_ids = get_player_ship_ids();

function main_menu_callback(shipid, selection)
	player_ids = get_player_ship_ids();
	if (selection == 0) then
		text_to_speech(player_ids[1], "Begin mission, training mission one");
		enqueue_lua_script("TRAINING-MISSION-1.LUA");
	elseif (selection == 1) then
		text_to_speech(player_ids[1], "Begin mission, saving planet erph");
		enqueue_lua_script("SAVING-PLANET-ERPH.LUA");
	elseif (selection == 2) then
		text_to_speech(player_ids[1], "Begin mission, space pox");
		enqueue_lua_script("SPACEPOX.LUA");
	elseif (selection == 3) then
		text_to_speech(player_ids[1], "Begin mission, demolisher");
		enqueue_lua_script("DEMOLISHER.LUA");
	end
end

show_menu(player_ids[1],
		"MAIN MENU\n" ..
		"TRAINING MISSION 1\n" ..
		"SAVING PLANET ERPH\n" ..
		"SPACE POX\n" ..
		"DEMOLISHER\n",
		"main_menu_callback");

