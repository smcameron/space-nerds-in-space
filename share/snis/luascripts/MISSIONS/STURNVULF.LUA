package.path = package.path .. ";share/snis/luascripts/MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

adjx = -300000;
adjz = -300000;

arg = {...};

previous_location = nil;
first_time_panel_open = true;
first_time_taking_card = true;
num_lifeforms_aboard_ship = 5;
zarkon = nil;
intfic.current_location = "outside_airlock";
intfic.cardinal_directions = { "fore", "starboard", "aft", "port", "up", "down" };
intfic.tag_just_placed = false;
special_cargo_container = {};
intfic.cargo_container_asteroid = 1;
intfic.ccasteroidx = 0;
intfic.ccasteroidy = 0;
intfic.ccasteroidz = 0;
intfic.ccasteroidname = "Unknown";
intfic.bomb_disarmed = false;

local function dbgout(s)
	if demon_print ~= nil then
		demon_print(s);
	else
		intfic.write(s);
	end
end

if random_point_on_sphere ~= nil then
	-- Choose a location for the STURNVULF
	svx, svy, svz = random_point_on_sphere(200000);
	math.randomseed(os.time());
	robot_channel = math.floor(math.random() * 950.0 + 50.0);
else
	-- Choose a location for the STURNVULF
	svx = 354000 + adjx;
	svy = 232000;
	svz = 10000 + adjz;
	-- Choose a random channel for the ROBOT COMMS
	robot_channel = 1234;
end

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
		intfic.write("The quadcorder is off.  There is a button on the quadcorder.\n");
	else
		intfic.write("The quadcorder shows that there are\n");
		intfic.write(tostring(num_lifeforms_aboard_ship) .. " lifeforms aboard the STURNVULF\n");
	end
end

local function examine_captains_log(entry)
	local x = math.floor(intfic.ccasteroidx / 1000.0) * 1000;
	local y = math.floor(intfic.ccasteroidy / 1000.0) * 1000;
	local z = math.floor(intfic.ccasteroidz / 1000.0) * 1000;

	intfic.write("The last entry reads:\n" ..
		"The zarkons have boarded our ship and installed a bomb\n" ..
		"in engineering. So far we are unable to defuse it.\n" ..
		"I think they may have also drugged the ventilation system\n" ..
		"as many of the crew have inexplicably fallen unconcious.\n" ..
		"I think they mean to leave us for dead and blow up the ship.\n" ..
		"Before the Zarkons boarded, we were able to hide the weapons\n" ..
		"components near the asteroid " .. intfic.ccasteroidname .. ".\n" ..
		"The coordinates are approximately:\n" ..
		"  X = " .. tostring(x) .. "\n" ..
		"  Y = " .. tostring(y) .. "\n" ..
		"  Z = " .. tostring(z) .. "\n" ..
		"Although there were reports of space monsters in the area,\n" ..
		"we did not encounter any.\n");
end

local function quadcorder_button_fn(quad)
	if quad.button_state then
		intfic.write("The quadcorder turns on.\n");
		examine_quadcorder(nil); -- dubious to pass a nil here.
	else
		intfic.write("The quadcorder turns off.\n");
	end
end

local function weapons_active()
	if intfic.objects["resistor"].related_object ~= nil and
		intfic.objects["resistor"].related_object[2] == "circuit_board" and
		intfic.objects["circuit_board"].related_object ~= nil and
		intfic.objects["circuit_board"].related_object[2] == "eng_console_slot" and
		intfic.objects["eng_console_switch"].button_state == true then
		return true;
	end
	return false;
end

local function eng_console_switch_fn(swtch)
	local switch_ready = intfic.objects["resistor"].related_object ~= nil and
		intfic.objects["resistor"].related_object[2] == "circuit_board" and
		intfic.objects["circuit_board"].related_object ~= nil and
		intfic.objects["circuit_board"].related_object[2] == "eng_console_slot";
	if swtch.button_state and switch_ready then
		intfic.write("The lights on the engineering console begin blinking.\n");
	else
		if switch_ready then
			intfic.write("The lights on the engineering console go dark.\n");
		else
			intfic.write("Nothing happens.\n");
		end
	end
end


local function examine_eng_console(entry)
	if intfic.objects["resistor"].related_object ~= nil and
		intfic.objects["resistor"].related_object[2] == "circuit_board" and
		intfic.objects["circuit_board"].related_object ~= nil and
		intfic.objects["circuit_board"].related_object[2] == "eng_console_slot" then
		intfic.write("There are many blinking lights on the console\n");
	else
		intfic.write("All of the lights on the console are dark\n");
	end
	intfic.write("There is a small panel on the back of the console\n");
	intfic.write("There is a large switch marked WEAPONS on the console\n");
	if weapons_active() then
		intfic.write("and next to it is a flashing label: WEAPONS ARMED\n");
	end
end

local function examine_eng_lights(entry)
	if intfic.objects["resistor"].related_object ~= nil and
		intfic.objects["resistor"].related_object[2] == "circuit_board" and
		intfic.objects["circuit_board"].related_object ~= nil and
		intfic.objects["circuit_board"].related_object[2] == "eng_console_slot" then
		intfic.write("The lights are blinking\n");
	else
		intfic.write("All of the lights on the console are dark\n");
	end
end

local function examine_magazine(entry)

	intfic.write("The torpedo magazine is very large and heavy.\n");
	intfic.write("There are " .. tostring(intfic.objects["torpedo_magazine"].torpedo_count) ..
			" torpedoes in the magazine.\n");
end

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
			"leads down. To aft is a bulkhead with a door\n" ..
			"marked ENGINEERING.",
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
		fore = "torpedo_bay",
		aft = "transporter_room",
	},
	transporter_room = {
		unique_name = "transporter_room",
		shortdesc = "Ok, I am in the transporter room.",
		desc = "I am in the transporter room.  There is a console\n" ..
			"here on one end of the room and the transporter chamber\n" ..
			"is to port.",
		fore = "corridor2",
		port = "transporter",
	},
	transporter = {
		unique_name = "transporter",
		shortdesc = "Ok, I'm in the transporter chamber.",
		desc = "I'm in the transporter.  Something is wrong here.\n" ..
			"The walls of the transporter are burned and black and\n" ..
			"there is black goop all over the walls and floor.\n",
		starboard = "transporter_room",
	},
	torpedo_bay = {
		unique_name = "torpedo_bay",
		shortdesc = "Ok, I am in the torpedo bay",
		desc = "I am in the torpedo bay.  There is a large console here.\n" ..
			"There is a large robot arm for loading the torpedoes and\n" ..
			"a large drum magazine containing torpedoes\n",
		aft = "corridor2",
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
	duct1 = {
		unique_name = "duct1",
		shortdesc = "Ok, I am in the ventilation duct",
		desc = "To fore is the first mate's quarters\n" ..
			"To aft is more duct\n",
		aft = "duct2",
		fore = "first_mates_quarters",
		visited = false,
		forbidden_objects = { "bomb" },
	},
	duct2 = {
		unique_name = "duct2",
		shortdesc = "Ok, I am in the ventilation duct",
		desc = "To fore is more duct, and to aft is more duct\n" ..
			"and the duct branches off to starboard as well.",
		fore = "duct1",
		aft = "duct3",
		starboard = "duct4",
		visited = false,
	},
	duct3 = {
		unique_name = "duct3",
		shortdesc = "Ok, I am in the ventilation duct",
		desc = "To fore is more duct, further progress aft is\n" ..
			"blocked by a large fan.",
		fore = "duct2",
		visited = false,
	},
	duct4 = {
		unique_name = "duct4",
		shortdesc = "Ok, I am in the ventilation duct",
		desc = "To starboard the duct opens into a room.\n" ..
			"To port is more duct.\n",
		port = "duct2",
		starboard = "engineering",
		visited = false,
		forbidden_objects = { "bomb" },
	},
	engineering = {
		unique_name = "engineering",
		shortdesc = "I am in engineering",
		desc = "There is a large engineering console here.\n" ..
			"To port is an open ventilation duct\n" ..
			"To fore is a bulkhead with a door\n",
		port = "duct4",
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
	captains_log = {
		unique_name = "captains_log",
		name = "log",
		synonyms = { "captains", "book", "logbook", },
		location = "captains_quarters",
		portable = true,
		desc = "the captain's log",
		examine = { examine_captains_log },
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
		synonyms = { "vent" },
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
		synonyms = { "quad", "corder" },
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
		button_fn = quadcorder_button_fn;
	},
	captain_jameson = {
		unique_name = "captain_jameson",
		name = "captain",
		desc = "CAPTAIN JAMESON",
		synonyms = { "jameson" },
		location = "bridge",
		related_object = {"on", "bridge_floor" },
		portable = true,
		examine = "Captain Jameson is unconscious, but alive.",
		surface = true,
		container_restrictions = { "transporter_tag", "card" },
		passenger_number = 0;
	},
	commander_klon = {
		unique_name = "commander_klon",
		name = "commander",
		desc = "COMMANDER KLON",
		synonyms = { "klon" },
		location = "transporter_room",
		related_object = {"on", "transporter_room_floor" },
		portable = true,
		examine = "Commander Klon is unconscious, but alive.",
		surface = true,
		container_restrictions = { "transporter_tag", "card" },
		passenger_number = 2;
	},
	doctor_nork = {
		unique_name = "doctor_nork",
		name = "doctor",
		desc = "DOCTOR NORK",
		synonyms = { "nork" },
		location = "torpedo_bay",
		related_object = {"on", "torpedo_bay_floor" },
		portable = true,
		examine = "Doctor Nork is unconscious, but alive.",
		surface = true,
		container_restrictions = { "transporter_tag", "card" },
		passenger_number = 3;
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
		desc = "a transporter tag",
		synonyms = { "transporter" },
		portable = true,
		location = "pocket",
		related_object = nil,
	},
	eng_door1 = {
		unique_name = "eng_door1",
		name = "door",
		desc = "door to engineering",
		synonyms = { "engineering" },
		portable = false,
		location = "corridor1",
		examine = "The door appears to be locked from the other side",
		suppress_itemizing = true,
		no_open_from_this_side = true,
                doorstatus = "closed",
                doorroom = "engineering",
                doordirout = "aft",
                complement_door = "eng_door2",
	},
	eng_door2 = {
		unique_name = "eng_door2",
		name = "door",
		desc = "door",
		portable = false,
		location = "engineering",
		examine = "The door is a sturdy metal door set in the bulkhead",
		suppress_itemizing = true,
                doorstatus = "closed",
                doorroom = "corridor1",
                doordirout = "fore",
                complement_door = "eng_door1",
	},
	eng_bulkhead = {
		unique_name = "eng_bulkhead",
		name = "bulkhead",
		desc = "a bulkhead",
		location = "corridor1",
		portable = false,
		suppress_itemizing = true,
	},
	eng_bulkhead2 = {
		unique_name = "eng_bulkhead2",
		name = "bulkhead",
		desc = "a bulkhead",
		location = "engineering",
		portable = false,
		suppress_itemizing = true,
	},
	large_fan = {
		unique_name = "large_fan",
		name = "fan",
		desc = "a large, slowly spinning fan.",
		location = "duct3",
		portable = false,
	},
	engineer_bob = {
		unique_name = "engineer_bob",
		name = "engineer",
		desc = "ENGINEER ROBERT BOBBERSON",
		synonyms = { "engineer", "robert", "bob", "bobberson" },
		location = "engineering",
		portable = true,
		examine = "Engineer Bobberson is unconscious, but alive.",
		surface = true,
		container_restrictions = { "transporter_tag", "card" },
		related_object = { "on", "eng_floor" },
		passenger_number = 1;
	},
	eng_floor = {
		unique_name = "eng_floor",
		name = "floor",
		desc = "floor",
		synonyms = { "ground" },
		location = "engineering",
		suppress_itemizing = true,
		portable = false,
		surface = true,
	},
	transporter_room_floor = {
		unique_name = "transporter_room_floor",
		name = "floor",
		desc = "floor",
		synonyms = { "ground" },
		location = "transporter_room",
		suppress_itemizing = true,
		portable = false,
		surface = true,
	},
	dolly = {
		unique_name = "dolly",
		name = "dolly",
		desc = "a dolly",
		synonyms = { "hand", "truck" },
		location = "transporter_room",
		portable = true,
		surface = true,
		examine = "The dolly is just a normal hand truck.",
	},
	torpedo_bay_floor = {
		unique_name = "torpedo_bay_floor",
		name = "floor",
		desc = "floor",
		synonyms = { "ground" },
		location = "torpedo_bay",
		suppress_itemizing = true,
		portable = false,
		surface = true,
	},
	circuit_board = {
		unique_name = "circuit_board",
		name = "circuit",
		synonyms = { "board", "pcb", "badge", },
		desc = "a small circuit board",
		location = "engineering",
		portable = true,
		related_object = { "on", "eng_floor" },
		examine = "There are burn marks where there should be a resistor",
		surface = true,
		container_restrictions = { "resistor" },
	},
	resistor = {
		unique_name = "resistor",
		name = "resistor",
		synonyms = { "resister" },
		desc = "a 1M ohm resistor",
		location = "engineers_quarters",
		related_object = { "on", "desk3" },
		portable = true,
		examine = "The resistor looks alright.",
	},
	soldering_iron = {
		unique_name = "soldering_iron",
		name = "iron",
		synonyms = { "soldering" },
		desc = "a soldering iron",
		location = "engineers_quarters",
		portable = true,
		related_object = { "on", "desk3" },
	},
	eng_console = {
		unique_name = "eng_console",
		name = "console",
		desc = "the engineering console",
		location = "engineering",
		portable = false,
		surface = true,
		examine = { examine_eng_console },
	},
	eng_console_panel = {
		unique_name = "eng_console_panel",
		name = "panel",
		desc = "a small panel on the back of the console",
		portable = false,
		location = "engineering",
		related_object = { "on", "eng_console" },
		suppress_itemizing = true,
                doorstatus = "closed",
                doorroom = nil,
                doordirout = nil,
                complement_door = nil; -- doors (usually) are in pairs
	},
	eng_console_slot = {
		unique_name = "eng_console_slot",
		name = "slot",
		desc = "a circuit board slot",
		location = "nowhere",
		related_object = nil,
		container = true,
		container_restrictions = { "circuit_board", },
		portable = false,
	},
	eng_console_switch = {
		unique_name = "eng_console_switch",
		name = "switch",
		desc = "a large switch",
		location = "engineering",
		related_object = { "on", "eng_console" },
		button = true,
		button_state = false,
		portable = false,
		suppress_itemizing = true,
		button_fn = eng_console_switch_fn,
	},
	eng_console_lights = {
		unique_name = "eng_console_lights",
		name = "lights",
		synonyms = { "light" },
		desc = "some lights",
		location = "engineering",
		related_object = { "on", "eng_console" },
		portable = false,
		suppress_itemizing = true,
		examine = { examine_eng_lights },
	},
	goo1 = {
		unique_name = "goo1",
		name = "goo",
		desc = "some kind of gooey slimy substance is on the walls of the duct",
		synonyms = { "goop", "slime", "ooze", "snot", "ectoplasm", "substance" },
		portable = false,
		location = "duct1",
		examine = "The goo is slimy and unpleasant.",
	},
	goo2 = {
		unique_name = "goo1",
		name = "goo",
		desc = "some kind of gooey slimy substance is on the walls of the duct",
		synonyms = { "goop", "slime", "ooze", "snot", "ectoplasm", "substance" },
		portable = false,
		location = "duct2",
		examine = "The goo is unpleasant and slimy.",
	},
	goo3 = {
		unique_name = "goo1",
		name = "goo",
		desc = "some kind of gooey slimy substance is on the walls of the duct",
		synonyms = { "goop", "slime", "ooze", "snot", "ectoplasm", "substance" },
		portable = false,
		location = "duct3",
		examine = "The goo is icky and emanates an odor.",
	},
	goo4 = {
		unique_name = "goo1",
		name = "goo",
		desc = "some kind of gooey slimy substance is on the walls of the duct",
		synonyms = { "goop", "slime", "ooze", "snot", "ectoplasm", "substance" },
		portable = false,
		location = "duct4",
		examine = "The goo is slimy and disgusting.",
	},
	goo5 = {
		unique_name = "goo5",
		name = "goop",
		desc = "some kind of black goop all over the transporter chamber",
		synonyms = { "goo", "slime", "ooze", "snot", "ectoplasm", "substance", },
		portable = false,
		location = "transporter",
		examine = "The goop is black and viscous and foul smelling.\n",
	},
	transporter_console = {
		unique_name = "transporter_console",
		name = "console",
		desc = "a large transporter console",
		location = "transporter_room",
		portable = false,
		examine = "The transporter console has a large lever on it."
	},
	transporter_lever = {
		unique_name = "transporter_lever",
		name = "lever",
		button = true,
		button_state = false,
		last_button_state = false,
		location = "transporter_room",
		related_object = { "on", "transporter_console" },
		suppress_itemizing = true,
	},
	torpedo_console = {
		unique_name = "torpedo_console",
		name = "console",
		desc = "a large torpedo console",
		location = "torpedo_bay",
		portable = false,
		examine = "The torpedo console has a large switch on it.",
		surface = true,
	},
	torpedo_magazine = {
		unique_name = "torpedo_magazine",
		name = "magazine",
		desc = "a very large drum magazine full of torpedoes",
		synonyms = { "drum" },
		examine = { examine_magazine },
		portable = false,
		location = "torpedo_bay",
		torpedo_count = 3;
		container = true,
		container_restrictions = { "bomb" },
	},
	torpedoes = {
		unique_name = "torpedoes",
		name = "torpedoes",
		synonyms = { "torpedo" },
		desc = "torpedoes",
		portable = false,
		related_object = { "in", "magazine" },
		examine = "The torpedoes are far too heavy to move",
		location = "torpedo_bay",
		suppress_itemizing = true,
	},
	torpedo_robot = {
		unique_name = "torpedo robot",
		name = "robot",
		synonyms = { "arm", "loader", "loading", },
		desc = "A large robot arm for loading torpedoes",
		location = "torpedo_bay",
		portable = false,
		examine = "The torpedo loading arm looks big and dangerous.",
	},
	torpedo_switch = {
		unique_name = "torpedo_switch",
		name = "switch",
		desc = "a switch",
		location = "torpedo_bay",
		portable = false,
		related_object = { "on", "torpedo_console" },
		suppress_itemizing = true,
		button = true,
		button_state = false,
		last_button_state = false,
	},
	tube_door = {
		unique_name = "door",
		name = "door",
		synonyms = { "tube", },
		desc = "a torpedo tube door",
		location = "torpedo_bay",
		suppress_itemizing = true,
		portable = false,
	},
	bomb = {
		unique_name = "bomb",
		name = "bomb",
		synonyms = { "explosive", "device" },
		desc = "a large device that looks suspiciously bomb-like",
		location = "engineering",
		portable = true,
		examine = "The bomb large, a little smaller than a torpedo.\n" ..
			"It is a hopeless nest of wires far\n" ..
			"too dangerous to mess with."
	},
};

function flush_output()
	out = intfic.get_output();
	if (out ~= nil) then
		msgtab = intfic.strsplit(out, "\n");
		for i,v in pairs(msgtab) do
			comms_channel_transmit("ROBOT", robot_channel, "-- " .. string.upper(v));
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
	if object_distance ~= nil then
		if sturnvulf == nil then
			dbgout("sturnvulf is nil! (2)");
		end
		distance = object_distance(sturnvulf, player_ids[1]);
		if distance == nil then
			dbgout("distance to sturnvulf is nil! (2)");
			intfic.write("... no response ...\n");
		end
		if distance <= 8000 then
			intfic.send_input(message);
		else
			intfic.write("I'm sorry, your signal is breaking up, say again?\n");
		end
	else
		intfic.send_input(message);
	end
end

function custom_button_press(shipid, button)
	if shipid ~= player_ids[1] then
		register_callback("custom-button-press-event", "custom_button_press");
		return;
	end
	if button == 4 then
		-- science
		science_custom_button_pressed();
	end
	register_callback("custom-button-press-event", "custom_button_press");
end

local function setup_custom_buttons()
	set_custom_button_label(player_ids[1], 4, "TRANSPORTER");
	enable_custom_button(player_ids[1], 4);
	register_callback("custom-button-press-event", "custom_button_press");
end

local function setup_comms()
	set_variable("SUPPRESS_STARBASE_COMPLAINTS", 1);
        -- comms_channel_transmit("ROBOT", 0, ": *** HAILING ON CHANNEL 1234 ***");
	comms_channel_listen("ROBOT", robot_channel, "robot_listen");
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

function science_custom_button_pressed()
	candidates = { "captain_jameson", "engineer_bob", "commander_klon", "doctor_nork" };

	if intfic.objects["transporter_tag"].related_object ~= nil then
		local passenger = intfic.objects["transporter_tag"].related_object[2];
		-- if transporter tag is on a potential passenger
		if intfic.in_array(passenger, candidates) then
			-- get rid of the passenger
			intfic.objects[passenger].location = "nowhere";
			-- drop the tag on the floor whereever it is
			intfic.objects["transporter_tag"].related_object = nil;
			intfic.tag_just_placed = false;
			intfic.write(intfic.objects[passenger].desc ..
					" dematerializes as\nhe is beamed off the STURNVULF\n");
			intfic.write(intfic.objects["transporter_tag"].desc ..
					" remains here.\n");
			-- create the passenger on the player ship
			if (create_passenger ~= nil) then
				create_passenger(intfic.objects[passenger].passenger_number,
							intfic.objects[passenger].desc,
							starbase[1], starbase[1], 1000.0);
				set_passenger_location(intfic.objects[passenger].passenger_number, player_ids[1]);
				play_sound(player_ids[1], "sounds/transporter-sound.ogg");
				text_to_speech(player_ids[1], intfic.objects[passenger].desc .. " has just beamed aboard");
			end
			num_lifeforms_aboard_ship = num_lifeforms_aboard_ship - 1;
		end
	else
		text_to_speech(player_ids[1], "Transporter unable to obtain lock");
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

	-- Transport any tagged lifeforms if we are playing standalone without SNIS
	if intfic.objects["transporter_tag"].related_object ~= nil then
		if player_ids == nil then
			science_custom_button_pressed();
		else
			if not intfic.tag_just_placed then
				intfic.tag_just_placed = true;
				intfic.write("One to beam up!");
			end;
		end
	end

	-- if they open the panel on the eng console, make the board slot visible
	if intfic.objects["eng_console_panel"].doorstatus == "open" then
		intfic.objects["eng_console_panel"].suppress_itemizing = false;
		intfic.setlocation(intfic.objects["eng_console_slot"], "engineering");
		intfic.objects["eng_console_slot"].related_object = { "on", "eng_console" };
	else
		intfic.objects["eng_console_panel"].suppress_itemizing = true;
		intfic.setlocation(intfic.objects["eng_console_slot"], "nowhere");
		intfic.objects["eng_console_slot"].related_object = nil;
	end

	-- if they put the resistor on the circuit board, change the circuit board description
	if intfic.getlocation(intfic.objects["resistor"]) == intfic.getlocation(intfic.objects["circuit_board"]) and
		intfic.objects["resistor"].related_object ~= nil and
		intfic.objects["resistor"].related_object[1] == "on" and
		intfic.objects["resistor"].related_object[2] == "circuit_board" then
		intfic.objects["circuit_board"].examine = "The circuit board looks OK.";
	else
		intfic.objects["circuit_board"].examine =
			"There are burn marks where there should be a resistor";
	end

	-- load a torpedo when they press the torpedo switch
	if intfic.objects["torpedo_switch"].button_state then
		if weapons_active() then
			if intfic.objects["torpedo_magazine"].torpedo_count <= 0 then
				if intfic.objects["bomb"].related_object ~= nil and
					intfic.objects["bomb"].related_object[2] == "torpedo_magazine" then
					intfic.write("The giant magazine rotates 1/10th of a circle.\n");
					intfic.write("The robot arm whirls around grabbing at\n");
					intfic.write("where normally it would find a torpedo,\n");
					intfic.write("but the magazine contains the bomb.\n");
					intfic.write("The torpedo tube door opens with a hiss.\n");
					intfic.write("The robot arm swings towards the tube and\n");
					intfic.write("deposits the bomb into the tube.\n");
					intfic.write("tube door slams shut. There is a violent shudder\n");
					intfic.write("and the bomb is fired off into space.\n");
					intfic.objects["bomb"].related_object = nil;
					intfic.setlocation(intfic.objects["bomb"], "nowhere");
					intfic.bomb_disarmed = true;
					if add_explosion ~= nil then
						add_explosion(svx, svy + 3000, svz, 150, 350, 250, 13, 1);
					end
				else
					intfic.write("The giant magazine rotates 1/10th of a circle.\n");
					intfic.write("The robot arm whirls around grabbing at\n");
					intfic.write("where normally it would find a torpedo,\n");
					intfic.write("but the magazine is empty.\n");
					intfic.write("The torpedo tube door opens with a hiss.\n");
					intfic.write("The robot arm swings towards the tube and\n");
					intfic.write("then back to its resting position and the\n");
					intfic.write("tube door slams shut. There is a violent shudder\n");
					intfic.write("and if there were anything in the tube it\n");
					intfic.write("would have been blasted into space.\n");
				end
			else
				intfic.write("The giant magazine rotates 1/10th of a circle.\n");
				intfic.write("The robot arm whirls around and grabs\n");
				intfic.write("a torpedo from the magazine.\n");
				intfic.write("The torpedo tube door opens with a hiss.\n");
				intfic.write("The robot arm swings towards the tube and\n");
				intfic.write("deposits the massive torpedo into the tube.\n");
				intfic.write("The robot arm then swings back to its resting position\n");
				intfic.write("and the tube door slams shut.\n");
				intfic.write("With a violent shudder, the torpedo is fired off\n");
				intfic.write("into space.\n");
				intfic.objects["torpedo_magazine"].torpedo_count =
					intfic.objects["torpedo_magazine"].torpedo_count - 1;
				if get_object_location ~= nil then
					x, y, z = get_object_location(sturnvulf);
					if x ~= nil then
						-- actually fire a torpedo
						add_torpedo(x, y, z, 0, 90, 0, sturnvulf);
					end
				end
			end
		else
			intfic.write("Nothing happens.\n");
		end
	end

	-- if the player picks up the bomb, make them drop it immediately
	-- unless they use the dolly.
	if intfic.objects["bomb"].location == "pocket" and
		(intfic.objects["bomb"].related_object == nil or
		intfic.objects["bomb"].related_object[2] ~= "dolly") then
		intfic.write("The bomb is too heavy and you immediately set\n");
		intfic.write("it back down.\n");
		intfic.objects["bomb"].location = intfic.current_location;
	end
	-- silently fix mischief like putting the bomb in/or on something that is
	-- not the dolly.
	if intfic.objects["bomb"].related_object ~= nil and
		intfic.objects["bomb"].related_object[2] ~= "dolly" then
			intfic.objects["bomb"].location =
				intfic.getlocation(intfic.objects["bomb"]);
	end

	intfic.objects["torpedo_switch"].button_state = false;
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
	intfic.room["first_mates_quarters"].aft = "duct1";
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
zarkon_timer = 0;

function maintain_zarkon_focus()
	if zarkon ~= nil then
		x, y, z = get_object_location(zarkon);
		if x ~= nil then
			ai_push_attack(zarkon, player_ids[1]);
		else
			zarkon = nil;
		end
	end
	register_timer_callback("maintain_zarkon_focus", 150, 0);
end

function add_zarkons(cookie)
	dbgout("Add zarkons!");
	if sturnvulf == nil then
		dbgout("sturnvulf is nil!");
	end
	dist = object_distance(sturnvulf, player_ids[1]);
	if dist == nil then
		dbgout("distance to sturnvulf is nil!");
		register_timer_callback("add_zarkons", 300, 0);
		return;
	end
	if dist > 15000 then
		dbgout("Distance too great!");
		register_timer_callback("add_zarkons", 300, 0);
		return;
	end

	zarkon_timer = zarkon_timer - 1;
	if zarkon_timer > 0 then
		dbgout("timer not expired: " .. tostring(zarkon_timer));
		register_timer_callback("add_zarkons", 300, 0);
		return;
	end

	dbgout("Adding a zarkon!");
	local x, y, z = random_point_on_sphere(5000);
	px, py, pz = get_object_location(player_ids[1]);
	x = x + px;
	y = y + py;
	z = z + pz;
	zarkon = add_ship(generate_ship_name(), x, y, z, 12, 3, 0);
	ai_push_attack(zarkon, player_ids[1]);
	zarkon_timer = 5 + math.floor(math.random() * 5);
	register_timer_callback("add_zarkons", 300, 0);
end

local function table_count(table)
	local count = 0;
	for k, v in pairs(table) do
		count = count + 1;
	end
	return count;
end

function add_adjusted_ship(name, x, y, z, a, b, c)
	return add_ship(name, x + adjx, y, z + adjz, a, b, c);
end

function add_adjusted_asteroid(x, y, z)
	return add_asteroid(x + adjx, y, z + adjz);
end

function add_adjusted_nebula(name, x, y, z, r)
	return add_nebula(name, x + adjx, y, z + adjz, r);
end

function setup_solarsystem()
	clear_all();
	set_variable("MISSILE_FIRE_CHANCE", "0");
	set_variable("INITIAL_MISSILE_COUNT", "255"); -- give player lots of missiles
	planet = {};
	planet[1] = add_planet("BACHU", adjx + 61667.011707, 65406.588868, adjz + 478750.311434, 6326.000000, 0);
	starbase = {};
	starbase[1] = add_starbase(adjx + 65524.003685, 77976.702190, adjz + 475855.906157, 0);
	planet[2] = add_planet("GUL", adjx + 240767.046176, 10709.084948, adjz + 63069.823786, 4276.000000, 0);
	starbase[2] = add_starbase(adjx + 241301.405915, 18314.471849, adjz + 65224.093602, 1);
	planet[3] = add_planet("OSPOP", adjx + 479256.530329, 4072.975742, adjz + 479257.937371, 5833.000000, 0);
	starbase[3] = add_starbase(adjx + 477643.686755, 12897.278002, adjz + 477648.249863, 2);
	planet[4] = add_planet("DESP", adjx + 62862.999767, 128560.772598, adjz + 181430.733288, 5958.000000, 0);
	starbase[4] = add_starbase(adjx + 66924.897985, 140351.718730, adjz + 182545.952211, 3);
	ship = {};
	ship[1] = add_adjusted_ship("DRACO SAE", 76938.739472, 2319.095262, 471156.345369, 13, 1, 1);
	ship[2] = add_adjusted_ship("FAUCON GALASSIA", 73828.485758, 245.239262, 455766.471069, 8, 0, 1);
	ship[3] = add_adjusted_ship("VOID OJUR", 66929.720709, 140514.542616, 182477.983239, 14, 2, 1);
	ship[4] = add_adjusted_ship("BLACKSKY WINGER", 34697.540990, 3039.694629, 458289.054158, 4, 2, 1);
	ship[5] = add_adjusted_ship("SCORPIUS EPSILON", 58925.430359, 134484.727613, 184861.311878, 11, 1, 1);
	ship[6] = add_adjusted_ship("LUMINO TOPAZ", 480851.888548, 13884.838285, 479044.064837, 20, 2, 1);
	ship[7] = add_adjusted_ship("VOLARE GARNAU", 70870.586894, -25.984958, 502169.870114, 7, 4, 1);
	ship[8] = add_adjusted_ship("CIELO BETA", 485678.789508, 424.577878, 481058.300584, 16, 2, 1);
	ship[9] = add_adjusted_ship("ZEPHYR SHINING", 443672.783283, 16945.838011, 478221.210944, 6, 0, 1);
	ship[10] = add_adjusted_ship("COSMIC MOONGRAZER", 58975.946286, 71628.736928, 479243.183026, 1, 4, 1);
	ship[11] = add_adjusted_ship("OCELOT ESTERION", 109174.690710, 11.549888, 448348.870866, 8, 1, 1);
	ship[12] = add_adjusted_ship("GALAXIE WHISPER", 110970.731128, 204.187889, 441694.701218, 2, 4, 1);
	nebula = {};
	nebula[1] = add_adjusted_nebula("", 48465.902008, 0.000000, 317620.051428, 3595.000000);
	ship[13] = add_adjusted_ship("ELLION HEAVENLY", 454545.958580, 1134.258561, 460351.259852, 9, 0, 1);
	ship[14] = add_adjusted_ship("ELECTRA DARKSKY", 64272.730562, 121166.663121, 194106.555855, 17, 4, 1);
	ship[15] = add_adjusted_ship("STARFALL WINGER", 84989.072014, 67374.404108, 484119.067287, 11, 0, 1);
	ship[16] = add_adjusted_ship("MAGNA STARBORNE", 455287.410298, 1218.313225, 498946.277809, 13, 2, 1);
	ship[17] = add_adjusted_ship("LONTANA DRIFTER", 69906.621658, 80236.015355, 474751.516369, 1, 2, 1);
	ship[18] = add_adjusted_ship("WAYMAKER VOIDNESS", 89238.469201, 120148.025325, 203194.485450, 2, 2, 1);
	ship[19] = add_adjusted_ship("SPARROW RAPIDO", 65899.235393, 126082.079596, 188154.783933, 17, 4, 1);
	ship[20] = add_adjusted_ship("TIBURON HOPEFUL", 240929.159697, 19134.296715, 70202.663361, 15, 2, 1);
	ship[21] = add_adjusted_ship("DRACO POLARIS", 57116.709029, 65491.587537, 485671.172669, 0, 1, 1);
	ship[22] = add_adjusted_ship("COSMIC JAGUAR", 35347.807268, 6013.823595, 170808.833487, 4, 4, 1);
	ship[23] = add_adjusted_ship("MOONGAZER CETUS", 238180.287309, 14872.506300, 69391.085157, 12, 3, 1);
	ship[24] = add_adjusted_ship("TIEMPO ZARMION", 56546.723355, -41.024147, 210696.588457, 2, 2, 1);
	ship[25] = add_adjusted_ship("VEGA SHINING", 67906.865001, 64724.465117, 463007.629657, 14, 2, 1);
	ship[26] = add_adjusted_ship("TUNSTOR MANTA", 251506.079522, 119.109099, 45757.248745, 3, 3, 1);
	ship[27] = add_adjusted_ship("SILENT SOCHE", 241402.500734, 18818.120455, 65089.640833, 6, 0, 1);
	ship[28] = add_adjusted_ship("ALTA SAPPHIRE", 476667.107824, 10577.073772, 476251.827471, 10, 1, 1);
	ship[29] = add_adjusted_ship("RAY POTENTE", 57157.286326, 123631.816616, 183215.725203, 12, 3, 1);
	ship[30] = add_adjusted_ship("COSMO JADE", 256970.240355, 3936.408034, 68992.433398, 16, 0, 1);
	ship[31] = add_adjusted_ship("VISTA ASTERION", 254970.240355, 7936.408034, 68992.433398, 22, 0, 1); -- mantis
	ship[31] = add_adjusted_ship("CEPHEID VOID", 236970.240355, 4936.408034, 65992.433398, 22, 0, 1); -- mantis
	ship[31] = add_adjusted_ship("GALASSIA DEMON", 255970.240355, 9936.408034, 67992.433398, 22, 0, 1); -- mantis
	ship[31] = add_adjusted_ship("LUMIERE HARBINGER", 236970.240355, 4936.408034, 68992.433398, 22, 0, 1); -- mantis
	ship[31] = add_adjusted_ship("PROTON NIGHT", 257970.240355, 4536.408034, 67992.433398, 22, 0, 1); -- mantis
	asteroid = {};
	asteroid[1] = add_adjusted_asteroid(144589.162425, 0.000000, 327373.456506);
	asteroid[2] = add_adjusted_asteroid(146804.882431, 0.000000, 317055.886180);
	asteroid[3] = add_adjusted_asteroid(140639.150981, 0.000000, 312604.686890);
	asteroid[4] = add_adjusted_asteroid(136099.691463, 0.000000, 317148.537309);
	asteroid[5] = add_adjusted_asteroid(138896.040789, 0.000000, 329488.591268);
	asteroid[6] = add_adjusted_asteroid(142571.075976, 0.000000, 331995.857774);
	asteroid[7] = add_adjusted_asteroid(129576.031867, 0.000000, 340258.882880);
	asteroid[8] = add_adjusted_asteroid(118286.447566, 0.000000, 336593.572882);
	asteroid[9] = add_adjusted_asteroid(120010.729074, 0.000000, 325036.618728);
	asteroid[10] = add_adjusted_asteroid(126210.311638, 0.000000, 324966.284668);
	asteroid[11] = add_adjusted_asteroid(128230.095848, 0.000000, 323771.888706);
	asteroid[12] = add_adjusted_asteroid(517379.555217, 0.000000, 337007.933418);
	asteroid[13] = add_adjusted_asteroid(509234.278812, 0.000000, 330469.450783);
	asteroid[14] = add_adjusted_asteroid(502774.415480, 0.000000, 329265.759414);
	asteroid[15] = add_adjusted_asteroid(503825.180134, 0.000000, 341931.459053);
	asteroid[16] = add_adjusted_asteroid(503585.561766, 0.000000, 347116.553700);
	asteroid[17] = add_adjusted_asteroid(499882.800637, 0.000000, 342620.965570);
	asteroid[18] = add_adjusted_asteroid(491882.832521, 0.000000, 331123.486769);
	asteroid[19] = add_adjusted_asteroid(499640.190082, 0.000000, 327204.718171);
	asteroid[20] = add_adjusted_asteroid(503532.407057, 0.000000, 325369.359933);
	asteroid[21] = add_adjusted_asteroid(510564.591715, 0.000000, 321526.446721);
	asteroid[22] = add_adjusted_asteroid(509047.660440, 0.000000, 337344.312827);
	asteroid[23] = add_adjusted_asteroid(507559.769859, 0.000000, 345094.459064);
	asteroid[24] = add_adjusted_asteroid(506903.048141, 0.000000, 344931.092771);
	asteroid[25] = add_adjusted_asteroid(253791.876836, 0.000000, 484839.165762);
	asteroid[26] = add_adjusted_asteroid(263669.210419, 0.000000, 489046.697663);
	asteroid[27] = add_adjusted_asteroid(257610.795406, 0.000000, 489487.915526);
	spacemonster = {};
	spacemonster[1] = add_spacemonster("M. MYSTERIUM", adjx + 507228.306394, -8246.417376, adjz + 328124.460679);
	nebula[2] = add_adjusted_nebula("", 69585.274937, 0.000000, adjz + 300340.578100, 3080.000000);
	spacemonster[2] = add_spacemonster("M. MYSTERIUM", adjx + 254528.951265, 9817.508487, adjz + 481300.977881);
	spacemonster[3] = add_spacemonster("M. MYSTERIUM", adjx + 505180.143081, -9492.437995, adjz + 328542.504220);
	asteroid[28] = add_adjusted_asteroid(250550.687603, 0.000000, 484708.098047);
	derelict = {};
	derelict[1] = add_derelict("XII", adjx + 246566.351322, 20139.833471, adjz + 64167.836048, 10, 3);
	spacemonster[4] = add_spacemonster("M. MYSTERIUM", 257263.566660, 11119.829349, 480949.289835);
	nebula[3] = add_adjusted_nebula("", 39506.168309, 0.000000, adjz + 301620.539418, 1363.000000);
	asteroid[29] = add_adjusted_asteroid(262941.277573, 0.000000, 494743.096644);
	cargo_container = {};
	cargo_container[1] = add_cargo_container(247371.156986, 19345.582006, 63816.013416, 9.159448, 1.359991, -12.703562);
	ship[31] = add_adjusted_ship("ESPLORATORE GLOBE", 231172.860393, 692.198850, 140661.548819, 3, 1, 1);
	nebula[4] = add_adjusted_nebula("", 43346.053382, 0.000000, 299700.598558, 3228.000000);
	asteroid[30] = add_adjusted_asteroid(255742.626491, 0.000000, 496489.954199);
	asteroid[31] = add_adjusted_asteroid(248799.764496, 0.000000, 502112.645048);
	asteroid[32] = add_adjusted_asteroid(241725.908810, 0.000000, 494774.391436);
	asteroid[33] = add_adjusted_asteroid(245024.333980, 0.000000, 483383.192260);
	asteroid[34] = add_adjusted_asteroid(258403.781028, 0.000000, 469523.072838);
	derelict[2] = add_derelict("NUSE", adjx + 240329.526069, 4361.679071, adjz + 67303.152010, 22, 3);
	asteroid[35] = add_adjusted_asteroid(264766.907381, 0.000000, 464500.139831);
	asteroid[36] = add_adjusted_asteroid(262242.416649, 0.000000, 482515.299258);
	cargo_container[2] = add_cargo_container(240040.080268, 4722.030634, 67335.528719, -13.050931, -2.015906, 7.549839);
	asteroid[37] = add_adjusted_asteroid(270023.400647, 0.000000, 496777.257059);
	asteroid[38] = add_adjusted_asteroid(271861.112597, 0.000000, 498135.431300);
	spacemonster[5] = add_spacemonster("M. MYSTERIUM", adjx + 132271.386258, 5507.558810, adjz + 323115.599520);
	cargo_container[3] = add_cargo_container(239883.107612, 4822.357782, 67340.274813, -13.923001, -1.458533, 7.576207);
	spacemonster[6] = add_spacemonster("M. MYSTERIUM", adjx + 132468.676894, 8278.155057, adjz + 322460.100621);
	black_hole = {};
	black_hole[1] = add_black_hole("VIPH", 473413.278755, 0.000000, 143545.364655, 1073.000000);
	ship[32] = add_adjusted_ship("GALASSIA ZIRIPH", 233402.840185, -110.376835, 143982.919465, 2, 3, 1);
	nebula[5] = add_adjusted_nebula("", 75985.084882, 0.000000, 310580.266412, 1739.000000);
	nebula[6] = add_adjusted_nebula("", 94544.532939, 0.000000, 322099.914924, 1909.000000);
	nebula[7] = add_adjusted_nebula("", 56145.673271, 0.000000, 314420.149249, 3493.000000);
	nebula[8] = add_adjusted_nebula("", 57425.635707, 0.000000, 324019.855784, 2899.000000);
	nebula[9] = add_adjusted_nebula("", 45905.978254, 0.000000, 329779.679482, 2836.000000);
	nebula[10] = add_adjusted_nebula("", 38226.205873, 0.000000, 326579.777303, 2581.000000);
	nebula[11] = add_adjusted_nebula("", 35666.282119, 0.000000, 308020.343775, 2893.000000);
	nebula[12] = add_adjusted_nebula("", 61265.521898, 0.000000, 297140.675922, 1475.000000);
	nebula[13] = add_adjusted_nebula("", 80464.951731, 0.000000, 320179.972947, 5162.000000);
	nebula[14] = add_adjusted_nebula("", 81104.932391, 0.000000, 327859.738622, 1398.000000);
	nebula[15] = add_adjusted_nebula("", 91344.628526, 0.000000, 304180.460937, 2907.000000);
	nebula[16] = add_adjusted_nebula("", 90704.646749, 0.000000, 300980.558759, 1994.000000);
	nebula[17] = add_adjusted_nebula("", 76625.065541, 0.000000, 305460.422256, 3246.000000);
	nebula[18] = add_adjusted_nebula("", 63825.445652, 0.000000, 318900.011628, 3432.000000);

	-- set all asteroid speeds to 0 to keep them from moving.
	for k, v in ipairs(asteroid) do
		set_asteroid_speed(v, 0.0);
	end

	-- set initial NPC ships to faction 0 (neutral) to keep them docile
	for k, v in ipairs(ship) do
		set_faction(v, 0);
	end;

	player_ids = get_player_ship_ids();
	-- set player 1 location
	if (player_ids[1]) then
		reset_player_ship(player_ids[1]);
		dock_player_to_starbase(player_ids[1], starbase[2]);
	end
	sturnvulf = add_ship("STURNVULF", svx, svy, svz, 13, 0, 0);
	velnoxa = add_derelict("VELNOXA", svx, svy + 1000, svz - 5000, 17, 0);
	set_object_velocity(velnoxa, 0, 0, 0);
	ai_push_catatonic(sturnvulf); -- Make the ship just sit there doing nothing.
	add_zarkons(0);


	-- Add a bunch of cargo containers around a random asteroid
	intfic.cargo_container_asteroid = math.floor(math.random() * table_count(asteroid)) + 1;
	if asteroid[intfic.cargo_container_asteroid] == nil then
		intfic.cargo_container_asteroid = 1; -- should not happen
	end
	cca = asteroid[intfic.cargo_container_asteroid];
	intfic.ccasteroidname = get_object_name(cca);
	intfic.ccasteroidx, intfic.ccasteroidy, intfic.ccasteroidz = get_object_location(cca);
	special_cargo_container = {};
	macguffin = add_commodity("WEAPONS COMPONENTS", "PIECES", "UNKNOWN METALS", 1000.0, 0.5, 1.0, 0.0, 0.0, 0.0, 0.0);
	for cc = 1,8 do
		local ccx, ccy, ccz = random_point_on_sphere(2000, 2000, 2000);
		ccx = ccx + intfic.ccasteroidx;
		ccy = ccy + intfic.ccasteroidy;
		ccz = ccz + intfic.ccasteroidz;
		special_cargo_container[cc] = add_cargo_container(ccx, ccy, ccz, 0, 0, 0);
		set_commodity_contents(special_cargo_container[cc], macguffin, 100.0);
	end
end

function player_docked(id, station)
	if station == starbase[2] then
		local weapons = 0;
		local passengers = 0;
		for i = 0,7 do
			local cargoitem = "cargo[" .. tostring(i) .. "].contents.item";
			local cargoqty = "cargo[" .. tostring(i) .. "].contents.qty";
			attr = get_ship_attribute(player_ids[1], cargoitem);
			qty = get_ship_attribute(player_ids[1], cargoqty);
			if attr >= 0 then
				local name = get_commodity_name(attr);
				if name == "WEAPONS COMPONENTS" then
					weapons = weapons + qty;
				end
			end
		end

		local candidates = { "captain_jameson", "engineer_bob", "commander_klon", "doctor_nork" };
		for k, v in pairs(candidates) do
			if intfic.getlocation(intfic.objects[v]) == "nowhere" then
				passengers = passengers + 1;
			end
		end

		if weapons == 0 and passengers == 0 then
			register_callback("player-docked-event", "player_docked");
			return;
		end

		local passenger_msg = "";
		if passengers == 4 then
			passenger_msg = "Congratulations, you rescued all the passengers.\n";
		else
			passenger_msg = "You rescued " .. tostring(passengers) .. " out of 4 passengers.\n";
		end

		local weapons_msg = "";
		if weapons == 800 then
			weapons_msg = "Congratulations, you recovered all the weapons\ncomponents.";
		else
			weapons_msg = "You recovered " .. tostring(weapons) ..
				" out of 800 weapons components.\n";
		end
		comms_transmission(starbase[2], passengers_msg);
		comms_transmission(starbase[2], weapons_msg);
		text_to_speech(player_ids[1], passengers_msg);
		text_to_speech(player_ids[1], weapons_msg);

		if intfic.bomb_disarmed then
			bomb_msg = "Congratulations, you disarmed the bomb.\n";
		else
			bomb_msg = "You failed to disarm the bomb aboard the STURMVULF.\n";
		end
		comms_transmission(starbase[2], bomb_msg);
		text_to_speech(player_ids[1], bomb_msg);
	end
	register_callback("player-docked-event", "player_docked");
end

function introduction(cookie)
	comms_transmission(starbase[2], "HELLO");
	comms_transmission(starbase[2], "WE HAVE A MISSION FOR YOU");
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "THE SHIP STURNVULF WAS SECRETLY TRANSPORTING");
	comms_transmission(starbase[2], "WEAPONS PARTS WHEN WE BELIEVE IT WAS INTERCEPTED");
	comms_transmission(starbase[2], "BY ZARKONS AND DISABLED");
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "WE HAVE REASON TO BELIEVE THERE ARE CREWMEMBERS STILL");
	comms_transmission(starbase[2], "ABOARD THE SHIP IN NEED OF RESCUE");
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "ADDITIONALLY, THE STURVULF EJECTED THE CONTRABAND");
	comms_transmission(starbase[2], "IT WAS CARRYING BEFORE IT WAS DISABLED IN AN ATTEMPT");
	comms_transmission(starbase[2], "TO THROW OFF THE ZARKONS.  WE WOULD LIKE YOU TO RECOVER");
	comms_transmission(starbase[2], "THIS CARGO AS WELL AS ANY SURVIVORS STILL ON THE STURNVULF");
	comms_transmission(starbase[2], "BEWARE OF ZARKON DISRUPTORS IN THE AREA.");
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "THE SHIP VELNOXA WAS SENT TO INVESTIGATE BUT WAS");
	comms_transmission(starbase[2], "DESTROYED, WE THINK BY THE ZARKONS, BUT NOT BEFORE");
	comms_transmission(starbase[2], "IT LANDED A RESCUE ROBOT ABOARD THE STURNVULF WITH");
	comms_transmission(starbase[2], "A TRANSPORTER TAG WHICH MAY BE PLACED ON ANY SURVIVORS");
	comms_transmission(starbase[2], "TO ENABLE YOUR TRANSPORTER TO OBTAIN A GOOD LOCK DESPITE");
	comms_transmission(starbase[2], "ANY ADVERSE CONDITIONS ABOARD THE STURNVULF.");
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "THE RESCUE ROBOT LANDED ON THE STURNVULF BY THE VELNOXA");
	comms_transmission(starbase[2], "WAS LISTENING FOR INSTRUCTIONS ON CHANNEL " .. tostring(robot_channel));
	comms_transmission(starbase[2], "AND SHOULD STILL BE LISTENING.  HOWEVER, YOU MUST BE CLOSE");
	comms_transmission(starbase[2], "TO THE STURNVULF TO COMMUNICATE SUCCESSFULLY WITH IT.");
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "THE LAST KNOWN POSITION STURNVULF WAS:");
	comms_transmission(starbase[2], "X = " .. tostring(math.floor(svx / 1000) * 1000));
	comms_transmission(starbase[2], "Y = " .. tostring(math.floor(svy / 1000) * 1000));
	comms_transmission(starbase[2], "Z = " .. tostring(math.floor(svz / 1000) * 1000));
	comms_transmission(starbase[2], "");
	comms_transmission(starbase[2], "YOUR MISSION IS TO RESCUE ANY SURVIVORS ABOARD THE\n");
	comms_transmission(starbase[2], "STURNVULF AND RECOVER THE CARGO OF WEAPONS COMPONENTS\n");
	comms_transmission(starbase[2], "AND RETURN THEM BOTH TO SB-01.  IT IS ALSO IMPORTANT\n");
	comms_transmission(starbase[2], "THAT THE STURNVULF ITSELF NOT BE DESTROYED.\n");
	comms_transmission(starbase[2], "GOOD LUCK");
	setup_custom_buttons();
	setup_comms(); -- input/output via SNIS comms
end

if set_variable == nil then
	intfic.gameloop(); -- standalone mode i/o via terminal
else
	setup_solarsystem();
	register_timer_callback("introduction", 300, 0);
	register_callback("player-docked-event", "player_docked");
end;

