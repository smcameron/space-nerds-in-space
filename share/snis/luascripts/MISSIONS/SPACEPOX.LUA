-- a remote station suffers a spacepox outbreak
--
-- SPACE POX
--
-- Locate and deliver SPACE POX VACCINE to save the crew
-- of the ICARUS station orbiting planet BONX from the
-- SPACE POX which has broken out and is slowly killing
-- them off one by one in a series of unlikely "accidents"
--
-- This mission can be kind of difficult
-- The biggest difficulty is finding the space pox vaccine
-- unless you know the trick.  Then, the zarkon dreadknights
-- are liable to get you unless you make a quick getaway, but
-- your warp drive will probably not help you.

-- First, clear the board
clear_all();

-- Where the player starts
startx = 101000;
starty = 6000;
startz = 0;
player_with_vaccine = nil

-- how many survivors on the station
survivors = 8;

-- how many zarkons to attack
number_of_zarkons = 3;
zarkon_check_interval = 10 * 15; -- check for zarkon attack every 15 seconds

vaccine_delivered = false;

-- Interval between distress calls from ICARUS station is 120 seconds
distress_call_interval = 10 * 120;

derelictx = 512070 + math.random(40000) - 20000;
derelicty = -127000 + math.random(40000) - 20000;
derelictz = 257030 + math.random(40000) - 20000;

-- Faction values (must match share/snis/factions.txt)
zarkon = 3;
vekkazi = 4;

-- Ship type values (must match share/snis/ship_types.txt)
disruptor = 12;
dragonhawk = 10;
dreadknight = 18;

-- find the player and move him where we want him to start.
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	for i, player_id in ipairs(player_ids) do
		move_object(player_id, startx, starty, startz + (i * 100));
		reset_player_ship(player_id);
		set_faction(player_id, 0); -- set player faction to neutral so only zarkons fight
	end
end

-- Create our macguffin -- a derelict and a cargo container of spacepox vaccine
spacepox_vaccine = add_commodity("SPACEPOX VACCINE", "MEGADOSES", "ORGANIC MATTER",
					100.0, 02, 1.0, 0.1, 0.1, 0.7, 0.0);
derelict = add_derelict("LUNAE ORTUS", derelictx, derelicty, derelictz, dragonhawk, vekkazi);
set_object_velocity(derelict, 0, 0, 0);
cargocontainer = add_cargo_container(derelictx + 150, derelicty + 190, derelictz + 500, 0, 0, 0);
set_commodity_contents(cargocontainer, spacepox_vaccine, 100);

x = 2900;
y = 12000;
z = 290000;

-- locations and radii of our planets:

tobbraxx = 250000 + math.random(30000);
tobbraxy = 12000 + math.random(30000);
tobbraxz = 352000 + math.random(30000);
tobbraxr = 5100

scybonx = 22900 + math.random(30000);
scybony = 15000 + math.random(30000);
scybonz = 12000 + math.random(30000);
scybonr = 4900

-- Name and location of station and planets affected by SPACE POX
station = "ICARUS STATION"
bonxname = "BONX"
bonxx = derelictx + math.random(200000) - 100000;
bonxy = derelicty + math.random(200000) - 100000;
bonxz = derelictz + math.random(200000) - 100000;
bonxr = 6100

-- Function to create a planet and an accompanying starbase
function add_planet_plus_starbase(name, x, y, z, r, starbase_number)
	sbx = math.random(r / 2) + r;
	sby = math.random(r / 2) + r;
	sbz = math.random(r / 2) + r;
	if math.random(100) < 50 then
		sbx = -sbx;
	end
	if math.random(100) < 50 then
		sby = -sby;
	end
	if math.random(100) < 50 then
		sbz = -sbz;
	end
	sbx = sbx + x;
	sby = sby + y;
	sbz = sbz + z;
	p = add_planet(name, x, y, z, r);
	sb = add_starbase(sbx, sby, sbz, starbase_number);
	return p, sb;
end

-- Create our planets and their starbases
bonx, bonx_starbase = add_planet_plus_starbase(bonxname, bonxx, bonxy, bonxz, bonxr, 1);
scybon, scybon_starbase = add_planet_plus_starbase("SCYBON", scybonx, scybony, scybonz, scybonr, 2);
tobbrax, tobbrax_starbase = add_planet_plus_starbase("TOBBRAX", tobbraxx, tobbraxy, tobbraxz, tobbraxr, 3);

-- Add in some nebulas
n1 = add_nebula("INSANITY-MAJORIS", x + 5000, y - 100, z - 1500, 5000);
n2 = add_nebula("INSANITY-MINORIS", x + 500, y - 200, z - 3000, 3000);
n3 = add_nebula("INSANITY", x + 8000, y - 700, z - 2500, 2000);

shipname = { "HEBBO", "AMZAM", "IVIRAM", "CHABBER", "NAKEL" }
zarkon_ship = { -1, -1, -1, -1, -1 }

wormhole1x = derelictx + 1500;
wormhole1y = derelicty + 1000;
wormhole1z = derelictz + 100;

wormhole2x = scybonx + scybonr * 2;
wormhole2y = scybony + 500;
wormhole2z = scybonz + 700;

-- Give the player a wormhole out of a bad situation
wormhole1, wormhole2 = add_wormhole_pair(wormhole1x, wormhole1y, wormhole1z, wormhole2x, wormhole2y, wormhole2z);

-- Function to create an asteroid field
function create_asteroid_field(number_of_asteroids, x, y, z, radius)
	for i = 1, number_of_asteroids do
		repeat
			dx = math.random(radius * 2) - radius;
			dy = math.random(radius * 2) - radius;
			dz = math.random(radius * 2) - radius;
			r2 = dx * dx + dy * dy + dz * dz
		until r2 < radius * radius
		add_asteroid(x + dx, y + dy, z + dz);
	end
end

function mission_update(text)
	for i, player_id in ipairs(player_ids) do
		show_timed_text(player_id, 10, "SPACEPOX\n" .. text);
		text_to_speech(player_id, text);
	end
end

function all_hands_lost()
	mission_update(
		"ALL HANDS ABOARD " .. station .. " ORBITING " .. bonxname .. "\n" ..
		"WERE LOST TO SPACE POX\n");
end

function you_brought_the_vaccine()
	vaccine_delivered = true;
	if survivors <= 0 then
		all_hands_lost();
	else
		mission_update(
			"YOU HAVE SAVED " .. 100.0 * (survivors / 8.0) .. " PERCENT OF\n" ..
			"THE CREWMEMBERS FROM THE SPACE POX\n");
	end
end

function wheres_the_vaccine()
	if (survivors > 0) then
		for i, player_id in ipairs(player_ids) do
			text_to_speech(player_id, "Incoming transmission from Icarus station");
		end
		comms_transmission(bonx_starbase, "------------------");
		comms_transmission(bonx_starbase, "WHAT ARE YOU DOING HERE WITHOUT THE VACCINE?");
		comms_transmission(bonx_starbase, "PLEASE GO OUT AND FIND THE SPACEPOX VACCINE");
		comms_transmission(bonx_starbase, "AND RETURN HERE WHEN YOU HAVE IT");
		comms_transmission(bonx_starbase, "------------------");
	else
		all_hands_lost();
	end
end

-- check if the player has the spacepox vaccine in one of the cargo bays
function player_has_vaccine(player)
	ncargo_bays = get_ship_attribute(player, "ncargo_bays");
	print("ncargo bays = " .. ncargo_bays);
	if ncargo_bays == nil then
		return false;
	end
	if ncargo_bays <= 0 then
		return false;
	end
	for i = 0, ncargo_bays - 1, 1 do
		print("getting cargo bay '" .. string.format("cargo[%d[.contents.item", i));
		cargo = get_ship_attribute(player, string.format("cargo[%d].contents.item", i));
		print("cargo[" .. i .. "] = " .. cargo .. " vs. vaccine = " .. spacepox_vaccine);
		if (cargo == spacepox_vaccine) then
			print("MATCH");
			return true;
		end
	end
	return false;
end

-- Function gets called when player docks at a starbase
function player_docked_callback(player_id, station_id)
	print("player docked callback player: " .. player_id .. ", station: " .. station_id .. " vs. " .. bonx_starbase);
	if station_id == bonx_starbase then
		if player_has_vaccine(player_id) then
			print("you brought the vaccine");
			register_timer_callback("you_brought_the_vaccine", 50, 0);
		else
			print("you did not bring the vaccine");
			register_timer_callback("wheres_the_vaccine", 50, 0);
		end
	 end
end

-- Function to introduce the mission
function intro()
	for i, player_id in ipairs(player_ids) do
		show_timed_text(player_id, 10,
			"SPACEPOX\n" ..
			"CAN YOU SAVE THE CREW OF " .. station .. "\n" ..
			"STATION BEFORE THEY ALL SUCCUMB\n" ..
			"TO THE DREADED SPACE POX?\n");
	end
end


-- Generic distress call from so-and-so complaining that the space pox has
-- killed his predecessor in some unlikely way.
function distress_call(rank, firstname, lastname, predecessor, died)
	if vaccine_delivered then -- nothing to complain about
		return;
	end
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "Incoming transmission from Icarus station");
	end
	comms_transmission(bonx_starbase, "------------------");
	comms_transmission(bonx_starbase, "THIS IS " .. rank .. " " .. firstname .. " " .. lastname);
	comms_transmission(bonx_starbase, "ABOARD " .. station .. " ORBITING " .. bonxname);
	comms_transmission(bonx_starbase, predecessor .. " CAME DOWN WITH THE SPACE POX");
	comms_transmission(bonx_starbase, "AND " .. died);
	comms_transmission(bonx_starbase, "THE LAST KNOWN POSITION OF THE LUNAE ORTUS");
	comms_transmission(bonx_starbase, "WAS (" ..
			derelictx .. ", " .. derelicty .. ", " .. derelictz .. ").");
	comms_transmission(bonx_starbase, "IN THE MIDDLE OF THE GREATER ZORLACC ASTEROID FIELD");
	comms_transmission(bonx_starbase, "PLEASE HURRY WITH THE SPACE POX VACCINES");
	comms_transmission(bonx_starbase, rank .. " " .. lastname .. " OUT");
	comms_transmission(bonx_starbase, "------------------");
end

-- First distress call from ICARUS station
function distress_call1()
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "Incoming transmission from Icarus station");
		print("Incoming 1");
	end
	comms_transmission(bonx_starbase, "------------------");
	comms_transmission(bonx_starbase, "THIS IS ADMIRAL ASTON PIERCE");
	comms_transmission(bonx_starbase, "ABOARD " .. station .. " ORBITING " .. bonxname);
	comms_transmission(bonx_starbase, "IT SEEMS WE HAVE AN OUTBREAK OF THE SPACE POX");
	comms_transmission(bonx_starbase, "SPACEMAN FIRST CLASS ERICCSON WAS THE FIRST CASE");
	comms_transmission(bonx_starbase, "WE DON'T YET KNOW IF THERE WILL BE MORE CASES");
	comms_transmission(bonx_starbase, "THE LUNAE ORTUS WAS SUPPOSED TO ARRIVE WITH");
	comms_transmission(bonx_starbase, "VACCINES BUT WE HAVE LOST CONTACT WITH HER");
	comms_transmission(bonx_starbase, "THE LAST KNOWN POSITION OF THE LUNAE ORTUS");
	comms_transmission(bonx_starbase, "WAS (" ..
			derelictx .. ", " .. derelicty .. ", " .. derelictz .. ")");
	comms_transmission(bonx_starbase, "IN THE MIDDLE OF THE GREATER ZORLACC ASTEROID FIELD");
	comms_transmission(bonx_starbase, "WE ARE IN DESPERATE NEED OF THAT MEDICINE");
	comms_transmission(bonx_starbase, "ADMIRAL PIERCE OUT.");
	comms_transmission(bonx_starbase, "------------------");
	register_timer_callback("distress_call2", distress_call_interval, 0);
	survivors = survivors - 1;
end

-- Subsequent distress calls from ICARUS station
function distress_call2()
	distress_call("CAPTAIN", "LANCE", "BROMAN", "ADMIRAL PIERCE",
			"ACCIDENTALLY EJECTED HIMSELF OUT OF AN AIRLOCK");
	register_timer_callback("distress_call3", distress_call_interval, 0);
	survivors = survivors - 1;
end

function distress_call3()
	distress_call("COMMANDER", "ACE", "JACKMAN", "CAPTAIN BROMAN",
			"ACCIDENTALLY WENT FOR A SPACE WALK WITHOUT HIS SPACE HELMET");
	register_timer_callback("distress_call4", distress_call_interval, 0);
	survivors = survivors - 1;
end

function distress_call4()
	distress_call("LIEUTENANT COMMANDER", "JETT", "STEEL", "COMMANDER JACKMAN",
			"ACCIDENTALLY LAUNCHED HIMSELF OUT A TORPEDO TUBE");
	register_timer_callback("distress_call5", distress_call_interval, 0);
	survivors = survivors - 1;
end

function distress_call5()
	distress_call("LIEUTENANT", "MAX", "RANGER", "LIEUTENANT COMMANDER STEEL",
			"ACCIDENTALLY TOOK A NAP INSIDE THE REACTOR CORE");
	register_timer_callback("distress_call6", distress_call_interval, 0);
	survivors = survivors - 1;
end

function distress_call6()
	distress_call("LIEUTENANT JUNIOR GRADE", "RICK", "STONEMAN", "LIEUTENANT RANGER",
			"ACCIDENTALLY ATE A POISONOUS BLOWFISH OUT OF THE AQUARIUM");
	register_timer_callback("distress_call7", distress_call_interval, 0);
	survivors = survivors - 1;
end

function distress_call7()
	distress_call("ENSIGN", "WESLEY", "ROBERTS", "LIEUTENANT JUNIOR GRADE STONEMAN",
			"ACCIDENTALLY WAS STUNG TO DEATH BY SCORPIONS");
	register_timer_callback("distress_call8", distress_call_interval, 0);
	survivors = survivors - 1;
end

function distress_call8()
	distress_call("SPACEMAN FIRST CLASS", "JOE", "SIKSPACH", "ENSIGN ROBERTS",
		"A SNIPER TOOK HIM OUT");
	comms_transmission(bonx_starbase, "I AM NOT FEELING SO WELL MYSELF ACTUALLY");
	comms_transmission(bonx_starbase, "HANG ON A SEC, THERES A RATTLESNAKE IN");
	comms_transmission(bonx_starbase, "THE CONTROL ROOM I HAVE TO DEAL WITH");
	survivors = survivors - 1;
end

function maintain_zarkon_focus()
	if (player_with_vaccine ~= nil) then
		for i = 1, number_of_zarkons do
			ai_push_attack(zarkon_ship[i], player_with_vaccine);
		end
	end
	register_timer_callback("maintain_zarkon_focus", 20, 0);
end

function send_in_the_zarkons(player_id)
	print("Sending in the zarkons");
	px,py,pz = get_object_location(player_id);
	zx = px + math.random(4000) - 2000;
	zy = py + math.random(4000) - 2000;
	zz = pz + math.random(4000) - 2000;
	for i = 1, number_of_zarkons do
		zarkon_ship[i] = add_ship(shipname[i], zx, zy, zz, dreadknight, zarkon, 0);
		ai_push_attack(zarkon_ship[i], player_id);
		zy = zy + 250;
	end
	text_to_speech(player_id, "Warning, Zarkon scanning beam has been detected.");
	-- Mess up the player's warp drive to make it harder to run away
	set_player_damage(player_id, "warp", 255); -- 3 times because of the way
	set_player_damage(player_id, "warp", 255); -- damage is distributed
	set_player_damage(player_id, "warp", 255);
	register_timer_callback("maintain_zarkon_focus", 20, 0);
end

-- check to see if the space vaccine has been acquired and if so send in the zarkons
function check_for_zarkon_attack()
	print("checking for zarkon attack");
	for i, player_id in ipairs(player_ids) do
		if player_has_vaccine(player_id) then
			-- player has vaccine, send in the zarkons
			send_in_the_zarkons(player_id);
			player_with_vaccine = player_id;
			-- do not register the callback to check for zarkon attack again
			return;
		end
	end
	
	print("No zarkon attack this time");
	-- player doesn't have vaccine, check for zarkon attack again in a little while
	register_timer_callback("check_for_zarkon_attack", zarkon_check_interval, 0);
end

function add_a_bunch_of_neutral_ships()
	for i = 1, 30 do
		id = add_random_ship();
		set_faction(id, 0);
	end
end

-- Greater Zorlacc asteroid field
create_asteroid_field(200, derelictx + math.random(30000) - 15000,
			derelicty + math.random(30000) - 15000,
			derelictz + math.random(30000) - 15000, 150000);
-- Lesser Zorlacc asteroid field
create_asteroid_field(50, derelictx + 170000, derelicty + 50000, derelictz, 50000);

-- Create a very large, very sparse asteroid "field"
create_asteroid_field(50, scybonx, scybony, scybonz, 500000);


add_a_bunch_of_neutral_ships()

register_callback("player-docked-event", "player_docked_callback");
register_timer_callback("distress_call1", 150, 0);
register_timer_callback("check_for_zarkon_attack", zarkon_check_interval, 0);

intro();

