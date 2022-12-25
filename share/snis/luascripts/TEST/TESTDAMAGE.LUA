-- TEST DAMAGING PLAYER SHIP FROM LUA
player_ids = get_player_ship_ids();

set_player_damage(player_ids[1], "shields", 128, 0.6666, 0.0, 0.3333);
set_player_damage(player_ids[1], "warp", 255, 0.3333, 0.3333, 0.3333);

