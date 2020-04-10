-- transport prince arystid to its wedding
--
-- ROYAL WEDDING
--
-- Securely carry the Prince ARYSTID to its wedding with the Princess MADUSA
-- on planet VAYR, along with the necessary resources for the celebration.
--

arg = {...};

adjx = -300000;
adjz = -300000;

if ((#arg >= 2) and (tonumber(arg[2]) == 1)) then
	debug = true;
	demon_print('debug mode');
else
	debug = false;
end

-- First, clear the board
clear_all();

-- Mission data
queen_planet_name	= "FARANG-7";
queen_starbase_number	= 69;
queen_name		= "ANUK";
prince_name		= "ARYSTID";
bodyguard_name		= "KUMCHI";
prince_starbase_number	= 29;
prince_planet_name	= "GRYN";
wedding_starbase_number	= 26;
wedding_planet_name	= "VAYR";
princess_name		= "MADUSA";
pre_payment		= 1000;
post_payment		= 10000;

-- Faction values (must match share/snis/factions.txt)
schaazbaut	= 2;
zarkon		= 3;
vekkazi		= 4;

-- Planet types
earthlike	= 0;
gasgiant	= 1;
rocky		= 2;

queen_faction		= 2;
queen_faction_name	= "Schaazbaut";
princess_faction	= 4;
princess_faction_name	= "Vekkazi";

-- Ship type values (must match share/snis/ship_types.txt)
battlestar	= 5;
dragonhawk	= 10;
disruptor	= 12;
conqueror	= 14;
scrambler	= 15;
swordfish	= 16;
dreadknight	= 18;

-- Universe size
XKNOWN_DIM	= 600000.0;
YKNOWN_DIM	= (XKNOWN_DIM * 0.2);
ZKNOWN_DIM	= XKNOWN_DIM;

-- All known ships
ships = {}

-- Create commodities
-- add_commodity(category, name, unit, scans_as,
-- 		base_price, volatility, legality, odds);
wedding_cake = add_commodity("FOOD", "WEDDING CAKE", "BOXES", "FOOD",
				10.0, 02, 1.0, 0.0);
dead_bodyguard = add_commodity("BEINGS", "BODYGUARD", "DEAD", "ORGANIC MATTER",
				0.0, 0, 1.0, 0.0);

function random_point_at_distance(x, y, distance)
	-- Random angle betweem 0 and pi/2
	angle = math.random() * .5 * math.pi;

	-- Convert to offsets
	x_offset = math.floor(math.cos(angle) * distance);
	y_offset = math.floor(math.sin(angle) * distance);

	return x + x_offset, y + y_offset;
end

-- Function to create an asteroid field
function create_asteroid_field(number_of_asteroids, x, y, z, radius, speed)
	for i = 1, number_of_asteroids do
		repeat
			dx = math.random(-radius, radius);
			dy = math.random(-radius, radius);
			dz = math.random(-radius, radius);
			r2 = dx * dx + dy * dy + dz * dz;
		until r2 < radius * radius
		id = add_asteroid(x + dx, y + dy, z + dz);
		if ((id >= 0) and (speed >= 0)) then
			set_asteroid_speed(id, speed);
		end
	end
end

-- Locations and radii of our planets:

-- Queen planet is at the center of queen_faction influence sphere
-- Schaazbaut
-- 150000 0 150000
qx = 150000 + math.random(1000) + adjx;
qy = 0 + math.random(1000);
qz = 150000 + math.random(1000) + adjz;
qt = gasgiant;
qr = 8000; -- Huge empire planet

-- Wedding planet is near the center of princess_faction influence sphere
-- Vekkazi
-- 300000 0 0
wx = 300000 - math.random(20000) + adjx;
wy = math.random(20000);
wz = math.random(20000) + adjz;
wt = earthlike;
wr = 4000;

-- Prince is far from wedding
px, pz = random_point_at_distance(wx, wz, 500000);
py = qy;
--~ pt = rocky;
--~ pr = 1000; -- Small vacation planet
pt = earthlike;
pr = 4000;

-- Function to create a planet and an accompanying starbase
function add_planet_plus_starbase(name, x, y, z, r, s, t, starbase_number)
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
	p = add_planet(name, x, y, z, r, s, t);
	sb = add_starbase(sbx, sby, sbz, starbase_number);
	return p, sb;
end

-- Find the player and move him where we want him to start.
player_ids = get_player_ship_ids();
player_name = "Unknown Player";
if (player_ids[1]) then
	move_object(player_ids[1], qx + qr + 2000, qy + qr + 2000, qz + qr + 2000);
	player_name = get_object_name(player_ids[1]);
	reset_player_ship(player_ids[1]);
	--~ set_faction(player_ids[1], queen_faction); -- set player faction to queen_faction
	set_faction(player_ids[1], 0);
else
	print("Mission called with no player, exiting");
	demon_print("Mission called with no player, exiting");
	exit();
end

-- Create our planets and their starbases

queen_planet, queen_starbase		= add_planet_plus_starbase(queen_planet_name, qx, qy, qz, qr, 2, qt, queen_starbase_number);
prince_planet, prince_starbase		= add_planet_plus_starbase(prince_planet_name, px, py, pz, pr, 1, pt, prince_starbase_number);
wedding_planet, wedding_starbase	= add_planet_plus_starbase(wedding_planet_name, wx, wy, wz, wr, 2, wt, wedding_starbase_number);


set_planet_description(queen_planet,
	"Planet of " .. queen_faction_name .. " people living under the reign of beloved Queen " .. queen_name .. ".\n" ..
	"Her Majesty transformed this planet into a healthy living place\n" ..
	" and an exemplary prosperous industrial world.");
set_planet_government(queen_planet, 3) -- set queen planet to dictatorship
set_planet_tech_level(queen_planet, 5) -- space age
set_planet_economy(queen_planet, 0) -- Rich Industrial

set_planet_description(prince_planet,
	"Small planet of farmers which quickly became the\n" ..
	" number one destination for " .. queen_faction_name .. " tourists\n" ..
	" thanks to its warm weather and beautiful nature.");
set_planet_government(prince_planet, 7) -- set vacation planet to corporate state
set_planet_tech_level(prince_planet, 3) -- Silicon age
set_planet_economy(prince_planet, 6) -- Average Agricultural

-- Create the queen ship

queen_ship = add_ship("ROYAL MOTHERSHIP", qx + qr + 1000, qy + qr + 1000, qz, battlestar, queen_faction, 0);
ships[queen_ship] = true;
set_faction(queen_starbase,	queen_faction);
set_faction(prince_starbase,	queen_faction);
set_faction(wedding_starbase,	princess_faction);

-- Create the passengers
prince		= 0;
princess	= 1;
bodyguard	= 2;
create_passenger(princess,	"Princess " .. princess_name,	wedding_starbase,	wedding_starbase, 0);
create_passenger(prince,	"Prince " .. prince_name,	prince_starbase,	wedding_starbase, 0);
create_passenger(bodyguard,	bodyguard_name,			prince_starbase,	wedding_starbase, 0);

-- Cargo
wedding_cargo = {
	{wedding_cake, 			10},
	{lookup_commodity("Champagne"),	100},
	{lookup_commodity("wine"),	100},
	{lookup_commodity("gold"),	1000}
}

-- Add in some nebulas
nx = 2900 + adjx;
ny = 12000;
nz = 290000 + adjz;
n1 = add_nebula("INSANITY-MAJORIS", nx + 5000, ny - 100, nz - 1500, 5000);
n2 = add_nebula("INSANITY-MINORIS", nx + 500, ny - 200, nz - 3000, 3000);
n3 = add_nebula("INSANITY", nx + 8000, ny - 700, nz - 2500, 2000);


-- Secret base setup

secret_channel	= math.random(100, 5000);
secret_password	= "BELUGA";
secret_base	= {};
secret_base.number = 42;
secret_base.x, secret_base.z = random_point_at_distance(px, pz, 50000);
secret_base.y = py;

-- Add some stuff where the secret base is supposed to be hidden

create_asteroid_field(50, secret_base.x, secret_base.y, secret_base.z, 7000, 0);
secret_base.nebula = add_nebula("BELUGA", secret_base.x, secret_base.y, secret_base.z, 4000);

-- Secret base lives in dangerous places
add_spacemonster(generate_name(), secret_base.x + 1000, secret_base.y, secret_base.z);
add_spacemonster(generate_name(), secret_base.x + 2000, secret_base.y, secret_base.z);


-- Rebels

rebel_ship		= {};
alive			= {};
if (debug) then
	number_of_rebels	= 1; -- for debug
	set_variable("MISSILE_FIRE_CHANCE", "0"); -- for debug
else
	number_of_rebels	= 4;
	set_variable("MISSILE_FIRE_CHANCE", "2");
end

ending1_triggered = false;

queen_needed = true;

queen_attack_started = false;

-- Fill up cargo bays with resources for the wedding
function load_player_cargo()
	for i, cargo in pairs(wedding_cargo) do
		set_commodity_contents(player_ids[1], cargo[1],	cargo[2],	i - 1);
	end
end

-- Check the resources are still on board
function check_player_cargo()
	player_cargo = {0, 0, 0, 0};
	for i = 0, 7 do
		item	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.item");
		qty	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.qty");
		for j, cargo in pairs(wedding_cargo) do
			if (cargo[1] == item) then
				player_cargo[j] = player_cargo[j] + qty;
			end
		end
	end
	for i, qty in pairs(player_cargo) do
		if (qty < wedding_cargo[i][2]) then
			return false;
		end
	end
	return true;
end

function collect_player_cargo()
	for i = 0, 7 do
		item	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.item");
		qty	= get_ship_attribute(player_ids[1], "cargo[" .. i .. "].contents.qty");
		for j, cargo in pairs(wedding_cargo) do
			if (cargo[1] == item) then
				set_commodity_contents(player_ids[1], -1, 0, i);
			end
		end
	end
end

function get_first_empty_cargo_bay(shipid)
	for i = 0, 7 do
		item	= get_ship_attribute(shipid, "cargo[" .. i .. "].contents.item");
		qty	= get_ship_attribute(shipid, "cargo[" .. i .. "].contents.qty");
		if ((item < 0) or (qty <= 0)) then
			return i;
		end
	end
	-- TODO return -1 and handle this case properly
	return 7;
end

-- Function to introduce the mission
function intro()
	show_timed_text(-1, 30,
		"ROYAL WEDDING\n" ..
		"\nYou accepted a juicy contract from Queen " .. queen_name ..
		"\n to tranport her son, Prince " .. prince_name ..
		"\n to his wedding to Princess " .. princess_name .. " of the " .. princess_faction_name ..
		"\n in 2 hours on " .. wedding_planet_name ..
		"\n along with materials for the wedding.");
end

function add_random_planets(nplanets)
	-- More permissive than snis_server
	limit = XKNOWN_DIM / 10;
	for i = 1, nplanets do
		count = 0;
		repeat
			x = math.random(0, XKNOWN_DIM) + adjx;
			y = math.random(-YKNOWN_DIM, YKNOWN_DIM);
			z = math.random(0, ZKNOWN_DIM) + adjz;
			count = count + 1;
		until ((count > 100) or (not too_close_to_planet_or_sun(x, y, z, limit)));
		r = math.random(5000, 7000);
		add_planet_plus_starbase(generate_name(), x, y, z, r, math.random(0, 2), -1, i + 10);
	end
end

-- Paths
function queen_wait_near_queen_planet()
	ai_push_patrol(queen_ship, 2, qx + qr + 1000, qy + qr + 1000, qz, qx + qr + 5000, qy + qr + 5000, qz);
end

function queen_go_to_wedding()
	ai_push_patrol(queen_ship, 2, qx + qr + 1000, qy + qr + 1000, qz, wx + wr + 1000, wy + wr + 1000, wz);
end

function queen_wait_near_wedding()
	ai_push_patrol(queen_ship, 2, wx + wr + 1000, wy + wr + 1000, wz, wx + wr + 5000, wy + wr + 5000, wz);
end

-- First queen instructions
function queen_message()
	text_to_speech(player_ids[1], "Incoming transmission from Royal Mothership");
	comms_transmission(queen_ship, "---");
	comms_transmission(queen_ship, "Thank you for accepting this mission");
	comms_transmission(queen_ship, " of the utmost importance.");
	comms_transmission(queen_ship, "You will need to fetch the Prince from " .. prince_planet_name);
	comms_transmission(queen_ship, " where he was on holidays.");
	comms_transmission(queen_ship, "By the time you get there he will be waiting");
	comms_transmission(queen_ship, " at the orbiting station, SB-" .. prince_starbase_number .. ".");
	comms_transmission(queen_ship, "His bodyguard will accompany him to ensure his safety.");
	comms_transmission(queen_ship, "Supplies for the wedding have been transported into your");
	comms_transmission(queen_ship, " cargo bay, do not forget to deliver them as well.");
	comms_transmission(queen_ship, "As agreed upon, you get $" .. pre_payment .. " now and $" .. post_payment);
	update_player_wallet(player_ids[1], pre_payment);
	comms_transmission(queen_ship, " once Prince " .. prince_name .. " is safely on " .. wedding_planet_name .. ".");
	comms_transmission(queen_ship, "---");
end

-- First prince message
function prince_message()
	text_to_speech(player_ids[1], "Incoming transmission from Starbase " .. prince_starbase_number);
	comms_transmission(prince_starbase, "---");
	comms_transmission(prince_starbase, "Hello, this is Prince " .. prince_name);
	comms_transmission(prince_starbase, "I'm waiting for you at SB-" .. prince_starbase_number);
	comms_transmission(prince_starbase, "Orbiting planet " .. prince_planet_name);
	comms_transmission(prince_starbase, "Meet me there");
	comms_transmission(prince_starbase, "---");
end

-- Function gets called when player docks at a starbase
function player_docked_callback(player_id, station_id)
	if station_id == prince_starbase then
		register_timer_callback("board_prince", 50, 0);
	elseif station_id == wedding_starbase then
		if check_prince_aboard() then
			ending1();
		else
			comms_transmission(wedding_starbase, "---");
			comms_transmission(wedding_starbase, "Where is the prince?");
			comms_transmission(wedding_starbase, "We're waiting him for the wedding!");
			comms_transmission(wedding_starbase, "---");
		end
	elseif station_id == secret_base.sb then
		if check_prince_aboard() then
			ending2();
		else
			comms_transmission(station_id, "---");
			comms_transmission(station_id, "Where is the prince?");
			comms_transmission(station_id, "---");
		end
	end
end

function delete_passenger(passenger)
	create_passenger(passenger, generate_character_name(), queen_starbase, prince_starbase, 50);
end

-- Board prince and bodyguard
function board_prince()
	if check_prince_aboard() then
		comms_channel_transmit("Prince " .. prince_name, 0,	"Why are we docking here again?");
		comms_channel_transmit("Prince " .. prince_name, 0,	"We need to get to " .. wedding_planet .. "!");
		return
	end
	-- TODO: check there is enough places to board prince and bodyguard
	set_passenger_location(prince,		player_ids[1]);
	set_passenger_location(bodyguard,	player_ids[1]);
	text_to_speech(player_ids[1], "2 passengers have boarded " .. player_name);
	comms_channel_transmit("Prince " .. prince_name, 0,	"Hello, I am Prince " .. prince_name .. ".");
	comms_channel_transmit("Prince " .. prince_name, 0,	"Happy to be on board of " .. player_name .. ".");
	comms_channel_transmit("Prince " .. prince_name, 0,	"This is my bodyguard, " .. bodyguard_name .. ".");
	comms_channel_transmit(bodyguard_name, 0, 		"*Grunts*");
	comms_channel_transmit("Prince " .. prince_name, 0,	"Well, he's not much for talking.");
	register_timer_callback("prince_remark1", 400, 0);
	register_proximity_callback("warp_failure", player_ids[1], wedding_starbase, 300000);
	register_proximity_callback("prince_panic", player_ids[1], wedding_planet, 20000);
end

function prince_remark1()
	if check_player_cargo() then
		comms_channel_transmit("Prince " .. prince_name, 0,	"Good, I see you have the supplies for the party.");
		comms_channel_transmit("Prince " .. prince_name, 0,	"This will be a great day, everything needs to be perfect.");
		comms_channel_transmit("Prince " .. prince_name, 0,	"En route to " .. wedding_planet_name .. "!");
	else
		comms_channel_transmit("Prince " .. prince_name, 0,	"You lost the supplies for the party?");
		comms_channel_transmit("Prince " .. prince_name, 0,	"My mother will not be happy.");
		comms_channel_transmit("Prince " .. prince_name, 0,	"You better get me to " .. wedding_planet_name .. " on time!");
	end
end

function check_prince_aboard()
	return (get_passenger_location(prince) == player_ids[1]);
end

function add_a_bunch_of_random_ships(howmany, faction)
	for i = 1, howmany do
		id = add_random_ship();
		set_faction(id, faction);
		ships[id] = true;
	end
end

function warp_failure()
	if (get_passenger_location(prince) ~= player_ids[1]) then
		return
	end
	px,py,pz = get_object_location(player_ids[1]);
	add_explosion(px, py, pz, 1, 60, 30, 24, 0);
	set_player_damage(player_ids[1], "warp", 255); -- 3 times because of the way
	set_player_damage(player_ids[1], "warp", 255); -- damage is distributed
	set_player_damage(player_ids[1], "warp", 255);
	set_player_damage(player_ids[1], "warp", 255);
	set_commodity_contents(player_ids[1], dead_bodyguard, 1, get_first_empty_cargo_bay(player_ids[1]));
	delete_passenger(bodyguard);
	play_sound(player_ids[1], "sounds/big_explosion.ogg");
	comms_channel_transmit("Technician", 0, "There was a fire in the warp engine, it's badly damaged!");
	comms_channel_transmit("Technician", 0, "The bodyguard was killed by the explosion!");
	register_timer_callback("prince_remark2", 200, 0);
	register_timer_callback("rebels_attack", 500, 0);
end

function prince_remark2()
	if not check_prince_aboard() then
		return;
	end
	comms_channel_transmit("Prince " .. prince_name, 0,	"What happened?");
	comms_channel_transmit("Prince " .. prince_name, 0,	"Is this ship even cleared for flying?");
	comms_channel_transmit("Prince " .. prince_name, 0,	bodyguard_name .. " was my best bodyguard,");
	comms_channel_transmit("Prince " .. prince_name, 0,	" now I have to hire another!");
	comms_channel_transmit("Prince " .. prince_name, 0,	"You better fix this ASAP and get me to my wedding!");
end

function prince_remark3()
	if not check_prince_aboard() then
		return;
	end
	-- It might be that the prince can say this before the players know what he is talking about.
	-- Maybe we could use object-hit-event to trigger this some short time after the player is hit for
	-- the first time.  However, object-hit-event might need to be expanded to include missiles.
	comms_channel_transmit("Prince " .. prince_name, 0,	"How can they dare to attack us?");
	comms_channel_transmit("Prince " .. prince_name, 0,	"Kill these vermin!");
end

function rebels_attack()
	px,py,pz = get_object_location(player_ids[1]);
	zx = px + math.random(-9000, 9000);
	zy = py + math.random(-9000, 9000);
	zz = pz + math.random(-9000, 9000);
	shiptype = swordfish;
	--~ shiptype = scrambler; -- for debug
	for i = 1, number_of_rebels do
		rebel_ship[i] = add_ship(generate_ship_name(), zx, zy, zz, shiptype, queen_faction, 0);
		alive[rebel_ship[i]] = true;
		ships[rebel_ship[i]] = true;
		add_bounty(rebel_ship[i], "Rebellion", 5000, queen_starbase);
		zy = zy + 250;
		if (i > 1) then
			shiptype = scrambler
		end
	end
	register_timer_callback("maintain_rebel_focus", 60, 0);
	register_timer_callback("rebel_transmission", 90, 0);
	register_timer_callback("prince_remark3", 200, 0);
	register_callback("object-death-event", "object_death_callback");
end

function maintain_rebel_focus()
	continue = 0;
	for i = 1, number_of_rebels do
		if (alive[rebel_ship[i]]) then
			ai_push_attack(rebel_ship[i], player_ids[1]);
			continue = 1;
		end
	end
	if (continue == 1) then
		register_timer_callback("maintain_rebel_focus", 20, 0);
	else
		comms_channel_transmit("Prince " .. prince_name, 0,	"Good riddance!");
	end
end

function rebel_transmission()
	comms_transmission(rebel_ship[1], "Death to Prince " .. prince_name .. "!");
	comms_transmission(rebel_ship[2], "Queen " .. queen_name .. " is killing our families!");
	comms_transmission(rebel_ship[3], "Freedom for " .. queen_faction_name .. "!");
end

function object_death_callback(oid)
	if (alive[oid] == true) then
		alive[oid] = false;
	end
	if (ships[oid]) then
		ships[oid] = nil;
	end
	if (queen_needed and (oid == queen_ship)) then
		-- Queen ship is needed till the end of the mission
		queen_ship = add_ship("ROYAL MOTHERSHIP", wx + wr + 1000, wy + wr + 1000, wz, battlestar, queen_faction, 0);
		ships[queen_ship] = true;
		queen_wait_near_wedding();
	end
	-- FIXME we should probably also check that starbases are not destroyed
end

-- Prince panic

function prince_panic()
	if not check_prince_aboard() then
		return;
	end
	comms_channel_transmit("Prince " .. prince_name, 0,	"Please turn around!");
	comms_channel_transmit("Prince " .. prince_name, 0,	"I do not want this wedding!");
	comms_channel_transmit("Prince " .. prince_name, 0,	"My mother is just doing this for more power!");
	comms_channel_transmit("Prince " .. prince_name, 0,	"I thought setting the engine on fire would be enough!");
	register_timer_callback("queen_impatience", 300, 0);
	register_proximity_callback("prince_panic2", player_ids[1], wedding_starbase, 5500);
end

function queen_impatience(queen_wait)
	if (ending1_triggered) then
		return;
	end
	distance_to_starbase = object_distance(player_ids[1], wedding_starbase);
	if (((queen_wait > 3) and (distance_to_starbase > 5000)) or (distance_to_starbase > 22000)) then
		-- If we are too far or waited for too long
		queen_attack();
	else
		comms_transmission(queen_ship, "What are you waiting for?");
		comms_transmission(queen_ship, "Dock at SB-" .. wedding_starbase_number .. " and disembark Prince " .. prince_name .."!");
		register_timer_callback("queen_impatience", 300, queen_wait + 1);
	end
end

-- Ending 1

function prince_panic2()
	if not check_prince_aboard() then
		return;
	end
	prince_smashing = true;
	comms_channel_transmit("Prince " .. prince_name, 0,	"PLEASE!");
	comms_channel_transmit("Prince " .. prince_name, 0,	"PLEASE DONT'T DO THIS!");
	prince_panic_smashing();
	comms_channel_transmit("Technician", 0, "The prince is going crazy in here!");
	comms_channel_transmit("Technician", 0, "He's smashing everything!");
	comms_channel_transmit("Technician", 0, "Someone needs to stop him!");
	set_custom_button_label(player_ids[1], 2, "RESTRAIN PRINCE");
	enable_custom_button(player_ids[1], 2);
	register_callback("custom-button-press-event", "restrain_prince");
end

function prince_panic_smashing()
	if not check_prince_aboard() then
		return;
	end
	if (prince_smashing == false) then
		return;
	end
	systems	= {"shield", "impulse", "warp", "maneuvering", "phaser", "sensor", "comms", "tractor", "lifesupport"};
	system	= systems[math.random(1,9)];
	damage	= get_player_damage(player_ids[1], system) + 50;
	if (damage > 255) then
		damage = 255;
	end
	set_player_damage(player_ids[1], system, damage);
	register_timer_callback("prince_panic_smashing", 10, 0);
	play_sound(player_ids[1], "sounds/flak_hit.ogg");
end

function restrain_prince()
	prince_smashing = false;
	if check_prince_aboard() then
		comms_channel_transmit("Security", 0, "The prince has been restrained");
		text_to_speech(player_ids[1], "The prince has been restrained");
	end
	disable_custom_button(player_ids[1], 2);
	unregister_callback("custom-button-press-event", "restrain_prince");
end

function ending1()
	ending1_triggered = true;
	comms_transmission(queen_ship, "---");
	if (queen_attack_started) then
		comms_transmission(queen_ship, "Glad to see you came to your senses");
	end
	comms_transmission(queen_ship, "Thank you for bringing the prince in time for the wedding");
	if check_player_cargo() then
		comms_transmission(queen_ship, "Here are your $" .. post_payment .. " as promised");
		update_player_wallet(player_ids[1], post_payment);
	else
		comms_transmission(queen_ship, "But since you lost the cargo, you can forget about your $" .. post_payment);
		comms_transmission(queen_ship, "GET THE HELL OUT OF HERE, THIEVES");
	end
	collect_player_cargo();
	comms_transmission(queen_ship, "---");
	set_passenger_location(prince,	wedding_starbase);
	register_timer_callback("wedding_announcement", 50, 0);
end

function wedding_announcement()
	comms_transmission(wedding_starbase, "---");
	comms_transmission(wedding_starbase, "WE ARE PROUD TO ANNOUNCE THE WEDDING");
	comms_transmission(wedding_starbase, " OF PRINCE " .. prince_name .. " AND PRINCESS " .. princess_name);
	comms_transmission(wedding_starbase, " SEALING THE UNION OF THE " .. queen_faction_name .. " AND THE " .. princess_faction_name);
	comms_transmission(wedding_starbase, "---");
	end_mission();
end

-- Ending 2

function queen_attack()
	if (ending1_triggered) then
		return;
	end
	queen_attack_started = true;
	comms_transmission(queen_ship, "You traitor! Come back here!");
	comms_transmission(queen_ship, "The " .. queen_faction_name .. " fleet will find you!");
	set_faction(player_ids[1], zarkon); -- Set player faction to zarkon to make sure queen faction attacks them
	ai_push_attack(queen_ship, player_ids[1]) -- FIXME Will only work if queen is near
	-- TODO add a few attack ships pursuing player
	queen_needed = false;
	comms_channel_transmit("Prince " .. prince_name, 0,	"Thank you so much!");
	comms_channel_transmit("Prince " .. prince_name, 0,	"Once you manage to escape pursuit");
	comms_channel_transmit("Prince " .. prince_name, 0,	"I can guide you to people I trust");
	register_timer_callback("check_if_player_safe", 60, 0);
end

function check_if_safe(shipid)
	if (debug) then
		demon_print("check if " .. get_object_name(shipid) .. " is safe");
	end
	for oid, alive in pairs(ships) do
		if ((alive == true) and (shipid ~= oid)) then
			distance = object_distance(shipid, oid);
			if (distance and (distance < 20000)) then
				demon_print(get_object_name(oid) .. " is too close");
				return false;
			end
		end
	end
	if (debug) then
		demon_print(get_object_name(shipid) .. " is safe");
	end
	return true;
end

function check_if_player_safe()
	if (check_if_safe(player_ids[1])) then
		comms_channel_listen("Belugos", secret_channel, "secret_channel_callback");
		comms_channel_transmit("Prince " .. prince_name, 0,	"I think we lost them!");
		comms_channel_transmit("Prince " .. prince_name, 0,	"Send " .. secret_password .. " on channel " .. secret_channel);
		comms_channel_transmit("Prince " .. prince_name, 0,	"I hope my friends are listening");
	else
		register_timer_callback("check_if_player_safe", 60, 0);
	end
end

function secret_channel_callback(from, channel, message)
	if (from == "Belugos") then
		return;
	end
	if (channel ~= secret_channel) then
		return;
	end
	if (string.upper(message) == secret_password) then
		add_spacemonster(generate_name(), secret_base.x + 1000, secret_base.y, secret_base.z);
		add_spacemonster(generate_name(), secret_base.x + 2000, secret_base.y, secret_base.z);
		comms_channel_transmit("Belugos", secret_channel,	"Glad to hear from you, Prince!");
		comms_channel_transmit("Belugos", secret_channel,	"We heard about the wedding.");
		comms_channel_transmit("Belugos", secret_channel,	"Come meet us at these coordinates: " .. secret_base.x .. ", " .. secret_base.y .. ", " .. secret_base.z);
		comms_channel_transmit("Belugos", secret_channel,	"We will drop stealth mode when you get close");
		register_proximity_callback("secret_base_showsup", player_ids[1], secret_base.nebula, 6000);
	end
end

function secret_base_showsup()
	set_faction(player_ids[1], 0); -- Set player faction to neutral
	add_explosion(secret_base.x, secret_base.y, secret_base.z, 1, 60, 30, 3, 0);
	secret_base.sb = add_starbase(secret_base.x, secret_base.y, secret_base.z, secret_base.number);
	comms_transmission(secret_base.sb, "Hello, friends!");
	comms_transmission(secret_base.sb, "Please dock at the starbase");
end

function ending2()
	ending2_triggered = true;
	comms_transmission(secret_base.sb, "---");
	comms_transmission(secret_base.sb, "Thank you for your help, " .. player_name .. "!");
	comms_transmission(secret_base.sb, "I'm sorry about the money you were promised");
	comms_transmission(secret_base.sb, "You can keep the cargo instead");
	comms_transmission(secret_base.sb, "I can never thank you enough!");
	comms_transmission(secret_base.sb, "BYE");
	comms_transmission(secret_base.sb, "---");
	set_passenger_location(prince,	secret_base.sb);
	end_mission();
end

function end_mission()
	set_faction(player_ids[1], 0); -- Set player faction to neutral
	comms_channel_transmit("MCMic", 0, "-- END OF ROYAL-WEDDING MISSION - CONGRATULATIONS! --");
	unregister_callback("player-docked-event", "player_docked_callback");
	unregister_callback("object-death-event", "object_death_callback");
end

-- Initial setup
function start_mission()
	create_asteroid_field(50, px, py, pz, 50000, -1);

	add_a_bunch_of_random_ships(20, queen_faction);

	add_random_planets(3);

	queen_wait_near_queen_planet();
	--~ dock_player_to_starbase(queen_ship, queen_starbase);

	dock_player_to_starbase(player_ids[1], queen_starbase);
	load_player_cargo();

	register_callback("player-docked-event", "player_docked_callback");

	intro();

	register_timer_callback("queen_message", 50, 0);
	register_timer_callback("queen_go_to_wedding", 600, 0);
	register_proximity_callback("queen_wait_near_wedding", queen_ship, wedding_planet, 10000);
	register_timer_callback("prince_message", 600, 0);
end

-- Let's go

start_mission();
