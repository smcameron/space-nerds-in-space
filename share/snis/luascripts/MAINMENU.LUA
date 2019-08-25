-- main menu for lua mission scripts
clear_all();

player_ids = get_player_ship_ids();

function main_menu_callback(shipid, selection)
	player_ids = get_player_ship_ids();
	if (selection == 0) then
		text_to_speech(player_ids[1], "Begin mission, training mission one");
		enqueue_lua_script("TRAINING-MISSION-1");
	elseif (selection == 1) then
		text_to_speech(player_ids[1], "Begin mission, saving planet erph");
		enqueue_lua_script("SAVING-PLANET-ERPH");
	elseif (selection == 2) then
		text_to_speech(player_ids[1], "Begin mission, space pox");
		enqueue_lua_script("SPACEPOX");
	elseif (selection == 3) then
		text_to_speech(player_ids[1], "Begin mission, royal wedding");
		enqueue_lua_script("ROYAL-WEDDING");
	elseif (selection == 4) then
		text_to_speech(player_ids[1], "Begin mission, Cubo de la Muerte");
		enqueue_lua_script("CUBOMUERTE");
	elseif (selection == 5) then
		text_to_speech(player_ids[1], "Begin mission, rescue of the sturnvulf");
		enqueue_lua_script("STURNVULF");
	elseif (selection == 6) then
		text_to_speech(player_ids[1], "Begin mission, kali");
		enqueue_lua_script("KALI");
	elseif (selection == 7) then
		text_to_speech(player_ids[1], "Begin mission, demolisher");
		enqueue_lua_script("DEMOLISHER");
	elseif (selection == 8) then
		text_to_speech(player_ids[1], "Regerating universe");
		enqueue_lua_script("REGENERATE");
	elseif (selection == 9) then
		-- do nothing
	end
end

show_menu(player_ids[1],
		"MAIN MENU\n" ..
		"TRAINING MISSION 1\n" ..
		"SAVING PLANET ERPH\n" ..
		"SPACE POX\n" ..
		"ROYAL WEDDING\n" ..
		"CUBO DE LA MUERTE\n" ..
		"RESCUE OF THE STURNVULF\n" ..
		"KALI\n" ..
		"DEMOLISHER\n" ..
		"REGENERATE UNIVERSE\n" ..
		"EXIT MENU\n",
		"main_menu_callback");

