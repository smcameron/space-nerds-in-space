-- arg = effect, player ship id -- reset the specified bridge

arg = {...};
player_ids = get_player_ship_ids();

if #arg < 3 then
    demon_print("USAGE: TERM_EFFECT EFFECT (0-2) PLAYER-SHIP-ID  [ ... ]");
    return;
end

effect = tonumber(arg[2]);
demon_print("Effect = " .. effect);

for i = 3, #arg do
    id = tonumber(arg[i]);


    found = 0 ;
    for j, v in pairs(player_ids) do
        if (v == id) then
            demon_print("EXECUTING TERMINAL EFFECT ON PLAYER SHIP ID " .. arg[i]);
	    terminal_effect(v, 0xff, effect);
            found = 1;
        end
    end
    if (found == 0) then
       demon_print("FAILED TO FIND PLAYER ID " .. arg[i]);
    end
end

