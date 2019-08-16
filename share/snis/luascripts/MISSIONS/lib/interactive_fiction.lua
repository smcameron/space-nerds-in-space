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
intfic.after_each_turn_hook = nil;
intfic.extra_help = nil;

local function print_value(v, indent)
	local toprint = "";
	if (type(v) == "number") then
		toprint = toprint .. v .. ",\r\n"
	elseif (type(v) == "string") then
		toprint = toprint .. "\"" .. v .. "\",\r\n"
	end
	return toprint;
end

function intfic.format_table(tbl, indent)
	if not indent then
		indent = 0
	end
	if tbl == nil then
		return "nil"
	end
	local toprint = string.rep(" ", indent) .. "{\r\n"
	indent = indent + 2
	if type(tbl) ~= "table" then
		toprint = toprint .. print_value(tbl) .. string.rep(" ", indent - 2) .. "\n";
		return toprint
	end
	for k, v in pairs(tbl) do
		toprint = toprint .. string.rep(" ", indent)
		if (type(k) == "number") then
			toprint = toprint .. "[" .. k .. "] = "
		elseif (type(k) == "string") then
			toprint = toprint  .. k ..  "= "
		end
		if (type(v) == "number") then
			toprint = toprint .. v .. ",\r\n"
		elseif (type(v) == "string") then
			toprint = toprint .. "\"" .. v .. "\",\r\n"
		elseif (type(v) == "table") then
			toprint = toprint .. intfic.format_table(v, indent + 2) .. ",\r\n"
		else
			toprint = toprint .. "\"" .. tostring(v) .. "\",\r\n"
		end
	end
	toprint = toprint .. string.rep(" ", indent-2) .. "}"
	return toprint
end
local fmttbl = intfic.format_table;

function intfic.dprnt(words)
	for k, v in pairs(intfic.cdr(words)) do
		local var = _G[v];
		if var == nil then
			intfic.write(v .. ": unknown identifer\n");
		else
			intfic.write(v .. " = " .. fmttbl(var));
		end
	end
end

function intfic.strsplit(inputstr, sep)

	if inputstr == nil then
		return nil;
	end
	if sep == nil then
		sep = "%s";
	end
	local new_array = {};
	for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
		table.insert(new_array, str);
	end
	return new_array;
end

-- split_wordlist, given words ("a", "b", "c", "d", ... } and separatorwords {"b"}
-- returns:
-- { direct_objs = { "a", },
--		 indirect_objs = { "c", "d" },
--		 prep = "b"
-- }
function intfic.split_wordlist(words, separatorwords)
	local found = false
	local firstlist = {};
	local secondlist = {};
	local prep = "";
	count = 1;
	for k, v in pairs(words) do
		if intfic.in_array(v, separatorwords) then
			found = true;
			count = 1;
			prep = v;
		else
			if found then
				secondlist[count] =  v;
				count = count + 1;
			else
				firstlist[count] = v;
				count = count + 1;
			end
		end
	end
	local answer = {};
	answer.direct_objs = firstlist;
	answer.prep = prep;
	answer.indirect_objs = secondlist;
	return answer;
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
	local new_table = {}
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
	local n = #intfic.output;
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
	local forbidden = false;
	if intfic.room[destination].forbidden_objects ~= nil then
		for k, v in pairs(intfic.room[destination].forbidden_objects) do
			if intfic.getlocation(intfic.objects[v]) == "pocket" then
				intfic.write(v .. ": it won't fit in here.\n");
				forbidden = true;
			end
		end
	end
	if not forbidden then
		intfic.current_location = destination;
	end
end

function intfic.dogo(words)
	intfic.map(intfic.go_direction, intfic.cdr(words));
end

function intfic.dolook()
	intfic.room[intfic.current_location].visited = false;
	intfic.print_room_description(intfic.current_location, intfic.objects);
end

function intfic.getlocation(o)
	if o == nil then
		return nil;
	end
	return o.location;
end

-- return true if o is "on" or "in" possible_container (recursively)
function is_contained_in(o, possible_container)
	if o == possible_container then
		return true;
	end
	if o.related_object == nil then
		return false;
	end
	if o.related_object[2] == possible_container.unique_name then
		return true;
	end
	container = intfic.objects[o.related_object[2]];
	if not container.surface and not container.container then
		return false;
	end
	return is_contained_in(container, possible_container);
end

function intfic.setlocation(o, location)
	o.location = location;
	if (o.surface ~= nil and o.surface) or
		(o.container ~= nil and o.container) then
		intfic.set_related_object_locations(o);
	end
end

function intfic.set_related_object_locations(o)
	for k, v in pairs(intfic.objects) do
		if v.related_object ~= nil then
			if v.related_object[2] == o.unique_name then
				intfic.setlocation(v, intfic.getlocation(o));
			end
		end
	end
end

function intfic.doinventory()
	local count = 0;
	intfic.write("I am carrying:\n");
	for i, v in pairs(intfic.objects) do
		if intfic.getlocation(v) == "pocket" then
			if v.related_object ~= nil then
				intfic.write("   " .. v.desc .. " (" .. v.related_object[1] .. " the " ..
						intfic.objects[v.related_object[2]].name .. ")\n");
			else
				intfic.write("  " .. v.desc .. "\n");
			end
			count = count + 1;
		end
	end
	if (count == 0) then
		intfic.write("  Nothing.\n");
	end
end

-- return list of objects that are currently in the given list of locations
function intfic.all_in_locations(locations)
	local stuff = {};
	n = 1;
	for i, v in pairs(intfic.objects) do
		if intfic.in_array(intfic.getlocation(v), locations) and not v.suppress_itemizing then
			stuff[n] = intfic.objects[i].name;
			n = n + 1;
		end
	end
	return stuff;
end

-- nouns is a table of { { { word, object } .. }, { { word, object }, .. } }
-- rooms is a list of room names { "pocket", "somewhere" },
function intfic.disambiguate_noun_in_room(nouns, rooms)
	local answer = {};
	local count = 1;
	for k, v in pairs(nouns) do
		if v ~= nil then
			if v[2] ~= nil then
				if intfic.in_array(intfic.getlocation(v[2]), rooms) then
					answer[count] = v;
					count = count + 1;
				else
					answer[count] = { v[1], "not here" };
					count = count + 1;
				end
			else
				answer[count] = { v[1], nil };
				count = count + 1;
			end
		end
	end
	return answer;
end

-- We can end up with duplicate objects, e.g. maybe we have two coins, one of which is
-- "here", and one which is not.  If we "take coin", we should take the one that is here
-- and not complain that the one that isn't here, isn't here. This function removes
-- duplicate entries that would cause such complaints.
local function remove_duplicate_unfound_nouns(nouns)
	local not_here = {};
	local here = {};
	count = 1;
	for k, v in pairs(nouns) do
		for k2, v2 in pairs(v) do
			if v2[2] == "not here" then
				if not_here[v2[1]] == nil then
					not_here[v2[1]] = 1;
				else
					not_here[v2[1]] = not_here[v2[1]] + 1;
				end
			else
				if here[v2[1]] == nil then
					here[v2[1]] = 1;
				else
					here[v2[1]] = here[v2[1]] + 1;
				end
			end
		end
	end

	local answer = {};
	count = 1;

	for k, v in pairs(nouns) do
		for k2, v2 in pairs(v) do
			if here[v2[1]] == nil and v2[2] == "not here" and not_here[v2[1]] ~= nil then
				answer[count] = v2;
				count = count + 1;
				here[v2[1]] = nil;
				not_here[v2[1]] = nil;
			elseif here[v2[1]] ~= nil and v2[2] ~= "not here" then
				answer[count] = v2;
				count = count + 1;
				here[v2[1]] = nil;
				not_here[v2[1]] = nil;
			end
		end
	end
	return { answer };
end

function intfic.disambiguate_nouns_in_room(nouns, rooms)
	local answer = {};
	local count = 1;
	for k, v in pairs(nouns) do
		answer[count] = intfic.disambiguate_noun_in_room(v, rooms);
		count = count + 1;
	end
	answer = remove_duplicate_unfound_nouns(answer);
	return answer;
end

-- this is for handling the word "all".
-- lookup_nouns will translate "all" to
-- { "all", nil }.  We want to replace
-- that with the appropriate list of objects,
-- The "appropriate" list is context sensitive.
-- which for "drop", will be the list of objs
-- that the player is carrying. for "take", will
-- be the list of objects lying around.  For
-- "examine", will be union of above two lists.
-- Hence the fixupfunc, providing the context
-- correct fixup function.

function intfic.fixup_all(objlist, locations)
	local foundall = false;
	local fixedup = {};
	for i, v in pairs(objlist) do
		if intfic.in_array(v, intfic.everything) then
			foundall = true;
		end
	end
	if foundall then
		return intfic.merge_tables(objlist, intfic.all_in_locations(locations));
	end
	return objlist;
end

function intfic.lookup_noun(word)
	local found = false;
	local answer = { };
	for k, v in pairs(intfic.objects) do
		if v.name == word then
			table.insert(answer, { word, v });
			found = true;
		elseif v.synonyms ~= nil then
			if intfic.in_array(word, v.synonyms) then
				table.insert(answer, { word, v });
				found = true;
			end
		end
	end
	if not found then
		table.insert(answer, { word, nil });
	end
	return answer;
end

function intfic.lookup_nouns_all_in_locations(words, locations)
	local wordlist = intfic.fixup_all(words, locations);
	return intfic.map(intfic.lookup_noun, wordlist);
end

function intfic.take_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write(entry[1] .. ": I do not see that here.\n");
		return;
	end;
	if entry[2] == nil then
		intfic.write(entry[1] .. ": I don't know about that.\n");
		return;
	end
	local loc = intfic.getlocation(entry[2]);
	if loc == "pocket" then
		if entry[2].related_object == nil then
			intfic.write(entry[1] .. ": I already have that.\n");
			return;
		end;
	end
	if not loc == intfic.current_location and not loc == "pocket" then
		intfic.write(entry[1] .. ": I don't see that here.\n");
		return;
	end
	if not entry[2].portable then
		intfic.write(entry[1] .. ": I can't seem to take it.\n");
		return;
	end
	intfic.setlocation(entry[2], "pocket");
	entry[2].related_object = nil;
	intfic.write(entry[1] .. ": Taken.\n");
end

function intfic.drop_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write(entry[1] .. ": I don't see that around.\n");
		return;
	end
	if entry[2] == nil then
		intfic.write(entry[1] .. ": I do not know what that is.\n");
		return;
	end
	if not intfic.getlocation(entry[2]) == "pocket" then
		intfic.write(entry[1] .. ": I don't have that.\n");
		return;
	end
	intfic.setlocation(entry[2], intfic.current_location);
	intfic.write(entry[1] .. ": Dropped.\n");
end

function intfic.examine_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write("I don't see the " .. entry[1] .. " here.\n");
		return;
	end
	if entry[2] == nil then
		intfic.write("I don't know what the " .. entry[1] .. " is.\n");
		return;
	end
	if intfic.getlocation(entry[2]) ~= "pocket" and intfic.getlocation(entry[2]) ~= intfic.current_location then
		intfic.write("I don't see any " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].doorstatus ~= nil then
		intfic.write("The " .. entry[2].desc .. " is " .. entry[2].doorstatus .. ".\n");
	end
	if entry[2].examine == nil then
		if entry[2].button then
			if entry[2].button_state then
				intfic.write("The " .. entry[2].name .. " is activated.\n");
			else
				intfic.write("The " .. entry[2].name .. " is deactivated.\n");
			end
		else
			intfic.write("I don't see anything special about the " .. entry[1] .. ".\n");
		end
		return;
	end
	if type(entry[2].examine) == "string" then
		intfic.write(entry[1] .. ": " .. entry[2].examine .. "\n");
	else
		entry[2].examine[1](entry[2]);
	end
end;

function intfic.open_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write("I don't see the " .. entry[1] .. " here.\n");
		return;
	end
	if entry[2] == nil then
		intfic.write("I don't know what the " .. entry[1] .. " is.\n");
		return;
	end
	if intfic.getlocation(entry[2]) ~= "pocket" and intfic.getlocation(entry[2]) ~= intfic.current_location then
		intfic.write("I don't see any " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].doorstatus == nil then
		intfic.write("I don't see how one could open the " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].doorstatus == "open" then
		intfic.write("The " .. entry[1] .. " is already open.\n");
		return;
	end
	-- check if the door only allows opening from one side (this is a bit hacky)
	if entry[2].no_open_from_this_side ~= nil then
		intfic.write("The " .. entry[1] .. " is locked from the other side.\n");
		return;
	end
	-- connect the rooms and open the two halves of the door
	local door1 = entry[2];
	local door2 = nil;
	if door1.complement_door ~= nil then
		door2 = intfic.objects[door1.complement_door];
	end
	local loc1 = intfic.getlocation(door1);
	local loc2 = nil;
	if door2 ~= nil then
		loc2 = intfic.getlocation(door2);
	end
	if door1 ~= nil and door1.doordirout ~= nil and door1.doorroom ~= nil then
		intfic.room[loc1][door1.doordirout] = door1.doorroom;
	end
	if door2 ~= nil and door2.doordirout ~= nil and door2.doorroom then
		intfic.room[loc2][door2.doordirout] = door2.doorroom;
	end
	if door1 ~= nil then
		door1.doorstatus = "open";
		intfic.write("Ok, I opened the " .. door1.desc .. ".\n");
	end
	if door2 ~= nil then
		door2.doorstatus = "open";
	end
	return;
end

function intfic.close_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write("I don't see the " .. entry[1] .. " here.\n");
		return;
	end
	if entry[2] == nil then
		intfic.write("I don't know what the " .. entry[1] .. " is.\n");
		return;
	end
	if intfic.getlocation(entry[2]) ~= "pocket" and intfic.getlocation(entry[2]) ~= intfic.current_location then
		intfic.write("I don't see any " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].doorstatus == nil then
		intfic.write("I don't see how one could close the " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].doorstatus == "closed" then
		intfic.write("The " .. entry[1] .. " is already closed.\n");
		return;
	end
	-- close both halves of the door and disconnect the rooms
	local door1 = entry[2];
	local door2 = intfic.objects[door1.complement_door];
	local loc1 = intfic.getlocation(door1);
	local loc2 = nil;
	if door2 ~= nil then
		loc2 = intfic.getlocation(door2);
	end
	if door1.doordirout ~= nil then
		intfic.room[loc1][door1.doordirout] = nil;
	end
	if loc2 ~= nil and door2 ~= nil and door2.doordirout ~= nil then
		intfic.room[loc2][door2.doordirout] = nil;
	end
	door1.doorstatus = "closed";
	if door2 ~= nil then
		door2.doorstatus = "closed";
	end
	intfic.write("Ok, I closed the " .. entry[2].desc .. ".\n");
	return;
end

function intfic.push_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write("I don't see the " .. entry[1] .. " here.\n");
		return;
	end
	if entry[2] == nil then
		intfic.write("I don't know what the " .. entry[1] .. " is.\n");
		return;
	end
	if intfic.getlocation(entry[2]) ~= "pocket" and intfic.getlocation(entry[2]) ~= intfic.current_location then
		intfic.write("I don't see any " .. entry[1] .. ".\n");
		return;
	end
	if entry[2].button == nil or not entry[2].button then
		intfic.write("Pushing the " .. entry[2].name .. " does not seem to do anything.\n");
		return;
	end
	entry[2].button_state = not entry[2].button_state;
	if entry[2].button_fn ~= nil then
		entry[2].button_fn(entry[2]);
	end
end

function intfic.generic_doverb(verb_fn, words, locations)
	local direct_objs = intfic.lookup_nouns_all_in_locations(intfic.cdr(words), locations);
	direct_objs = intfic.disambiguate_nouns_in_room(direct_objs, locations);
	if intfic.table_empty(direct_objs) then
		intfic.write("Uh, say again?.\n");
		return;
	end
	for k, v in ipairs(direct_objs) do
		intfic.map(verb_fn, v);
	end
end

function intfic.dotake(words)
	intfic.generic_doverb(intfic.take_object, words, { intfic.current_location, "pocket" });
end

function intfic.dodrop(words)
	intfic.generic_doverb(intfic.drop_object, words, { "pocket" });
end

function intfic.doexamine(words)
	intfic.generic_doverb(intfic.examine_object, words, { "pocket", intfic.current_location });
end

function intfic.doopen(words)
	intfic.generic_doverb(intfic.open_object, words, { "pocket", intfic.current_location });
end

function intfic.count_items_in_container(container, preposition)
	local count = 0;
	for k, v in pairs(intfic.objects) do
		if v.related_object ~= nil and v.related_object[1] == preposition and
			v.related_object[2] == container.unique_name then
			count = count + 1;
		end
	end
	return count;
end

function intfic.container_full(container, preposition)
	local limit = 100;
	if preposition == "on" then
		limit = container.surface_limit
		if limit == nil then
			return false;
		end
	elseif preposition == "in" then
		limit = container.container_limit
		if limit == nil then
			return false;
		end
	end
	return intfic.count_items_in_container(container, preposition) >= limit;
end

-- find the indirect object of put or output why it cannot be found
-- That is, given a list of (hopefully one) candidate objects, compare it to the state of
-- the game and return the object if it is reachable and suitable for putting or else
-- explain why it is not reachable/suitable and return  nil.
local function find_indirect_object_of_put(disam_indirect_objs, preposition)
	local count = 0;
	local indirect_object = {};
	for k, v in pairs(disam_indirect_objs) do
		for k2, v2 in pairs(v) do
			if intfic.getlocation(v2[2]) ~= "pocket" and
				intfic.getlocation(v2[2]) ~= intfic.current_location then
				intfic.write("I do not see the " .. v2[1] .. " here.\n");
				return nil;
			end
			if v2[2] == "not here" then
				intfic.write("I do not see the " .. v2[1] .. " here.\n");
				return nil;
			end
			count = count + 1;
			if preposition == "on" then
				if not v2[2].surface then
					intfic.write("I don't see a way to put anything " ..
						preposition .. " the " .. v2[1] .. ".\n");
					return nil;
				end
			elseif preposition == "in" then
				if not v2[2].container then
					intfic.write("I don't see a way to put anything " ..
						preposition .. " the " .. v2[1] .. ".\n");
					return nil;
				end
			else
				intfic.write("I don't see a way to put anything " ..
					preposition .. " the " .. v2[1] .. ".\n");
				return nil;
			end
			indirect_object = v2[2];
		end
	end

	if count ~= 1 then
		intfic.write("I'm not sure what you mean by that\n");
		return nil;
	end
	return indirect_object;
end

-- put "obj" in or on "indirect_object" or output the reason why not
-- obj is the object to put
-- preposition is "in" or "on"
-- indirect_object is the object in/on which obj is to be put
local function do_put_obj_on_indirect_obj_helper(obj, preposition, indirect_object)
	if intfic.getlocation(obj) ~= "pocket" and intfic.getlocation(obj) ~= intfic.current_location then
		intfic.write(obj.name .. ": I don't see that here.\n");
		return;
	end
	if obj == indirect_object then -- can't put something on itself.
		intfic.write("I can't put the " .. obj.name .. " " .. preposition .. " itself\n");
		return;
	end
	if not obj.portable then
		intfic.write("I can't seem to move the " .. obj.name .. "\n");
		return;
	end
	if is_contained_in(indirect_object, obj) then
		intfic.write(obj.name ..  ": Uh, we will need a higher dimensional universe first.\n");
		return;
	end;
	local object_ok = true;
	if indirect_object.container_restrictions ~= nil then
		object_ok = intfic.in_array(obj.unique_name,
			indirect_object.container_restrictions);
	end
	if not object_ok then
		intfic.write("I can't put the " .. obj.name .. " " .. preposition ..
				" the " .. indirect_object.name .. "\n");
		return;
	end
	if intfic.container_full(indirect_object, preposition) then
		intfic.write("The " .. indirect_object.name .. " is too full\n");
		return;
	end
	obj.related_object = { preposition, indirect_object.unique_name };
	intfic.setlocation(obj, indirect_object.location);
	intfic.write("Ok, I put the " .. obj.name .. " " .. preposition ..
		" the " .. indirect_object.name .. "\n");
end

-- put direct objects in/on indirect object
-- direct_objs is a table of tables, each inner table contains an object name
-- in [1], and the object itself in [2].
-- Best to print("direct_objs = " .. fmttbl(direct_objcs)); to see it.
-- indirect_object is the object in/on which direct objects should be put
local function do_put_action_on_direct_objs(direct_objs, preposition, indirect_object)
	for k, v in pairs(direct_objs) do
		for k2, v2 in pairs(v) do
			if v2[2] ~= nil then
				do_put_obj_on_indirect_obj_helper(v2[2], preposition, indirect_object)
			else
				intfic.write(v2[1] .. ": I don't know about that.\n");
			end
		end
	end
end

-- The common part of "put" verb among "put x on y" and "put x in y"
-- putparams is a table like:
-- { direct_objs = { "noun1", "noun2", "noun3", ... },
--   prep = "on", -- or "in"
--   indirect_objs =  { "noun1", "noun2", ... }, -- Normally, this will contain ONE noun. 
-- }
local function doput_common(putparams)
	local direct_objs = intfic.lookup_nouns_all_in_locations(putparams.direct_objs, { intfic.current_location, "pocket" });
	local indirect_objs = intfic.lookup_nouns_all_in_locations(putparams.indirect_objs, { intfic.current_location, "pocket" });
	local disam_direct_objs = intfic.disambiguate_nouns_in_room(direct_objs, { intfic.current_location, "pocket"});
	local disam_indirect_objs = intfic.disambiguate_nouns_in_room(indirect_objs, { intfic.current_location, "pocket"});

	local indirect_object = find_indirect_object_of_put(disam_indirect_objs, putparams.prep);
	if indirect_object == nil then
		return;
	end
	do_put_action_on_direct_objs(direct_objs, putparams.prep, indirect_object);
end

function intfic.doput(words)

	local putparams = intfic.split_wordlist(intfic.cdr(words), { "on", "in" });
	local count = 0;

	if not intfic.table_empty(intfic.cdr(putparams.indirect_objs)) then
		-- there should be only one indirect object.
		intfic.write("I don't understand quite what you mean.\n");
		return;
	end
	if putparams.prep == "on" then
		if intfic.table_empty(putparams.direct_objs) then
			-- This is actually "wear", e.g. "put on coat".
			intfic.not_implemented({ "put on (wear)"});
			return;
		end
		doput_common(putparams);
	elseif putparams.prep == "in" then
		if intfic.table_empty(putparams.direct_objs) then
			-- put in blah
			intfic.write("I do not quite understand what you mean.\n");
			return;
		end
		doput_common(putparams);
	else
		intfic.write("I do not understand what you mean.\n");
	end
end

function intfic.dopush(words)
	intfic.generic_doverb(intfic.push_object, words, { "pocket", intfic.current_location });
end

function intfic.dohelp(words)
	intfic.write("I know the following verbs:\n");
	intfic.write("go, take, get, drop, look, examine\n");
	intfic.write("open, close, inventory, listen, put\n");
	intfic.write("press, push, flip, help\n");
	if intfic.extra_help ~= nil then
		intfic.extra_help();
	end
end

function intfic.doclose(words)
	intfic.generic_doverb(intfic.close_object, words, { "pocket", intfic.current_location });
end

function intfic.not_implemented(w)
	intfic.write(w[1] .. " is not yet implemented.\n");
end

function intfic.ignore_some_words(words)
	answer = {};
	count = 1;
	for k, v in ipairs(words) do
		if not intfic.in_array(v, intfic.ignore_words) then
			answer[count] = v;
			count = count + 1;
		end
	end
	return answer;
end

function intfic.execute_command(cmd)
	if cmd == nil then
		return
	end
	if cmd == "" then
		return
	end
	local words = intfic.strsplit(cmd, " ,.;");
	if words == nil
	then
		return;
	end
	if words[1] == "dprnt" then
		words = intfic.strsplit(cmd, " ");
	end
	if intfic.verb[words[1]] == nil then
		intfic.write("I don't understand what you mean by '" .. words[1] .. "'\n");
		return;
	end
	intfic.verb[words[1]][1](intfic.ignore_some_words(words));
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
			if intfic.getlocation(v) == loc then
				if v.suppress_itemizing == nil or not v.suppress_itemizing then
					if not foundone then
						intfic.write("I see:\n");
						foundone = true;
					end
					if v.related_object == nil then
						intfic.write("   " .. v.desc .. "\n");
					else
						intfic.write("   " .. v.desc .. " (" .. v.related_object[1] .. " the " ..
								intfic.objects[v.related_object[2]].name .. ")\n");
					end
				end
			end
		end
		intfic.write("Obvious exits:\n");
		local dircount = 0;
		for k, v in pairs(intfic.cardinal_directions) do
			if intfic.room[loc][v] ~= nil then
				intfic.write("  " .. v .. "\n");
				dircount = dircount + 1;
			end
		end
		if dircount == 0 then
			intfic.write("  None\n");
		end
	end
	intfic.room[loc].visited = true;
end

intfic.ignore_words = { "a", "an", "the", "please" };

intfic.verb = {
		go = { intfic.dogo },
		take = { intfic.dotake },
		get = { intfic.dotake },
		drop = { intfic.dodrop },
		look = { intfic.dolook },
		l = { intfic.dolook },
		examine = { intfic.doexamine },
		open = { intfic.doopen },
		close = { intfic.doclose },
		x = { intfic.doexamine },
		inventory = { intfic.doinventory },
		i = { intfic.doinventory },
		listen = { intfic.dolisten },
		put = { intfic.doput },
		press = { intfic.dopush },
		push = { intfic.dopush },
		flip = { intfic.dopush },
		throw = { intfic.dopush },
		help = { intfic.dohelp },
		quit = { intfic.do_exit },
		dprnt = { intfic.dprnt },
};

intfic.room = {
	-- maintenance_room = {
			-- unique_name = "maintenance_room"; -- must equal index into intfic.room
			-- shortdesc = "Maintenance Room",
			-- desc = "You are in the maintenance room.  The floor is covered\n" ..
				-- "in some kind of grungy substance.  A control panel is on the\n" ..
				-- "wall.  A door leads out into a corridor to the south\n",
			-- south = "corridor",
			-- visited = false,
		-- },
	-- corridor = {
			-- unique_name = "corridor",
			-- shortdesc = "Corridor",
   			-- desc = "You are in a corridor.  To the east is the bridge.  To the\n" ..
				-- "west is the hold.  A doorway on the north side of the corridor\n" ..
				   -- "leads into the maintenance room.\n",
			-- north = "maintenance_room",
			-- visited = false,
		-- },
	-- pocket = {
			-- unique_name = "pocket",
			-- shortdesc = "pocket",
			-- desc = "pocket",
			-- visited = false,
		-- },
	nowhere = {
		unique_name = "nowhere",
		shortdesc = "nowhere",
		desc = "nowhere",
		visited = false,
	},
};

intfic.objects = {
	-- knife = { location = "pocket", name = "knife", desc = "a small pocket knife", portable=true },
	-- mop = { location = "maintenance_room", name = "mop", desc = "a mop", portable=true },
	-- bucket = { location = "maintenance_room", name = "bucket", desc = "a bucket", portable=true, container = true },
	-- table = { location = "maintenance_room", name = "bucket", desc = "a bucket", portable=true, surface = true },
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
	intfic.execute_command(string.lower(command));
	intfic.print_room_description(intfic.current_location, intfic.objects)
	if intfic.after_each_turn_hook ~= nil then
		intfic.after_each_turn_hook();
	end
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

