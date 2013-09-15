
-- First, clear the board
clear_all();

startx = 1000;
starty = 1000;

trapx = 100000;
trapy = 90000;
trapz = 0;
trap_spring_distance = 3000;

-- find the player and move him where we want him to start.
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    move_object(player_ids[1], startx, starty, 0);
    player_name = get_object_name(player_ids[1]);
end

-- add a starbase
sb = add_starbase(startx + 500, starty + 500, 0, 1);

-- bait the trap
derelict = add_derelict("COR AUREOS", trapx, trapy, trapz, 1, 2);

zarkon = 3;
disruptor = 12;
battlestar = 5;

function spring_the_trap()
	local x, y, z;

	x, y, z = get_object_location(player_ids[1]);

	badguy1 = add_ship("MERCILESS", x + 5000, y + 4000, 25, disruptor, zarkon); 
	badguy2 = add_ship("SCOURGE", x - 5500, y + 5000, 15, disruptor, zarkon); 
	badguy3 = add_ship("RESISTANCE", x - 7000, y + 2000, 25, disruptor, zarkon); 
	badguy4 = add_ship("FEARLESS", x - 5000, y + 4500, 25, disruptor, zarkon); 
	badguy5 = add_ship("ZHARK", x + 5300, y + 6000, 25, battlestar, zarkon); 
	badguy6 = add_ship("XANUU", x - 3000, y + 4000, 25, battlestar, zarkon); 

	attack_ship(badguy1, player_ids[1]);
	attack_ship(badguy2, player_ids[1]);
	attack_ship(badguy3, player_ids[1]);
	attack_ship(badguy4, player_ids[1]);
	attack_ship(badguy5, player_ids[1]);
	attack_ship(badguy6, player_ids[1]);
end

function mayday_message(cookie)

	local x, y, z, dist, dx, dy, dz;

	if (cookie % 60) == 1 then
		comms_transmission(derelict, ": MAYDAY. SITUATION CRITICAL. SEND HELP.");
		comms_transmission(derelict, string.format(": LOCATION: (%f %f)", trapx, trapy));
		comms_transmission(derelict, ": LIFE SUPPORT SYSTEMS NEAR FAILURE");
		comms_transmission(derelict, ": MOST OF THE CREW IS DEAD");
		comms_transmission(derelict, ": SHIP IS BADLY DAMAGED");
		comms_transmission(derelict, "");
	end

	-- calculate distance between player and trap
	trapx, trapy, trapz = get_object_location(derelict);
	x, y, z = get_object_location(player_ids[1]);
	dx = x - trapx;
	dy = y - trapy;
	dz = z - trapz;
	dist = math.sqrt(dx * dx + dy * dy + dz * dz);

	-- close enough?
	if dist < trap_spring_distance then
		spring_the_trap();
	else
		register_timer_callback("mayday_message", 10, cookie + 1);
	end
end

register_timer_callback("mayday_message", 10, 0);

