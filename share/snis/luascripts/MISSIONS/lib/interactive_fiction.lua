-- Copyright (C) 2019 Stephen M. Cameron
-- Author: Stephen M. Cameron
--
-- This file is part of Space Nerds In Space.
--
-- Space Nerds In Space is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- Space Nerds In Space is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with Space Nerds In Space; if not, write to the Free Software
-- Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA


-- This module provides the (very) basics for writing interactive ficiton in Lua.

intfic = {};

intfic.time_to_quit = false;
intfic.current_location = "nowhere";
intfic.last_location = "";

function intfic.strsplit(inputstr, sep)
	if sep == nil then
		sep = "%s";
	end
	local new_array = {};
	for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
		table.insert(new_array, str);
	end
	return new_array;
end

function intfic.map(func, array)
	local new_array = {};
	for i, v in ipairs(array) do
		new_array[i] = func(v);
	end
	return new_array;
end

function intfic.cdr(array)
	local new_array = {}
	for i = 2, #array do
		table.insert(new_array, array[i])
	end
	return new_array;
end

function intfic.table_empty(t)
	return next(t) == nil;
end

function intfic.merge_tables(a, b)
	for k, v in pairs(b) do
		a[k] = v;
	end;
	return a;
end

function intfic.append_tables(a, b)
	new_table = {}
	i = 1;
	for k, v in pairs(a) do
		new_table[i] = v;
		i = i + 1;
	end
	for k, v in pairs(b) do
		new_table[i] = v;
		i = i + 1;
	end
	return new_table;
end

function intfic.in_array(word, array)
	for i, v in ipairs(array) do
		if v == word then
			return true
		end
	end
	return false
end

function intfic.write(str)
	n = #intfic.output;
	intfic.output[n + 1] = str;
end

function intfic.go_direction(direction)
	if intfic.room[intfic.current_location][direction] == nil then
		intfic.write(direction .. ": I can't go that way\n");
		return;
	end
	if not intfic.in_array(direction, intfic.cardinal_directions) then
		intfic.write(direction .. ": I can't go that way\n");
		return;
	end
	destination = intfic.room[intfic.current_location][direction];
	if intfic.room[destination] == nil then
		intfic.write(direction .. ": I do not understand\n");
		return;
	end
	intfic.current_location = destination;
end

function intfic.dogo(words)
	intfic.map(intfic.go_direction, intfic.cdr(words));
end

function intfic.dolook()
	intfic.room[intfic.current_location].visited = false;
	intfic.print_room_description(intfic.current_location, intfic.objects);
end

function intfic.doinventory()
	count = 0;
	intfic.write("I am carrying:\n");
	for i, v in pairs(intfic.objects) do
		if v.location == "pocket" then
			intfic.write("  " .. v.desc .. "\n");
			count = count + 1;
		end
	end
	if (count == 0) then
		intfic.write("  Nothing.\n");
	end
end

function intfic.all_in_location(loc)
	stuff = {};
	n = 1;
	for i, v in pairs(intfic.objects) do
		if v.location == loc then
			stuff[n] = i;
			n = n + 1;
		end
	end
	return stuff;
end

function intfic.all_in_room()
	return intfic.all_in_location(intfic.current_location);
end

function intfic.all_holding()
	return intfic.all_in_location("pocket");
end

function intfic.all_holding_or_here()
	return intfic.append_tables(intfic.all_in_room(), intfic.all_holding());
end

-- this is for handling the word "all".
-- lookup_nouns will translate "all" to
-- [ "all", None ].  We want to replace
-- that with the appropriate list of objects,
-- The "appropriate" list is context sensitive.
-- which for "drop", will be the list of objs
-- that the player is carrying. for "take", will
-- be the list of objects lying around.  For
-- "examine", will be union of above two lists.
-- Hence the fixupfunc, providing the context
-- correct fixup function.

function intfic.fixup_all(objlist, fixupfunc)
	foundall = false;
	fixedup = {};
	for i, v in pairs(objlist) do
		if intfic.in_array(v, intfic.everything) then
			foundall = true;
		end
	end
	if foundall then
		return intfic.merge_tables(objlist, fixupfunc());
	end
	return objlist;
end

function intfic.fixup_all_in_room(objlist)
	return intfic.fixup_all(objlist, intfic.all_in_room);
end

function intfic.fixup_all_holding(objlist)
	return intfic.fixup_all(objlist, intfic.all_holding);
end

function intfic.fixup_all_holding_or_here(objlist)
	return intfic.fixup_all(objlist, intfic.all_holding_or_here);
end

function intfic.lookup_noun(word)
	if intfic.objects[word] ~= nil then
		return { word, intfic.objects[word] };
	end
	return { word, nil };
end

function intfic.lookup_nouns(words)
	return intfic.map(intfic.lookup_noun, words);
end

function intfic.lookup_nouns_fixup(words, fixupfunc)
	wordlist = fixupfunc(words);
	return intfic.lookup_nouns(wordlist);
end

function lookup_nouns_all_in_room(words)
	return intfic.lookup_nouns_fixup(words, intfic.fixup_all_in_room);
end

function lookup_nouns_all_holding(words)
	return intfic.lookup_nouns_fixup(words, intfic.fixup_all_holding);
end

function lookup_nouns_all_holding_or_here(words)
	return intfic.lookup_nouns_fixup(words, intfic.fixup_all_holding_or_here);
end

function intfic.take_object(entry)
	-- entry is a table { "noun", object };

	if entry[2] == nil then
		intfic.write(entry[1] .. ": I don't know about that.\n");
		return;
	end
	if entry[2].location == "pocket" then
		intfic.write(entry[1] .. ": I already have that.\n");
		return;
	end
	if not entry[2].location == intfic.current_location then
		intfic.write(entry[1] .. ": I don't see that here.\n");
		return;
	end
	if not entry[2].portable then
		intfic.write(entry[1] .. ": I can't seem to take it.\n");
		return;
	end
	entry[2].location = "pocket";
	intfic.write(entry[1] .. ": Taken.\n");
end

function intfic.drop_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == nil then
		intfic.write(entry[1] .. ": I do not know what that is.\n");
		return;
	end
	if not entry[2].location == "pocket" then
		intfic.write(entry[1] .. ": I don't have that.\n");
		return;
	end
	entry[2].location = intfic.current_location;
	intfic.write(entry[1] .. ": Dropped.\n");
end

function intfic.examine_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == nil then
		intfic.write("I don't know what the " .. entry[i] .. " is.\n");
		return;
	end
	if entry[2].location ~= "pocket" and entry[2].location ~= intfic.current_location then
		intfic.write("I don't see any " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].examine == nil then
		intfic.write("I don't see anything special about the " .. entry[1] .. ".\n");
		return;
	end
	intfic.write(entry[1] .. ": " .. entry[2].examine .. "\n");
end;

function intfic.dotake(words)
	totake = lookup_nouns_all_in_room(intfic.cdr(words));
	if intfic.table_empty(totake) then
		intfic.write("Uh, say again?.\n");
		return;
	end
	intfic.map(intfic.take_object, totake);
end

function intfic.dodrop(words)
	todrop = lookup_nouns_all_holding(intfic.cdr(words));
	if intfic.table_empty(todrop) then
		intfic.write("Uh, say again?\n");
		return;
	end
	intfic.map(intfic.drop_object, todrop);
end

function intfic.doexamine(words)
	tox = lookup_nouns_all_holding_or_here(intfic.cdr(words));
	if intfic.table_empty(tox) then
		intfic.write("Uh, say again?\n");
		return;
	end
	intfic.map(intfic.examine_object, tox);
end

function intfic.not_implemented(w)
	intfic.write(w[1], " is not yet implemented.\n");
end

function intfic.execute_command(cmd)
	if cmd == "" then
		return
	end
	words = intfic.strsplit(cmd, " ,.;");
	if intfic.verb[words[1]] == nil then
		intfic.write("I don't understand what you mean by '" .. words[1] .. "'\n");
		return;
	end
	intfic.verb[words[1]][1](words);
end

function intfic.dolisten()
	intfic.write("Well, I can hear the humming sound of you know, all the space\nmachinery and stuff. Nothing else really.\n");
end

function intfic.do_exit()
	intfic.time_to_quit = true;
end

function intfic.print_room_description(loc, obj)
	local foundone = false
	if intfic.last_location ~= intfic.current_location then
		intfic.write(intfic.room[loc].shortdesc .. "\n\n");
		intfic.last_location = intfic.current_location;
	end
	if not intfic.room[loc].visited then
		intfic.write(intfic.room[loc].desc .. "\n\n");
		for k, v in pairs(obj) do
			if v.location == loc then
				if not foundone then
					intfic.write("I see:\n");
					foundone = true;
				end
				intfic.write("   " .. v.desc .. "\n");
			end
		end
	end
	intfic.room[loc].visited = true;
end

intfic.verb = {
		go = { intfic.dogo },
		take = { intfic.dotake },
		get = { intfic.dotake },
		drop = { intfic.dodrop },
		look = { intfic.dolook },
		examine = { intfic.doexamine },
		x = { intfic.doexamine },
		inventory = { intfic.doinventory },
		i = { intfic.doinventory },
		listen = { intfic.dolisten },
		quit = { intfic.do_exit },
};

intfic.room = {
	-- maintenance_room = {
			-- shortdesc = "Maintenance Room",
			-- desc = "You are in the maintenance room.  The floor is covered\n" ..
				-- "in some kind of grungy substance.  A control panel is on the\n" ..
				-- "wall.  A door leads out into a corridor to the south\n",
			-- south = "corridor",
			-- visited = false,
		-- },
	-- corridor = {
			-- shortdesc = "Corridor",
   			-- desc = "You are in a corridor.  To the east is the bridge.  To the\n" ..
				-- "west is the hold.  A doorway on the north side of the corridor\n" ..
				   -- "leads into the maintenance room.\n",
			-- north = "maintenance_room",
			-- visited = false,
		-- },
	-- pocket = {
			-- shortdesc = "pocket",
			-- desc = "pocket",
			-- visited = false,
		-- },
	nowhere = {
		shortdesc = "nowhere",
		desc = "nowhere",
		visited = false,
	},
};

intfic.objects = {
	-- knife = { location = "pocket", name = "knife", desc = "a small pocket knife", portable=true },
	-- mop = { location = "maintenance_room", name = "mop", desc = "a mop", portable=true },
	-- bucket = { location = "maintenance_room", name = "bucket", desc = "a bucket", portable=true },
	-- panel = { location = "maintenance_room", name = "panel", desc = "a control panel", portable=false },
	-- substance = { location = "maintenance_room", name = "substance", desc = "some kind of grungy substance", portable=false,
	--			examine = "the grungy substance looks extremely unpleasant.", },
	nothing = { location = "nowhere", name = "nothing", desc = "nothing", portable = false };
};

intfic.cardinal_directions = { "north", "northeast", "east", "southeast", "south", "southwest", "west", "northwest", "up", "down" };
intfic.everything = { "all", "everything", };
intfic.output = {};

function intfic.get_output()
	local tmp = table.concat(intfic.output);
	intfic.output = {};
	return tmp;
end

function intfic.send_input(command)
	intfic.execute_command(command);
	intfic.print_room_description(intfic.current_location, intfic.objects)
end

-- This is the only function in here that uses io.write or io.read.
-- Normally, you would not call this, but instead implement this in your
-- mission script, and use comms functions instead of io.write and io.read.
-- That is, hook player's comms input to intfic.send_input, and hook
-- intfic.get_output() to comms output.
function intfic.gameloop()
	while not intfic.time_to_quit do
		local out = intfic.get_output();
		if out ~= nil then
			io.write(out);
		end
		intfic.write("> ");
		out = intfic.get_output();
		if out ~= nil then
			io.write(out);
		end
		command = io.read("*line");
		intfic.send_input(command);
	end
end

