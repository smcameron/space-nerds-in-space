package.path = package.path .. ";share/snis/luascripts/MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

intfic.current_location = "corridor";
intfic.cardinal_directions = { "fore", "starboard", "aft", "port", "up", "down" };
intfic.room = {
	corridor = {
		shortdesc = "Corridor",
		desc = "I am in a long corridor. To the fore is the bridge.\n" ..
			"To the aft is the engine room. To port is a small\n" ..
			"utility closet.",
		fore = "bridge";
		aft = "engineroom",
		port = "closet",
		visited = false,
	},
	bridge = {
		shortdesc = "Bridge",
		desc = "I am on the bridge. To the aft is a corridor",
		aft = "corridor",
		visited = false,
	},
	engineroom = {
		shortdesc = "Engine Room",
		desc = "I am in the engine room. To fore is a corridor",
		fore = "corridor",
		visited = false,
	},
	closet = {
		shortdesc = "Utility Closet",
		desc = "I am in the utility closet. A corridor is to starboard.",
		starboard = "corridor",
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
	},
	bucket = {
		location = "closet",
		name = "bucket",
		desc = "a bucket",
		portable = true,
	},
	knife = {
		location = "pocket",
		name = "knife",
		desc = "a small pen knife",
		portable = true,
	},
	panel = {
		location = "bridge",
		name = "panel",
		desc = "a large, complicated control panel",
		portable = false,
	},
	drive = {
		location = "engineroom",
		name = "drive",
		desc = "a large dangerous looking warp drive",
		portable = false,
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

