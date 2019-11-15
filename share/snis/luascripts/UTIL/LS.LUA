-- LIST PLAYERS | FACTIONS

arg = {...};

if #arg < 2 then
	demon_print("USAGE: LS PLAYERS | FACTIONS");
	return;
end

function do_list_players()
	id = get_player_ship_ids();
	for i, v in pairs(id) do
		name = get_ship_attribute(v, "sdata.name");
		demon_print(v .. " - " .. name);
	end
end

function do_list_factions()
	for i = 0, 100 do
		name = get_faction_name(i);
		if name == nil then
			break;
		end
		demon_print(i .. " - " .. name);
	end
end

if arg[2] == "PLAYERS" then
	do_list_players();
elseif arg[2] == "FACTIONS" then
	do_list_factions();
else
	demon_print("UNKNOWN LIST SUBCOMMAND " .. arg[2]);
end

