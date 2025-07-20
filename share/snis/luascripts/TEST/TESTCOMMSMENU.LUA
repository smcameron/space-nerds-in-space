-- tests for comms menu stuff

menu = comms_menu_create(
	"1. item one\n" ..
	"2. item two\n" ..
	"3. item three", "comms_menu_callback");

function comms_menu_callback(choice, ship_id)
	demon_print("Choice " .. tostring(choice) .. " by id " .. tostring(ship_id));
	comms_menu_destroy(menu);
end


player_ids = get_player_ship_ids();
if (player_ids[1]) then
	comms_menu_invoke(menu, player_ids[1]);
end

