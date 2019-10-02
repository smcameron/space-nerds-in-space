-- arg = ship id -- set the faction of the specified ship id
-- example: setfaction 0 746 -- sets faction of object with ID 746 to 0

arg = {...};

if #arg < 3 then
    demon_print("USAGE: SETFACTION FACTION-NUMBER SHIP-ID  [ ... ]");
    return;
end

faction = arg[2];
for i = 3, #arg do
    id = tonumber(arg[i]);
    demon_print("SETTING FACTION OF " .. arg[i] .. " TO " .. faction);
    set_faction(id, faction);
end

