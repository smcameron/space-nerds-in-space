package.path = package.path .. ";share/snis/luascripts/MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

arg = {...};

previous_location = nil;

intfic.current_location = "outside_airlock";
intfic.cardinal_directions = { "fore", "starboard", "aft", "port", "up", "down" };
intfic.room = {
	outside_airlock = {
		unique_name = "outside_airlock",
		shortdesc = "Ok, I am outside the airlock",
		desc = "To aft is the the airlock, and to fore,\n" ..
			"the blackness of space\n" ..
			"There is a slot in the wall next to the door",
		aft = nil,
		visited = false,
	},
	airlock = {
		unique_name = "airlock",
		shortdesc = "Ok, I am in the airlock",
		desc = "There is a button and a switch on the wall.\n" ..
			"To fore is the outer door.  To aft, the inner door\n",
		fore = nil,
		visited = false,
	},
	corridor1 = {
		unique_name = "corridor1",
		shortdesc = "Ok, I am in a corridor",
		desc = "I am in a corridor. To fore is the airlock.\n" ..
			"There is a slot on the wall next to the airlock.",
		fore = nil,
		visited = false,
	};
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
		examine = "It looks like you could put a card into the slot\n",
	},
	slot2 = {
		unique_name = "slot2",
		location = "corridor1",
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
		examine = "The body is frozen solid.",
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
			examine = "The button is labeled OUTER DOOR\n",
	},
	switch = {
			unique_name = "switch1",
			name = "switch",
			desc = "a switch",
			location = "airlock",
			suppress_itemizing = true,
			portable = false,
			button_state = false,
			last_button_state = false,
			button = true,
			examine = "The switch is labeled INNER DOOR\n",
	},
	airlock_door1 = {
		unique_name = "airlock_door1",
		location = "outside_airlock",
		portable = false,
		suppress_itemizing = true,
		name = "door",
	},
	airlock_door2 = {
		unique_name = "airlock_door1",
		location = "airlock",
		portable = false,
		suppress_itemizing = true,
		name = "door",
	},
	airlock_door3 = {
		unique_name = "airlock_door3",
		location = "corridor1",
		portable = false,
		suppress_itemizing = true,
		name = "door",
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
	intfic.room["corridor1"].fore = nil;
	intfic.room["airlock"].aft = nil;
end

function open_inner_airlock_door()
	intfic.room["corridor1"].fore = "airlock";
	intfic.room["airlock"].aft = "corridor1";
end

function intfic.after_each_turn_hook()

	-- Make the ID card open the outer airlock door when put into the slot
	if intfic.objects["card"].location == "outside_airlock" and
		intfic.objects["card"].related_object ~= nil and
		intfic.objects["card"].related_object[2] == "slot" then
			-- put the card in the player's pocket
			intfic.objects["card"].location = "pocket";
			intfic.objects["card"].related_object = nil;
			open_outer_airlock_door();
			intfic.write("The outer airlock door opens. I retain the ID card.\n");
	end

	-- Make the ID card open the inner airlock door when put into the slot
	if intfic.objects["card"].location == "corridor1" and
		intfic.objects["card"].related_object ~= nil and
		intfic.objects["card"].related_object[2] == "slot2" then
			-- put the card in the player's pocket
			intfic.objects["card"].location = "pocket";
			intfic.objects["card"].related_object = nil;
			open_inner_airlock_door();
			intfic.write("The inner airlock door opens. I retain the ID card.\n");
	end

	-- Make the button a momentary switch that opens or closes the outer airlock door
	if intfic.objects["button"].button_state == true then
		intfic.objects["button"].button_state = false; -- unpress the button
		if intfic.room["outside_airlock"].aft == nil then
			if intfic.room["corridor1"].fore ~= nil then
				intfic.write("The inner airlock door closes\n");
				close_inner_airlock_door();
			end
			open_outer_airlock_door();
			intfic.write("The outer airlock door opens\n");
		else
			close_outer_airlock_door();
			intfic.write("The outer airlock door closes\n");
		end
	end

	-- Make the switch a momentary switch that opens or closes the outer airlock door
	if intfic.objects["switch"].button_state == true then
		intfic.objects["switch"].button_state = false; -- unpress the button
		if intfic.room["corridor1"].fore == nil then
			if intfic.room["outside_airlock"].aft ~= nil then
				intfic.write("The outer airlock door closes\n");
				close_outer_airlock_door();
			end
			open_inner_airlock_door();
			intfic.write("The inner airlock door opens\n");
		else
			close_inner_airlock_door();
			intfic.write("The inner airlock door closes\n");
		end
	end

	-- If you leave the airlock, the door should close behind you
	if previous_location == "airlock" then
		if intfic.current_location == "outside_airlock" then
			close_outer_airlock_door();
			intfic.write("The outer airlock door closes behind you\n");
		elseif intfic.current_location == "corridor1" then
			close_inner_airlock_door();
			intfic.write("The inner airlock door closes behind you\n");
		end
	end

	-- If they pick up Ensign Mallory, draw attention to the ID card
	if intfic.objects["body"].location == "pocket" and
		intfic.objects["card"].suppress_itemizing then
		intfic.write("You notice an ID card on the body.\n");
		intfic.objects["card"].suppress_itemizing = false;
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

