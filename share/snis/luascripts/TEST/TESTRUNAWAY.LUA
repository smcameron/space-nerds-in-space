
-- Deliberately do an infinite loop to test runaway detection

demon_print("TESTING RUNAWAY LUA SCRIPT\n");

i = 0;
while true do
	i = i + 1;
	if (i % 100000) == 0 then
		demon_print("i = " .. tostring(i) .. "\n");
	end
end

