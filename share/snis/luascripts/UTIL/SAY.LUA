
-- This is a simple example to demonstrate text to speech transmission

-- find the players
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    player_name = get_object_name(player_ids[1]);
end

text_to_speech(player_ids[1], "Demo of lua script saying hello to crew members of the star ship " .. player_name .. "!");

