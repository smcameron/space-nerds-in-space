-- arg = ship id -- destroy the specified ship

arg = {...};

if #arg < 2 then
	demon_print("USAGE: DESTROY_SHIP SHIP-ID  [ ... ]");
	return;
end

for i = 2, #arg do
	id = tonumber(arg[i]);
	demon_print("DESTROYING SHIP ID " .. arg[i]);
	x = destroy_ship(id);
	if x < 0 then
		demon_print("FAILED TO DESTROY ID " .. arg[i] .. " MAYBE IT WAS NOT A SHIP.");
	end
end

