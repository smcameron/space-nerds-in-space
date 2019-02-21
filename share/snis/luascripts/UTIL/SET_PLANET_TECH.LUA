-- set the specified planet's tech level 

arg = {...};

if #arg < 3 then
    demon_print("USAGE: SET_PLANET_TECH PLANET_ID TECHVALUE");
    return;
end

id = tonumber(arg[2]);
if id == nil then
    demon_print("SET_PLANET_DESCRIPTION: BAD PLANET ID");
    return;
end

tech = tonumber(arg[3]);
if tech == nil then
    demon_print("SET_PLANET_TECH: BAD TECH VALUE");
    return;
end

set_planet_tech_level(id, tech);

demon_print("SET TECH LEVEL OF " .. arg[2] .. " TO " .. arg[3]);

