
-- find the player and damage him... I need to work on this script some more.

player_ids = get_player_ship_ids();

if (player_ids[1]) then
    reset_player_ship(player_ids[1]);
    set_player_damage(player_ids[1], "lifesupport", 200);
    set_player_damage(player_ids[1], "shields", 100);
    set_player_damage(player_ids[1], "impulse", 150);
    set_player_damage(player_ids[1], "warp", 90);
    set_player_damage(player_ids[1], "maneuvering", 50);
    set_player_damage(player_ids[1], "phaser", 20);
    set_player_damage(player_ids[1], "sensors", 30);
    set_player_damage(player_ids[1], "comms", 60);
    set_player_damage(player_ids[1], "tractor", 40);
end

