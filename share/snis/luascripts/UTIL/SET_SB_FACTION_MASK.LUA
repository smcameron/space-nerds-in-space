-- arg = starbase_id, faction_mask -- set the faction mask of the specified starbase
-- example: set_sb_faction_mask 25 4 -- sets faction mask of starbase with ID 25 to 4

arg = {...};

if #arg < 3 then
    demon_print("USAGE: SET_SB_FACTION_MASK STARBASE-ID FACTION-MASK ");
    return;
end

starbase = tonumber(arg[2]);
faction_mask = tonumber(arg[3]);
demon_print("SETTING FACTION MASK OF SB " .. starbase .. " TO " .. faction_mask);
set_starbase_factions_allowed(starbase, faction_mask);

