package.path = package.path .. ";share/snis/luascripts/MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

intfic.current_location = "corridor";
intfic.cardinal_directions = { "fore", "starboard", "aft", "port", "up", "down" };
intfic.room = {
	corridor = {
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
		shortdesc = "Ok, I'm on the bridge",
		desc = "To the aft is a corridor",
		aft = "corridor",
		visited = false,
	},
	engineroom = {
		shortdesc = "Ok now I'm in the engine room",
		desc = "To fore is a corridor",
		fore = "corridor",
		visited = false,
	},
	closet = {
		shortdesc = "I'm in a utility closet",
		desc = "A corridor is to starboard.",
		visited = false,
	},
	pocket = {
		shortdesc = "pocket",
		desc = "pocket",
	},
};

intfic.objects = {
	mop = {
		location = "closet",
		name = "mop",
		desc = "a mop",
		portable = true,
		examine = "Um ... it's a mop? I mean... it's just a mop",
	},
	bucket = {
		location = "closet",
		name = "bucket",
		desc = "a bucket",
		portable = true,
		examine = "Well, it's a plastic bucket.  It's got a handle.",
	},
	coin = {
		location = "pocket",
		name = "coin",
		synonyms = { "money", "currency", "moolah" },
		desc = "a coin",
		portable = true,
		examine = "It's just a $5 coin."
	},
	coin2 = {
		location = "bridge",
		name = "coin",
		synonyms = { "money", "currency", "moolah" },
		desc = "a coin",
		portable = true,
		examine = "It's just a $5 coin."
	},
	panel = {
		location = "bridge",
		name = "panel",
		desc = "a large, complicated control panel",
		portable = false,
		examine = "Well, it's got lots of lights and switches and buttons and stuff.",
	},
	drive = {
		location = "engineroom",
		name = "drive",
		desc = "a large dangerous looking warp drive",
		portable = false,
		examine = "Uh, it's covered in blinking lights and making a deep humming sound.",
	},
	closet_door1 = {
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
	if string.sub(message, 1, 3) == "-- " then
		return;
	end;
	intfic.send_input(message);
end

local function setup_comms()
	set_variable("SUPPRESS_STARBASE_COMPLAINTS", 1);
	comms_channel_listen("ROBOT", 1234, "robot_listen");
	register_timer_callback("flush_output", 10, 0);
end

if false then
	intfic.gameloop();
else
	setup_comms();
end;

