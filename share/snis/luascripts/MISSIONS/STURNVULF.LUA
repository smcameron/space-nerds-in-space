package.path = package.path .. ";share/snis/luascripts/MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

arg = {...};

previous_location = nil;
first_time_panel_open = true;
first_time_taking_card = true;
num_lifeforms_aboard_ship = 3;

local function examine_body(entry)
	-- entry is { "noun", object },

	local cardobj = intfic.objects["card"];
	local bodyobj = intfic.objects["body"];

	intfic.write("The body is frozen solid.\n");
	if cardobj.location == "nowhere" then 
		intfic.write("There is an ID card on the body.\n");
		cardobj.location = bodyobj.location;
		cardobj.suppress_itemizing = false;
		cardobj.related_object = { "on", "body" };
	end	
end

local function examine_quadcorder(entry)
	-- entry is { "noun", object },

	local buttonobj = intfic.objects["quadcorder_button"];

	if buttonobj.button_state == false then
		intfic.write("The quadcorder is off\n");
	else
		intfic.write("The quadcorder shows that there are\n");
		intfic.write(tostring(num_lifeforms_aboard_ship) .. " lifeforms aboard the STURNVULF\n");
	end
end

intfic.current_location = "outside_airlock";
intfic.current_location = "corridor3";
intfic.cardinal_directions = { "fore", "starboard", "aft", "port", "up", "down" };
intfic.room = {
	outside_airlock = {
		unique_name = "outside_airlock",
		shortdesc = "Ok, I am outside the airlock of the STURNVULF.",
		desc = "To aft is the the airlock, and to fore,\n" ..
			"the blackness of space\n" ..
			"There is a slot in the wall next to the airlock door",
		aft = nil,
		visited = false,
	},
	airlock = {
		unique_name = "airlock",
		shortdesc = "Ok, I am in the airlock",
		desc = "There is a button on the wall.\n" ..
			"To fore is the outer door.  To aft, the inner door\n",
		fore = nil,
		visited = false,
	},
	corridor1 = {
		unique_name = "corridor1",
		shortdesc = "Ok, I am in a corridor",
		desc = "I am in a corridor. To fore is the vestibule\n" ..
			"A staircase leads up, and another staircase\n" ..
			"leads down.",
		fore = "vestibule",
		up = "corridor3",
		down = "corridor2",
		visited = false,
	},
	corridor2 = {
		unique_name = "corridor2",
		shortdesc = "Ok, I am in a corridor",
		desc = "I am in a corridor at the bottom of a staircase.\n" ..
			"The corridor continues fore and aft.\n" ..
			"To starboard is the Engineer's quarters,\n" ..
			"and to port is the Doctor's quarters.\n",
		up = "corridor1",
		port = "doctors_quarters",
		starboard = "engineers_quarters",
		port = "doctors_quarters",
		fore = nil,
		aft = nil,
	},
	corridor3 = {
		unique_name = "corridor3",
		shortdesc = "Ok, I am in a corridor",
		desc = "I am in a corridor at the top of a staircase.\n" ..
			"The corridor continues fore and aft.\n" ..
			"To port is the captain's quarters.\n" ..
			"To starboard is the first mate's quarters.\n",
		down = "corridor1",
		fore = "bridge",
		port = "captains_quarters",
		starboard = "first_mates_quarters",
		aft = nil,
	},
	captains_quarters = {
		unique_name = "captains_quarters",
		shortdesc = "Ok, I am in the captain's quarters.\n",
		desc = "I am in the captain's quarters.\n",
		starboard = "corridor3",
	},
	first_mates_quarters = {
		unique_name = "first_mates_quarters",
		shortdesc = "Ok, I am in the first mate's quarters.\n",
		desc = "I am in the first mate's quarters.\n" ..
			"A gentle breeze comes from a large ventilation\n" ..
			"duct on the aft wall.\n",
		port = "corridor3",
	},
	doctors_quarters = {
		unique_name = "doctors_quarters",
		shortdesc = "Ok, I am in the doctor's quarters.\n",
		desc = "I am in the doctor's quarters.\n",
		starboard = "corridor2",
	},
	engineers_quarters = {
		unique_name = "engineers_quarters",
		shortdesc = "Ok, I am in the engineer's quarters.\n",
		desc = "I am in the engineer's quarters.\n",
		port = "corridor2",
	},
	bridge = {
		unique_name = "bridge",
		shortdesc = "Ok, I'm on the bridge.",
		desc = "I'm on the bridge of the STURNVULF. It's chaos here.\n" ..
			"The main viewscreen is black.  All the consoles have\n" ..
			"been scorched and melted by blasters.",
		aft = "corridor3",
	},
	vestibule = {
		unique_name = "vestibule",
		shortdesc = "Ok, I am in the vestibule",
		desc = "I am in the vestibule used to prepare for spacewalks.\n" ..
			"To fore is the airlock. To aft is a corridor.\n" ..
			"There is a slot on the wall next to the airlock door.",
		aft = "corridor1",
		visited = false,
	},
	pocket = {
		unique_name = "pocket",
		shortdesc = "pocket",
		desc = "pocket",
	},
};

intfic.objects = {
	card = {
		unique_name = "card",
		location = "nowhere",
		name = "card",
		synonyms = { "id", "keycard", "badge" },
		desc = "an ID card",
		portable = true,
		examine = "It an ID card for Ensign Mallory",
		related_object = { "on", "body" },
		suppress_itemizing = true,
	},
	slot = {
		unique_name = "slot",
		location = "outside_airlock",
		name = "slot",
		desc = "a slot in the wall",
		suppress_itemizing = true,
		portable = false,
		container = true,
		container_restrictions = { "card" },
		examine = "It looks like you could put a card into the slot.\n" ..
			"There is a small panel above the slot.",
	},
	clip = {
		unique_name = "clip",
		location = "nowhere",
		name = "clip",
		desc = "a small metal clip",
		suppress_itemizing = true,
		portable = true,
		examine = "It's a small metal clip once used to hold an ID card",
	},
	fuse_holder = {
		unique_name = "fuse_holder",
		name = "holder",
		location = "nowhere",
		desc = "a fuse holder",
		suppress_itemizing = false,
		portable = false,
		container = true,
		container_restrictions = { "fuse", "clip" },
		container_limit = 1, -- can only hold one thing at a time
	},
	fuse = {
		unique_name = "fuse",
		name = "fuse",
		location = "nowhere",
		desc = "a burned out fuse",
		suppress_itemizing = false,
		portable = true,
		related_object = { "in" , "fuse_holder" },
		examine = "The fuse is burned out.",
	},
	fuse_panel = {
		unique_name = "fuse_panel",
		name = "panel",
		desc = "small panel",
		location = "outside_airlock",
		suppress_itemizing = true, -- dynamically manipulated by after_each_turn_hook fn.
		portable = false,
                doorstatus = "closed",
                doorroom = nil,
                doordirout = nil,
                complement_door = nil; -- doors (usually) are in pairs
	},
	slot2 = {
		unique_name = "slot2",
		location = "vestibule",
		name = "slot",
		desc = "a slot in the wall",
		suppress_itemizing = true,
		portable = false,
		container = true,
		container_restrictions = { "card" },
		examine = "It looks like you could put a card into the slot\n",
	},
	body = {
		unique_name = "body",
		location = "outside_airlock",
		name = "body",
		desc = "The body of Ensign Mallory, who appears to be dead",
		portable = true,
		surface = true,
		synonyms = { "ensign", "mallory", "corpse", "carcass", "cadaver" },
		container_restrictions = { "card" },
		examine = { examine_body },
	},
	button = {
			unique_name = "button1",
			name = "button",
			desc = "a button",
			location = "airlock",
			suppress_itemizing = true,
			portable = false,
			button_state = false,
			last_button_state = false,
			button = true,
			examine = "The button is labeled CYCLE AIRLOCK\n",
	},
	airlock_door1 = {
		unique_name = "airlock_door1",
		location = "outside_airlock",
		portable = false,
		suppress_itemizing = true,
		synonyms = { "airlock" },
		name = "door",
	},
	airlock_door2 = {
		unique_name = "airlock_door1",
		location = "airlock",
		portable = false,
		suppress_itemizing = true,
		synonyms = { "airlock" },
		name = "door",
	},
	airlock_door3 = {
		unique_name = "airlock_door3",
		location = "vestibule",
		portable = false,
		suppress_itemizing = true,
		synonyms = { "airlock" },
		name = "door",
	},
	staircase1 = {
		unique_name = "staircase1",
		name = "staircase",
		location = "corridor1",
		portable = false,
		suppress_itemizing = true,
		synonyms = { "stairs" },
	},
	bench = {
		unique_name = "bench",
		name = "bench",
		synonyms = { "seat", },
		desc = "a bench",
		location = "vestibule",
		portable = false,
	},
	locker = {
		unique_name = "locker",
		name = "locker",
		desc = "a locker",
		container = true,
		location = "vestibule",
		portable = false,
	},
	spacesuit = {
		unique_name = "spacesuit",
		name = "spacesuit",
		synonyms = { "suit" },
		desc = "a spacesuit",
		container = true,
		location = "vestibule",
		portable = true,
		related_object = { "in", "locker" },
	},
	console = {
		unique_name = "console",
		name = "console",
		synonyms = { "consoles", "station", "stations" },
		desc = "a console",
		surface = true,
		location = "bridge",
		portable = false,
		examine = "The consoles are burned and melted almost beyond recognition",
		suppress_itemizing = true,
	},
	desk1 = {
		unique_name = "desk1",
		name = "desk",
		desc = "a desk",
		surface = true,
		location = "captains_quarters",
		portable = false,
	},
	bed1 = {
		unique_name = "bed1",
		name = "bed",
		desc = "a bed",
		surface = true,
		location = "captains_quarters",
		portable = false,
	},
	chair1 = {
		unique_name = "chair1",
		name = "chair",
		desc = "a chair",
		surface = true,
		location = "captains_quarters",
		portable = false,
	},
	desk2 = {
		unique_name = "desk2",
		name = "desk",
		desc = "a desk",
		surface = true,
		location = "first_mates_quarters",
		portable = false,
	},
	bed2 = {
		unique_name = "bed2",
		name = "bed",
		desc = "a bed",
		surface = true,
		location = "first_mates_quarters",
		portable = false,
	},
	chair2 = {
		unique_name = "chair2",
		name = "chair",
		desc = "a chair",
		surface = true,
		location = "first_mates_quarters",
		portable = false,
	},
	desk3 = {
		unique_name = "desk3",
		name = "desk",
		desc = "a desk",
		surface = true,
		location = "engineers_quarters",
		portable = false,
	},
	desk4 = {
		unique_name = "desk4",
		name = "desk",
		desc = "a desk",
		surface = true,
		location = "doctors_quarters",
		portable = false,
	},
	duct = {
		unique_name = "duct",
		name = "duct",
		desc = "a ventilation duct on the aft wall",
		synonyms = { "ventilation" },
		location = "first_mates_quarters",
		suppress_itemizing = true,
		portable = false,
		examine = "The ventilation duct is covered by a grate",
	},
	grate = {
		unique_name = "grate",
		name = "grate",
		desc = "a grate",
		location = "first_mates_quarters",
		suppress_itemizing = true,
		portable = false,
		examine = "The grate is screwed into the wall.",
	},
	screws = {
		unique_name = "screws",
		name = "screws",
		desc = "screws",
		synonym = "screw",
		location = "first_mates_quarters",
		suppress_itemizing = true,
		portable = false,
	},
	screwdriver = {
		unique_name = "screwdriver",
		name = "screwdriver",
		desc = "a screwdriver",
		location = "engineers_quarters",
		related_object = { "on", "desk3" },
		portable = true,
	},
	quadcorder = {
		unique_name = "quadcorder",
		name = "quadcorder",
		desc = "a quadcorder",
		location = "doctors_quarters",
		related_object = { "on", "desk4" },
		portable = true,
		examine = { examine_quadcorder },
		surface = true;
		container_restrictions = { "quadcorder_button" },
	},
	quadcorder_button = {
		unique_name = "quadcorder_button",
		name = "button",
		desc = "a button",
		location = "doctors_quarters",
		related_object = { "on", "quadcorder" },
		portable = false,
		button = true,
		button_state = false,
		last_button_state = false,
		suppress_itemizing = true,
	},
	captain_jameson = {
		unique_name = "captain_jameson",
		name = "captain",
		desc = "Captain Jameson",
		synonyms = { "jameson" },
		location = "bridge",
		related_object = {"on", "bridge_floor" },
		portable = true,
		examine = "Captain Jameson is unconscious, but alive.", 
		surface = true,
		container_restrictions = { "transporter_tag", "card" },
	},
	bridge_floor = {
		unique_name = "bridge_floor",
		name = "floor",
		desc = "floor",
		synonyms = { "ground" },
		location = "bridge",
		suppress_itemizing = true,
		portable = false,
		surface = true,
	},
	transporter_tag = {
		unique_name = "transporter_tag",
		name = "tag",
		desc = "a number of transporter tags",
		synonyms = { "transporter" },
		portable = true,
		location = "pocket",
		related_object = nil,
	},
};

function flush_output()
	out = intfic.get_output();
	if (out ~= nil) then
		msgtab = intfic.strsplit(out, "\n");
		for i,v in pairs(msgtab) do
			comms_channel_transmit("ROBOT", 1234, "-- " .. string.upper(v));
		end
	end;
	register_timer_callback("flush_output", 10, 0);
end

function robot_listen(from, channel, message)
	-- Robot should ignore its own output
	if string.sub(message, 1, 3) == "-- " then
		return;
	end;
	-- Robot should ignore its own hail message
	hailmsg = string.find(message, "*** HAILING ON CHANNEL 1234 ***");
	if hailmsg ~= nil then
		return;
	end
	intfic.send_input(message);
end

local function setup_comms()
	set_variable("SUPPRESS_STARBASE_COMPLAINTS", 1);
        comms_channel_transmit("ROBOT", 0, ": *** HAILING ON CHANNEL 1234 ***");
	comms_channel_listen("ROBOT", 1234, "robot_listen");
	register_timer_callback("flush_output", 10, 0);
end

function close_outer_airlock_door()
	intfic.room["outside_airlock"].aft = nil;
	intfic.room["airlock"].fore = nil;
end

function open_outer_airlock_door()
	intfic.room["outside_airlock"].aft = "airlock";
	intfic.room["airlock"].fore = "outside_airlock";
end

function close_inner_airlock_door()
	intfic.room["vestibule"].fore = nil;
	intfic.room["airlock"].aft = nil;
end

function open_inner_airlock_door()
	intfic.room["vestibule"].fore = "airlock";
	intfic.room["airlock"].aft = "vestibule";
end

local function lifeform_tagged(lifeform)
	-- TODO: notify SCIENCE that a lifeform is tagged for transport.
	-- TODO: add passenger to player ship
	if lifeform == "captain_jameson" then
		intfic.write("Captain Jameson dematerializes as\nhe is beamed off the STURNVULF\n");
		intfic.objects["transporter_tag"].location = "pocket";
		intfic.objects["transporter_tag"].related_object = nil;
		intfic.objects["captain_jameson"].location = "nowhere";
		num_lifeforms_aboard_ship = num_lifeforms_aboard_ship - 1;
	end
end

function intfic.after_each_turn_hook()

	-- the airlock only works if the metal clip is in the fuse holder
	local airlock_is_working = intfic.objects["clip"].related_object ~= nil and
			intfic.objects["clip"].related_object[2] == "fuse_holder";

	-- Make the ID card open the outer airlock door when put into the slot
	if intfic.objects["card"].location == "outside_airlock" and
		intfic.objects["card"].related_object ~= nil and
		intfic.objects["card"].related_object[2] == "slot" then
			-- put the card in the player's pocket
			intfic.objects["card"].location = "pocket";
			intfic.objects["card"].related_object = nil;
			if airlock_is_working then
				open_outer_airlock_door();
				intfic.write("The outer airlock door opens.\n");
			else
				intfic.write("Nothing happens.\n");
			end
			intfic.write("I retain the ID card.\n");
	end

	-- Make the ID card open the inner airlock door when put into the slot
	if intfic.objects["card"].location == "vestibule" and
		intfic.objects["card"].related_object ~= nil and
		intfic.objects["card"].related_object[2] == "slot2" then
			-- put the card in the player's pocket
			intfic.objects["card"].location = "pocket";
			intfic.objects["card"].related_object = nil;
			if airlock_is_working then
				open_inner_airlock_door();
				intfic.write("The inner airlock door opens.\n");
			else
				intfic.write("Nothing happens.\n");
			end
			intfic.write("I retain the ID card.\n");
	end

	-- Make the button a momentary switch that opens or closes the outer airlock door
	if intfic.objects["button"].button_state == true then
		intfic.objects["button"].button_state = false; -- unpress the button
		if airlock_is_working then
			if intfic.room["outside_airlock"].aft == nil then
				if intfic.room["vestibule"].fore ~= nil then
					intfic.write("The inner airlock door closes\n");
					close_inner_airlock_door();
				end
				open_outer_airlock_door();
				intfic.write("The outer airlock door opens\n");
			else
				close_outer_airlock_door();
				intfic.write("The outer airlock door closes\n");
				if intfic.room["vestibule"].fore == nil then
					intfic.write("The inner airlock door opens\n");
					open_inner_airlock_door();
				end
			end
		else
			intfic.write("Nothing happens.\n");
		end
	end

	-- If you leave the airlock, the door should close behind you
	if previous_location == "airlock" then
		if intfic.current_location == "outside_airlock" and airlock_is_working then
			close_outer_airlock_door();
			intfic.write("The outer airlock door closed behind me.\n");
		elseif intfic.current_location == "vestibule" and airlock_is_working then
			close_inner_airlock_door();
			intfic.write("The inner airlock door closed behind me.\n");
		end
	end

	-- If they pick up Ensign Mallory, draw attention to the ID card
	if intfic.objects["body"].location == "pocket" and
		intfic.objects["card"].suppress_itemizing then
		intfic.write("I notice an ID card on the body.\n");
		intfic.objects["card"].suppress_itemizing = false;
	end

	-- If they pick up the id card, draw attention to the metal clip
	if intfic.getlocation(intfic.objects["card"]) == "pocket" and
		intfic.objects["clip"].suppress_itemizing and
		(intfic.objects["card"].related_object == nil or
		intfic.objects["card"].related_object[2] ~= "body") then
		intfic.write("As I grabbed the ID card, a small metal clip fell off it.\n");
		intfic.objects["clip"].suppress_itemizing = false;
		intfic.setlocation(intfic.objects["clip"], "outside_airlock");
	end

	-- When opening/closing the panel outside the airlock, move the fuse holder in/out of scene
	if intfic.objects["fuse_panel"].doorstatus == "open" then
		-- First time opening panel, remark on the fuse situation
		if first_time_panel_open then
			intfic.write("Opening the panel reveals a fuse in a holder.\n");
			first_time_panel_open = false;
		end
		intfic.objects["fuse_panel"].suppress_itemizing = false;
		intfic.setlocation(intfic.objects["fuse_holder"], "outside_airlock");
	else
		intfic.objects["fuse_panel"].suppress_itemizing = true;
		intfic.setlocation(intfic.objects["fuse_holder"], "nowhere");
	end

	if airlock_is_working then
		intfic.objects["slot"].desc = "a slot in the wall emitting a faint blue glow.";
		intfic.objects["slot"].examine = "A faint blue glow emanates from the slot\n" ..
			"It looks like you could put a card into the slot.\n" ..
			"There is a small panel above the slot.";
	else
		intfic.objects["slot"].desc = "a slot in the wall";
		intfic.objects["slot"].examine = "It looks like you could put a card into the slot.\n" ..
			"There is a small panel above the slot.";
	end

	-- Transport any tagged lifeforms
	if intfic.objects["transporter_tag"].related_object ~= nil then
		if intfic.objects["transporter_tag"].related_object[1] == "on" then 
			if intfic.objects["transporter_tag"].related_object[2] == "captain_jameson" then
				lifeform_tagged("captain_jameson");
			end
		end
	end
	previous_location = intfic.current_location;
end

function intfic.extra_help()
	intfic.write("\n");
	intfic.write("Cardinal directions are: starboard, port, fore\n");
	intfic.write("aft, up and down.  Move by typing, e.g. go port\n");
	intfic.write("You can shorten movement commands to single\n");
	intfic.write("letters: s, p, f, a, u, d.  Additionally you can\n");
	intfic.write("use l, i, and x to mean look, inventory, and examine\n");
	intfic.write("respectively.\n");
	intfic.write("\n");
end

local function dogofore()
	intfic.send_input("go fore");
end

local function dogoaft()
	intfic.send_input("go aft");
end

local function dogoport()
	intfic.send_input("go port");
end

local function dogostarboard()
	intfic.send_input("go starboard");
end

local function dogoup()
	intfic.send_input("go up");
end

local function dogodown()
	intfic.send_input("go down");
end

local function dowear()
	intfic.write("I am robot of non-humanoid form.\nThere are no clothes that would fit me.\n");
end

local function unscrew_object(entry)
	-- entry is a table { "noun", object };
	if entry[2] == "not here" then
		intfic.write(entry[1] .. ": I do not see that here.\n");
		return;
	end;
	if entry[2] == nil then
		intfic.write(entry[1] .. ": I don't know about that.\n");
		return;
	end
	if entry[2] ~= intfic.objects["screws"] and entry[2] ~= intfic.objects["grate"] then
		intfic.write(entry[1] .. ": I'm not sure how to unscrew that.\n");
		return;
	end
	if intfic.objects["screws"].portable then
		intfic.write("The screws are already unscrewed.\n");
		return;
	end
	if intfic.getlocation(intfic.objects["screwdriver"]) ~= "pocket" then
		intfic.write(entry[1] .. ": I will need a screwdriver.\n");
			return;
	end
	intfic.setlocation(intfic.objects["screws"], "pocket");
	intfic.objects["screws"].portable = true;
	intfic.objects["grate"].suppress_itemizing = false;
	intfic.objects["grate"].portable = true;
	intfic.write(entry[1] .. ": Ok, I unscrewed the grate.\n");
	intfic.objects["duct"].suppress_itemizing = false;
	intfic.objects["duct"].desc = "an open ventilation duct on the aft wall.";
end

local function dounscrew(words)
	intfic.generic_doverb(unscrew_object, words, { intfic.current_location, "pocket" });
end

function setup_custom_words()
	intfic.verb["p"] = { dogoport };
	intfic.verb["s"] = { dogostarboard };
	intfic.verb["f"] = { dogofore };
	intfic.verb["a"] = { dogoaft };
	intfic.verb["u"] = { dogoup };
	intfic.verb["d"] = { dogodown };
	intfic.verb["wear"] = { dowear };
	intfic.verb["unscrew"] = { dounscrew };
end

setup_custom_words();

function setup_solarsystem()
	clear_all();
	set_variable("MISSILE_FIRE_CHANCE", "0");
	planet = {};
	planet[1] = add_planet("BACHU", 61667.011707, 65406.588868, 478750.311434, 6326.000000, 0);
	starbase = {};
	starbase[1] = add_starbase(65524.003685, 77976.702190, 475855.906157, 0);
	planet[2] = add_planet("GUL", 240767.046176, 10709.084948, 63069.823786, 4276.000000, 0);
	starbase[2] = add_starbase(241301.405915, 18314.471849, 65224.093602, 1);
	planet[3] = add_planet("OSPOP", 479256.530329, 4072.975742, 479257.937371, 5833.000000, 0);
	starbase[3] = add_starbase(477643.686755, 12897.278002, 477648.249863, 2);
	planet[4] = add_planet("DESP", 62862.999767, 128560.772598, 181430.733288, 5958.000000, 0);
	starbase[4] = add_starbase(66924.897985, 140351.718730, 182545.952211, 3);
	ship = {};
	ship[1] = add_ship("TIME", 76938.739472, 2319.095262, 471156.345369, 13, 1, 1);
	ship[2] = add_ship("OSTO", 73828.485758, 245.239262, 455766.471069, 8, 0, 1);
	ship[3] = add_ship("ASUT", 66929.720709, 140514.542616, 182477.983239, 14, 2, 1);
	ship[4] = add_ship("EFASH", 34697.540990, 3039.694629, 458289.054158, 4, 2, 1);
	ship[5] = add_ship("JENEU", 58925.430359, 134484.727613, 184861.311878, 11, 1, 1);
	ship[6] = add_ship("MASPEA", 480851.888548, 13884.838285, 479044.064837, 20, 2, 1);
	ship[7] = add_ship("STOLI", 70870.586894, -25.984958, 502169.870114, 7, 4, 1);
	ship[8] = add_ship("GEL", 485678.789508, 424.577878, 481058.300584, 16, 2, 1);
	ship[9] = add_ship("AVI", 443672.783283, 16945.838011, 478221.210944, 6, 0, 1);
	ship[10] = add_ship("STASU", 58975.946286, 71628.736928, 479243.183026, 1, 4, 1);
	ship[11] = add_ship("FADUG", 109174.690710, 11.549888, 448348.870866, 8, 1, 1);
	ship[12] = add_ship("CHOTHEA", 110970.731128, 204.187889, 441694.701218, 2, 4, 1);
	nebula = {};
	nebula[1] = add_nebula("", 48465.902008, 0.000000, 317620.051428, 3595.000000);
	ship[13] = add_ship("USHU", 454545.958580, 1134.258561, 460351.259852, 9, 0, 1);
	ship[14] = add_ship("THOJU", 64272.730562, 121166.663121, 194106.555855, 17, 4, 1);
	ship[15] = add_ship("BOT", 84989.072014, 67374.404108, 484119.067287, 11, 0, 1);
	ship[16] = add_ship("ZENAA", 455287.410298, 1218.313225, 498946.277809, 13, 2, 1);
	ship[17] = add_ship("UBIM", 69906.621658, 80236.015355, 474751.516369, 1, 2, 1);
	ship[18] = add_ship("JOSTO", 89238.469201, 120148.025325, 203194.485450, 2, 2, 1);
	ship[19] = add_ship("ORO", 65899.235393, 126082.079596, 188154.783933, 17, 4, 1);
	ship[20] = add_ship("AKA", 240929.159697, 19134.296715, 70202.663361, 15, 2, 1);
	ship[21] = add_ship("TON", 57116.709029, 65491.587537, 485671.172669, 0, 1, 1);
	ship[22] = add_ship("SPUPHU", 35347.807268, 6013.823595, 170808.833487, 4, 4, 1);
	ship[23] = add_ship("FURAA", 238180.287309, 14872.506300, 69391.085157, 12, 3, 1);
	ship[24] = add_ship("ADIST", 56546.723355, -41.024147, 210696.588457, 2, 2, 1);
	ship[25] = add_ship("CHITHATH", 67906.865001, 64724.465117, 463007.629657, 14, 2, 1);
	ship[26] = add_ship("NIMIE", 251506.079522, 119.109099, 45757.248745, 3, 3, 1);
	ship[27] = add_ship("SILENT SOCHE", 241402.500734, 18818.120455, 65089.640833, 6, 0, 1);
	ship[28] = add_ship("BULOB", 476667.107824, 10577.073772, 476251.827471, 10, 1, 1);
	ship[29] = add_ship("INAT", 57157.286326, 123631.816616, 183215.725203, 12, 3, 1);
	ship[30] = add_ship("NELESP", 256970.240355, 3936.408034, 68992.433398, 16, 0, 1);
	asteroid = {};
	asteroid[1] = add_asteroid(144589.162425, 0.000000, 327373.456506);
	set_asteroid_speed(asteroid[1], 1.000000);
	asteroid[2] = add_asteroid(146804.882431, 0.000000, 317055.886180);
	set_asteroid_speed(asteroid[2], 1.000000);
	asteroid[3] = add_asteroid(140639.150981, 0.000000, 312604.686890);
	set_asteroid_speed(asteroid[3], 1.000000);
	asteroid[4] = add_asteroid(136099.691463, 0.000000, 317148.537309);
	set_asteroid_speed(asteroid[4], 1.000000);
	asteroid[5] = add_asteroid(138896.040789, 0.000000, 329488.591268);
	set_asteroid_speed(asteroid[5], 1.000000);
	asteroid[6] = add_asteroid(142571.075976, 0.000000, 331995.857774);
	set_asteroid_speed(asteroid[6], 1.000000);
	asteroid[7] = add_asteroid(129576.031867, 0.000000, 340258.882880);
	set_asteroid_speed(asteroid[7], 1.000000);
	asteroid[8] = add_asteroid(118286.447566, 0.000000, 336593.572882);
	set_asteroid_speed(asteroid[8], 1.000000);
	asteroid[9] = add_asteroid(120010.729074, 0.000000, 325036.618728);
	set_asteroid_speed(asteroid[9], 1.000000);
	asteroid[10] = add_asteroid(126210.311638, 0.000000, 324966.284668);
	set_asteroid_speed(asteroid[10], 1.000000);
	asteroid[11] = add_asteroid(128230.095848, 0.000000, 323771.888706);
	set_asteroid_speed(asteroid[11], 1.000000);
	asteroid[12] = add_asteroid(517379.555217, 0.000000, 337007.933418);
	set_asteroid_speed(asteroid[12], 1.000000);
	asteroid[13] = add_asteroid(509234.278812, 0.000000, 330469.450783);
	set_asteroid_speed(asteroid[13], 1.000000);
	asteroid[14] = add_asteroid(502774.415480, 0.000000, 329265.759414);
	set_asteroid_speed(asteroid[14], 1.000000);
	asteroid[15] = add_asteroid(503825.180134, 0.000000, 341931.459053);
	set_asteroid_speed(asteroid[15], 1.000000);
	asteroid[16] = add_asteroid(503585.561766, 0.000000, 347116.553700);
	set_asteroid_speed(asteroid[16], 1.000000);
	asteroid[17] = add_asteroid(499882.800637, 0.000000, 342620.965570);
	set_asteroid_speed(asteroid[17], 1.000000);
	asteroid[18] = add_asteroid(491882.832521, 0.000000, 331123.486769);
	set_asteroid_speed(asteroid[18], 1.000000);
	asteroid[19] = add_asteroid(499640.190082, 0.000000, 327204.718171);
	set_asteroid_speed(asteroid[19], 1.000000);
	asteroid[20] = add_asteroid(503532.407057, 0.000000, 325369.359933);
	set_asteroid_speed(asteroid[20], 1.000000);
	asteroid[21] = add_asteroid(510564.591715, 0.000000, 321526.446721);
	set_asteroid_speed(asteroid[21], 1.000000);
	asteroid[22] = add_asteroid(509047.660440, 0.000000, 337344.312827);
	set_asteroid_speed(asteroid[22], 1.000000);
	asteroid[23] = add_asteroid(507559.769859, 0.000000, 345094.459064);
	set_asteroid_speed(asteroid[23], 1.000000);
	asteroid[24] = add_asteroid(506903.048141, 0.000000, 344931.092771);
	set_asteroid_speed(asteroid[24], 1.000000);
	asteroid[25] = add_asteroid(253791.876836, 0.000000, 484839.165762);
	set_asteroid_speed(asteroid[25], 1.000000);
	asteroid[26] = add_asteroid(263669.210419, 0.000000, 489046.697663);
	set_asteroid_speed(asteroid[26], 1.000000);
	asteroid[27] = add_asteroid(257610.795406, 0.000000, 489487.915526);
	set_asteroid_speed(asteroid[27], 1.000000);
	spacemonster = {};
	spacemonster[1] = add_spacemonster("M. MYSTERIUM", 507228.306394, -8246.417376, 328124.460679);
	nebula[2] = add_nebula("", 69585.274937, 0.000000, 300340.578100, 3080.000000);
	spacemonster[2] = add_spacemonster("M. MYSTERIUM", 254528.951265, 9817.508487, 481300.977881);
	spacemonster[3] = add_spacemonster("M. MYSTERIUM", 505180.143081, -9492.437995, 328542.504220);
	asteroid[28] = add_asteroid(250550.687603, 0.000000, 484708.098047);
	set_asteroid_speed(asteroid[28], 1.000000);
	derelict = {};
	derelict[1] = add_derelict("XII", 246566.351322, 20139.833471, 64167.836048, 10, 3);
	spacemonster[4] = add_spacemonster("M. MYSTERIUM", 257263.566660, 11119.829349, 480949.289835);
	nebula[3] = add_nebula("", 39506.168309, 0.000000, 301620.539418, 1363.000000);
	asteroid[29] = add_asteroid(262941.277573, 0.000000, 494743.096644);
	set_asteroid_speed(asteroid[29], 1.000000);
	cargo_container = {};
	cargo_container[1] = add_cargo_container(247371.156986, 19345.582006, 63816.013416, 9.159448, 1.359991, -12.703562);
	ship[31] = add_ship("ESPLORATORE GLOBE", 231172.860393, 692.198850, 140661.548819, 3, 1, 1);
	nebula[4] = add_nebula("", 43346.053382, 0.000000, 299700.598558, 3228.000000);
	asteroid[30] = add_asteroid(255742.626491, 0.000000, 496489.954199);
	set_asteroid_speed(asteroid[30], 1.000000);
	asteroid[31] = add_asteroid(248799.764496, 0.000000, 502112.645048);
	set_asteroid_speed(asteroid[31], 1.000000);
	asteroid[32] = add_asteroid(241725.908810, 0.000000, 494774.391436);
	set_asteroid_speed(asteroid[32], 1.000000);
	asteroid[33] = add_asteroid(245024.333980, 0.000000, 483383.192260);
	set_asteroid_speed(asteroid[33], 1.000000);
	asteroid[34] = add_asteroid(258403.781028, 0.000000, 469523.072838);
	set_asteroid_speed(asteroid[34], 1.000000);
	derelict[2] = add_derelict("NUSE", 240329.526069, 4361.679071, 67303.152010, 22, 3);
	asteroid[35] = add_asteroid(264766.907381, 0.000000, 464500.139831);
	set_asteroid_speed(asteroid[35], 1.000000);
	asteroid[36] = add_asteroid(262242.416649, 0.000000, 482515.299258);
	set_asteroid_speed(asteroid[36], 1.000000);
	cargo_container[2] = add_cargo_container(240040.080268, 4722.030634, 67335.528719, -13.050931, -2.015906, 7.549839);
	asteroid[37] = add_asteroid(270023.400647, 0.000000, 496777.257059);
	set_asteroid_speed(asteroid[37], 1.000000);
	asteroid[38] = add_asteroid(271861.112597, 0.000000, 498135.431300);
	set_asteroid_speed(asteroid[38], 1.000000);
	spacemonster[5] = add_spacemonster("M. MYSTERIUM", 132271.386258, 5507.558810, 323115.599520);
	cargo_container[3] = add_cargo_container(239883.107612, 4822.357782, 67340.274813, -13.923001, -1.458533, 7.576207);
	spacemonster[6] = add_spacemonster("M. MYSTERIUM", 132468.676894, 8278.155057, 322460.100621);
	black_hole = {};
	black_hole[1] = add_black_hole("VIPH", 473413.278755, 0.000000, 143545.364655, 1073.000000);
	ship[32] = add_ship("GALASSIA ZIRIPH", 233402.840185, -110.376835, 143982.919465, 2, 3, 1);
	nebula[5] = add_nebula("", 75985.084882, 0.000000, 310580.266412, 1739.000000);
	nebula[6] = add_nebula("", 94544.532939, 0.000000, 322099.914924, 1909.000000);
	nebula[7] = add_nebula("", 56145.673271, 0.000000, 314420.149249, 3493.000000);
	nebula[8] = add_nebula("", 57425.635707, 0.000000, 324019.855784, 2899.000000);
	nebula[9] = add_nebula("", 45905.978254, 0.000000, 329779.679482, 2836.000000);
	nebula[10] = add_nebula("", 38226.205873, 0.000000, 326579.777303, 2581.000000);
	nebula[11] = add_nebula("", 35666.282119, 0.000000, 308020.343775, 2893.000000);
	nebula[12] = add_nebula("", 61265.521898, 0.000000, 297140.675922, 1475.000000);
	nebula[13] = add_nebula("", 80464.951731, 0.000000, 320179.972947, 5162.000000);
	nebula[14] = add_nebula("", 81104.932391, 0.000000, 327859.738622, 1398.000000);
	nebula[15] = add_nebula("", 91344.628526, 0.000000, 304180.460937, 2907.000000);
	nebula[16] = add_nebula("", 90704.646749, 0.000000, 300980.558759, 1994.000000);
	nebula[17] = add_nebula("", 76625.065541, 0.000000, 305460.422256, 3246.000000);
	nebula[18] = add_nebula("", 63825.445652, 0.000000, 318900.011628, 3432.000000);
	player_ids = get_player_ship_ids();
	-- set player 1 location
	if (player_ids[1]) then
		dock_player_to_starbase(player_ids[1], starbase[1]);
	end
end

if set_variable == nil then
	intfic.gameloop(); -- standalone mode i/o via terminal
else
	setup_solarsystem();
	setup_comms(); -- input/output via SNIS comms
end;

