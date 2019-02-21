-- set the specified planet's security 

arg = {...};

if #arg < 3 then
    demon_print("USAGE: SET_PLANET_SECURITY PLANET_ID SECURITYVALUE");
    return;
end

id = tonumber(arg[2]);
if id == nil then
    demon_print("SET_PLANET_DESCRIPTION: BAD PLANET ID");
    return;
end

sec = tonumber(arg[3]);
if sec == nil then
    demon_print("SET_PLANET_SECURITY: BAD SECURITY VALUE");
    return;
end

set_planet_security(id, sec);

demon_print("SET SECURITY OF " .. arg[2] .. " TO " .. arg[3]);

