package.path = package.path .. ";../MISSIONS/lib/?.lua";
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

intfic.gameloop();
