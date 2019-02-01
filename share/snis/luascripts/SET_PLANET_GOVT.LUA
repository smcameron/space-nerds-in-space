-- set the specified planet's government 

arg = {...};

if #arg < 3 then
    demon_print("USAGE: SET_PLANET_GOVT PLANET_ID GOVTVALUE");
    return;
end

id = tonumber(arg[2]);
if id == nil then
    demon_print("SET_PLANET_DESCRIPTION: BAD PLANET ID");
    return;
end

govt = tonumber(arg[3]);
if govt == nil then
    demon_print("SET_PLANET_GOVT: BAD GOVT VALUE");
    return;
end

set_planet_government(id, govt);

demon_print("SET GOVT OF " .. arg[2] .. " TO " .. arg[3]);

