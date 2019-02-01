-- set the specified planet's description

arg = {...};

if #arg < 2 then
    demon_print("USAGE: SET_PLANET_DESCRIPTION PLANET_ID DESCRIPTION");
    return;
end

id = tonumber(arg[2]);
if id == nil then
    demon_print("SET_PLANET_DESCRIPTION: BAD PLANET ID");
    return;
end

description = arg[3];
for i = 4, #arg do
	description = description .. " " .. arg[i];
end

set_planet_description(id, description);

demon_print("SET DESCRIPTION OF " .. arg[2] .. " TO " .. description);

