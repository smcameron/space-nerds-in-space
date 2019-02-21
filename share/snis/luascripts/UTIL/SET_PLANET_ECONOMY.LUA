-- set the specified planet's economy 

arg = {...};

if #arg < 3 then
    demon_print("USAGE: SET_PLANET_ECONOMY PLANET_ID ECONOMY-VALUE");
    return;
end

id = tonumber(arg[2]);
if id == nil then
    demon_print("SET_PLANET_DESCRIPTION: BAD PLANET ID");
    return;
end

econ = tonumber(arg[3]);
if econ == nil then
    demon_print("SET_PLANET_ECONOMY: BAD ECONOMY VALUE");
    return;
end

set_planet_economy(id, econ);

demon_print("SET ECONOMY OF " .. arg[2] .. " TO " .. arg[3]);

