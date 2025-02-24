-- Make the computer speak

arg = {...};

if #arg < 2 then
    demon_print("USAGE: SPEAK HELLO THERE");
    return;
end


-- find the players
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    player_name = get_object_name(player_ids[1]);
end

text = "";

for i = 2, #arg do
	text = text .. " " .. arg[i]
end

demon_print("SAYING " .. text);
text_to_speech(player_ids[1], string.lower(text));

