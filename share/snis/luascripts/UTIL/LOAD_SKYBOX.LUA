-- restore default skybox (obsolete?)
player_ids = get_player_ship_ids();

arg = {...};
player_ids = get_player_ship_ids();

if #arg < 2 then
    demon_print("USAGE: LOAD_SKYBOX SOLARSYSTEM-NAME");
    return;
end

if (player_ids[1]) then
        for i, player_id in ipairs(player_ids) do
                load_skybox(player_id, string.lower(arg[2]));
        end
end

