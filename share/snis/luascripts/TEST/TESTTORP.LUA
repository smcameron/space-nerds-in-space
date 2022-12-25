-- TEST ADDING TORPEDO FROM LUA

player_ids = get_player_ship_ids();

x,y,z = get_object_location(player_ids[1]);
add_torpedo(x, y, z, 100, 0, 0, player_ids[1]);

