
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
	end
end

show_menu(player_ids[1],
		"MAIN MENU\n" ..
		"TRAINING MISSION 1\n" ..
		"SAVING PLANET ERPH\n",
		"main_menu_callback");

