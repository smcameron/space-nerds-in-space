
-- First, clear the board
clear_all();

startx = 1000;
starty = 1000;

trapx = 100000;
trapz = 90000;
itrapx = trapx;
itrapy = 0;
itrapz = trapz;
trapy = 0;
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
trap_sprung = 0;

function spring_the_trap()
	local x, y, z;

	trap_sprung = 1;
	x, y, z = get_object_location(player_ids[1]);

	badguy1 = add_ship("MERCILESS", x + 5000, 25, z + 4000, disruptor, zarkon, 0); 
	badguy2 = add_ship("SCOURGE", x - 5500, 15, z + 5000, disruptor, zarkon, 0); 
	badguy3 = add_ship("RESISTANCE", x - 7000, 25, z + 2000, disruptor, zarkon, 0); 
	badguy4 = add_ship("FEARLESS", x - 5000, 25, z + 4500, disruptor, zarkon, 0); 
	badguy5 = add_ship("ZHARK", x + 5300, 25, z + 6000, battlestar, zarkon, 0); 
	badguy6 = add_ship("XANUU", x - 3000, 25, z + 4000, battlestar, zarkon, 0); 

	ai_push_attack(badguy1, player_ids[1]);
	ai_push_attack(badguy2, player_ids[1]);
	ai_push_attack(badguy3, player_ids[1]);
	ai_push_attack(badguy4, player_ids[1]);
	ai_push_attack(badguy5, player_ids[1]);
	ai_push_attack(badguy6, player_ids[1]);
end

function spring_second_trap()
	local x, y, z;

	trap_sprung = 1;
	x, y, z = get_object_location(player_ids[1]);

	badguy1 = add_ship("KHUBHI", x - 5000, 25, z - 4000, disruptor, zarkon, 0); 
	badguy2 = add_ship("BIZHONI", x - 3500, 15, z - 4100, disruptor, zarkon, 0); 
	badguy3 = add_ship("RHO", x - 7000, 25, z - 2000, disruptor, zarkon, 0); 
	badguy4 = add_ship("DAAR", x - 2700, 25, z - 4200, disruptor, zarkon, 0); 
	badguy5 = add_ship("ZUURHI", x + 1300, 25, z - 6000, battlestar, zarkon, 0); 
	badguy6 = add_ship("KHONRAD", x - 1000, 25, z - 4000, battlestar, zarkon, 0); 

	ai_push_attack(badguy1, player_ids[1]);
	ai_push_attack(badguy2, player_ids[1]);
	ai_push_attack(badguy3, player_ids[1]);
	ai_push_attack(badguy4, player_ids[1]);
	ai_push_attack(badguy5, player_ids[1]);
	ai_push_attack(badguy6, player_ids[1]);
end

function distancexyz(dx, dy, dz)
	return math.sqrt(dx * dx + dy * dy + dz * dz);
end

function distance(o1, o2)
	local x1, y1, z1, x2, y2, z2;

	x1, y1, z1 = get_object_location(o1);
	x2, y2, z2 = get_object_location(o2);
	return distancexyz(x1 - x2, y1 - y2, z1 - z2);
end

function mayday_message(cookie)

	local dist;
	local ux, uy, uz;

	dist = distance(derelict, player_ids[1]);

	if dist < 50000 and dist > 2000 then
		ux, uy, uz = user_coords(trapx, trapy, trapz);
		comms_transmission(derelict, ": MAYDAY. SITUATION CRITICAL. SEND HELP.");
		comms_transmission(derelict, ": LAST KNOWN LOCATION:");
		comms_transmission(derelict, string.format(": (%f %f %f)", ux, uy, uz));
		comms_transmission(derelict, ": LIFE SUPPORT SYSTEMS NEAR FAILURE");
		comms_transmission(derelict, ": MOST OF THE CREW IS DEAD");
		comms_transmission(derelict, ": SHIP IS BADLY DAMAGED");
		comms_transmission(derelict, "");
	end

	if trap_sprung == 0 then
		register_timer_callback("mayday_message", 60 * 10, 0);
	end
end

function check_trap(cookie)
	if distance(derelict, player_ids[1]) < trap_spring_distance then
		spring_the_trap();
	else
		register_timer_callback("check_trap", 10, cookie + 1);
	end
end

function starbase_message(cookie)
	local ux, uy, uz = user_coords(itrapx, itrapy, itrapz);
	comms_transmission(sb, string.format(": %s, WE HAVE RECEIVED A WEAK DISTRESS",
			player_name));
	comms_transmission(sb, ": SIGNAL FROM THE COR AUREOS WHICH HAS BEEN");
	comms_transmission(sb, ": BADLY DAMAGED.  LAST KNOWN LOCATION:");
	comms_transmission(sb, string.format(": (%f %f %f)", ux, uy, uz));
	comms_transmission(sb, ": YOUR NEW ORDERS ARE TO INVESTIGATE AND RESCUE");
	comms_transmission(sb, ": ANY SURVIVORS.  ADDITIONALLY, WE HAVE REASON");
	comms_transmission(sb, ": TO BELIEVE THE COR AUREOS WAS CARRYING TOP");
	comms_transmission(sb, ": SECRET PLANS OF THE ZARKON ALLIANCE.  FOR");
	comms_transmission(sb, ": THIS REASON THE COR AUREOS ITSELF MUST BE");
	comms_transmission(sb, ": RECOVERED AND TOWED BACK TO STARBASE.");
	comms_transmission(sb, ":");
	comms_transmission(sb, ": GOOD LUCK.");
end

second_trap_sprung = 0;

function check_derelict_pos()

	local dist;

	dist = distance(derelict, sb);

	if dist < 40000 and second_trap_sprung == 0 then
		second_trap_sprung = 1;
		spring_second_trap();
	end 
	
	if dist < 1000 then
		comms_transmission(sb, string.format(": %s, CONGRATULATIONS ON RETURNING",
				player_name));
		comms_transmission(sb, ": THE COR AUREOS WITH ITS PRECIOUS CARGO.  THE");
		comms_transmission(sb, ": SECRET PLANS OF THE ZARKON ALLIANCE WILL NO");
		comms_transmission(sb, ": DOUBT PROVE USEFUL IN RESISTING THEIR FORCES");
		comms_transmission(sb, ": A CASK OF KOLUNIAN ALE WILL BE BEAMED ABOARD");
		comms_transmission(sb, ": YOUR VESSEL IN CELEBRATION OF YOUR SUCCESSFUL");
		comms_transmission(sb, ": MISSION!");
		comms_transmission(sb, ": ");
	else
		register_timer_callback("check_derelict_pos", 10, 0); 
	end	
end

register_timer_callback("starbase_message", 10, 0);
register_timer_callback("mayday_message", 60 * 10, 0);
register_timer_callback("check_trap", 10, 0);
register_timer_callback("check_derelict_pos", 10, 0); 

