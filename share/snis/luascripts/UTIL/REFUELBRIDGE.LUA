-- arg = player ship id -- refuel the specified bridge

arg = {...};
player_ids = get_player_ship_ids();

if #arg < 2 then
    demon_print("USAGE: REFUELBRIDGE PLAYER-SHIP-ID  [ ... ]");
    return;
end

for i = 2, #arg do
    id = tonumber(arg[i]);

    found = 0 ;
    for j, v in pairs(player_ids) do
        if (v == id) then
            demon_print("REFUELING PLAYER SHIP ID " .. arg[i]);
	    refuel_player_ship(v);
            found = 1;
        end
    end
    if (found == 0) then
       demon_print("FAILED TO FIND PLAYER ID " .. arg[i]);
    end
end

