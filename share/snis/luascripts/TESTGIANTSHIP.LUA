
-- find the player
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    player_name = get_object_name(player_ids[1]);
    x, y, z = get_object_location(player_ids[1]);
    add_giant_spaceship(x + 10000, y, z);
end

