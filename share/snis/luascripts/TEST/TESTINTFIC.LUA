-- TEST INTERACTIVE FICTION LUA MODULE

package.path = package.path .. ";share/snis/luascripts/MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

arg = {...};

intfic.current_location = "corridor";
intfic.cardinal_directions = { "fore", "starboard", "aft", "port", "up", "down" };
intfic.room = {
	corridor = {
		unique_name = "corridor",
		shortdesc = "Ok, I am in the corridor",
		desc = "To the fore is the bridge.\n" ..
			"To the aft is the engine room.\n" ..
			"To port is the door of a small\n" ..
			"utility closet.",
		fore = "bridge";
		aft = "engineroom",
		visited = false,
	},
	bridge = {
		unique_name = "bridge",
		shortdesc = "Ok, I'm on the bridge",
		desc = "To the aft is a corridor",
		aft = "corridor",
		visited = false,
	},
	engineroom = {
		unique_name = "engineroom",
		shortdesc = "Ok now I'm in the engine room",
		desc = "To fore is a corridor",
		fore = "corridor",
		visited = false,
	},
	closet = {
		unique_name = "closet",
		shortdesc = "I'm in a utility closet",
		desc = "A corridor is to starboard.",
		visited = false,
	},
	pocket = {
		unique_name = "pocket",
		shortdesc = "pocket",
		desc = "pocket",
	},
};

local function bucket_examine(entry)
	intfic.write("Well, it's a bucket\n");
end

intfic.objects = {
	mop = {
		unique_name = "mop",
		location = "closet",
		name = "mop",
		desc = "a mop",
		portable = true,
		examine = "Um ... it's a mop? I mean... it's just a mop",
	},
	bucket = {
		unique_name = "bucket",
		location = "closet",
		name = "bucket",
		desc = "a bucket",
		portable = true,
		examine = { bucket_examine },
		container = true,
	},
	pail = {
		unique_name = "pail",
		location = "closet",
		name = "pail",
		desc = "a pail",
		portable = true,
		examine = "Well, it's a plastic pail.  It's got a handle.",
		container = true,
	},
	can = {
		unique_name = "can",
		location = "closet",
		name = "can",
		desc = "a can",
		portable = true,
		examine = "Well, it's a tin can.",
		container = true,
	},
	coin = {
		unique_name = "coin",
		location = "pocket",
		name = "coin",
		synonyms = { "money", "currency", "moolah" },
		desc = "a coin",
		portable = true,
		examine = "It's just a $5 coin."
	},
	coin2 = {
		unique_name = "coin2",
		location = "bridge",
		name = "coin",
		synonyms = { "money", "currency", "moolah" },
		desc = "a coin",
		portable = true,
		examine = "It's just a $5 coin."
	},
	panel = {
		unique_name = "panel",
		location = "bridge",
		name = "panel",
		desc = "a large, complicated control panel",
		portable = false,
		examine = "Well, it's got a button and a switch on it.",
		surface = true,
	},
	button = {
			unique_name = "button1",
			name = "button",
			desc = "a button",
			location = "bridge",
			related_object = { "on", "panel" },
			suppress_itemizing = true,
			portable = false,
			button_state = false,
			last_button_state = false,
			button = true,
	},
	switch = {
			unique_name = "switch1",
			name = "switch",
			desc = "a switch",
			location = "bridge",
			related_object = { "on", "panel" },
			suppress_itemizing = true,
			portable = false,
			button_state = false,
			last_button_state = false,
			button = true,
	},
	drive = {
		unique_name = "drive",
		location = "engineroom",
		name = "drive",
		desc = "a large dangerous looking warp drive",
		portable = false,
		examine = "Uh, it's covered in blinking lights and making a deep humming sound.",
	},
	closet_door1 = {
		unique_name = "closet_door1",
		location = "corridor",
		name = "door",
		desc = "closet door",
		portable = false,
		suppress_itemizing = true,
		doorstatus = "closed",
		doorroom = "closet",
		doordirout = "port",
		complement_door = "closet_door2"; -- doors are in pairs
	},
	closet_door2 = {
		unique_name = "closet_door2",
		location = "closet",
		name = "door",
		desc = "closet door",
		portable = false,
		suppress_itemizing = true,
		doorstatus = "closed",
		doorroom = "corridor",
		doordirout = "starboard",
		complement_door = "closet_door1"; -- doors are in pairs
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

function intfic.after_each_turn_hook()

	-- Make the button a momentary switch
	if intfic.objects["button"].button_state == true then
		intfic.objects["button"].button_state = false; -- unpress the button
		intfic.write("I hear a dull clunking sound in the distance.\n");
	end

	-- Make the toggle switch alternate
	if intfic.objects["switch"].button_state and not intfic.objects["switch"].last_button_state then
		intfic.objects["switch"].last_button_state = true;
		intfic.write("A loud buzzer sounds 3 times in quick succession.\n");
	elseif not intfic.objects["switch"].button_state and intfic.objects["switch"].last_button_state then
		intfic.objects["switch"].last_button_state = false;
		intfic.write("A faint whirring I hadn't previously noticed has now stopped.\n");
	end
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

function setup_directional_shortcuts()
	intfic.verb["p"] = { dogoport };
	intfic.verb["s"] = { dogostarboard };
	intfic.verb["f"] = { dogofore };
	intfic.verb["a"] = { dogoaft };
	intfic.verb["u"] = { dogoup };
	intfic.verb["d"] = { dogodown };
end

setup_directional_shortcuts();

if set_variable == nil then
	intfic.gameloop(); -- standalone mode i/o via terminal
else
	setup_comms(); -- input/output via SNIS comms
end;

