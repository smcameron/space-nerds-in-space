
arg = {...};

if #arg < 2 then
    demon_print("USAGE: TESTCOMMDITY [1, 2, 3]");
    demon_print("1. TEST LOADING CARGO");
    demon_print("2. TEST CHECKING CARGO");
    demon_print("3. TEST UNLOADING CARGO");
    return;
end

choice = tonumber(arg[2]);

player_ids = get_player_ship_ids();

-- if we already created the commodity in a previous run, don't
-- make another duplicate, just look it up.
wedding_cake = lookup_commodity("WEDDING CAKE");
if wedding_cake == nil then
	-- first run, have to make the commodity
	wedding_cake = add_commodity("FOOD", "WEDDING CAKE", "BOXES", "FOOD",
                                10.0, 02, 1.0, 0.0);
end

wedding_cargo = {
	{wedding_cake, 			10},
	{lookup_commodity("Champagne"),	100},
	{lookup_commodity("wine"),	100},
	{lookup_commodity("gold"),	1000}
}

function load_player_cargo()
	demon_print("Loading player cargo")
	for i, cargo in pairs(wedding_cargo) do
		set_commodity_contents(player_ids[1], cargo[1],	cargo[2],	i - 1);
	end
end


-- Check the resources are still on board
function check_player_cargo()
	demon_print("Checking player cargo")
	player_cargo = {0, 0, 0, 0};
	for i = 0, 7 do
		item	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.item");
		qty	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.qty");
		demon_print("item,qty = " .. item .. ", " .. qty);
		for j, cargo in pairs(wedding_cargo) do
			demon_print("Comparing " .. cargo[1] .. " to " .. item .. ".");
			if (cargo[1] == item) then
				demon_print("Adding " .. qty .. " to player cargo " .. j .. ".");
				player_cargo[j] = player_cargo[j] + qty;
			end
		end
	end

	for i, qty in pairs(player_cargo) do
		if (qty < wedding_cargo[i][2]) then
			demon_print("Player cargo " .. i .. " is NOT LOADED");
			return false;
		end
		demon_print("Player cargo " .. i .. " is LOADED");
	end
	demon_print("All player cargo is LOADED");
	return true;
end


function collect_player_cargo()
	count = 0;
	demon_print("emptying player cargo");
	for i = 0, 7 do
		item	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.item");
		qty	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.qty");
		for j, cargo in pairs(wedding_cargo) do
			if (cargo[1] == item) then
				count = count + 1;
				demon_print("Match.  Emptying cargo bay " .. player_ids[1] .. ", -1, 0, " .. i .. ".");
				set_commodity_contents(player_ids[1], -1, 0, i);
			end
		end
	end
	demon_print("Emptied " .. count .. " cargo bays");
end


if choice == 1 then
   load_player_cargo();
elseif choice == 2 then
   check_player_cargo();
elseif choice == 3 then
   collect_player_cargo();
else
   demon_print("Bad argument, only 1, 2, 3 are valid.");
end


