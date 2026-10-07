-- Haul a shipment of Coors beer across the system on a bet. No warp drive.
--
-- BEER RUN (Smokey and the Bandit style mission)
--
-- Big Enos and Little Enos have wagered $80,000 that you can't run out to
-- Golden City near Texarkana, pick up an illegal cargo of Coors beer,
-- and bring it back to Atlanta on sublight impulse without using the warp drive
-- before the clock runs out.
--
-- Along the way you'll face Sheriff Buford T. Justus of the Galactic Beer
-- Patrol, asteroid hazard zones, and black holes. Runaway bride Princess
-- Frogg might be hitching a ride and can call in a trucker convoy over CB 19!
--

arg = {...};

if ((#arg >= 2) and (tonumber(arg[2]) == 1)) then
	debug = true;
	demon_print("BEERRUN: debug mode enabled");
else
	debug = false;
end

-- Clear universe
clear_all();

-- -------------------------------------------------------------
-- TUNABLES & MISSION CONFIGURATION
-- -------------------------------------------------------------
if debug then
	time_limit_seconds = 45 * 60;   -- 45 minutes for debug
else
	time_limit_seconds = 28 * 60;   -- 28 minutes (~140k each way at 300 u/s + margin)
end

bet_amount = 80000;

-- Coordinates (Origin / Center star is at 0, 0, 0)
-- Atlanta is in the positive X/Z quadrant
atlanta_x = 100000;
atlanta_y = 500;
atlanta_z = 100000;
atlanta_r = 4500;
sb19_number = 19;

-- Texarkana, opposite the sun relative to atlanta
texarkana_x = -62374;
texarkana_y = 5428;
texarkana_z = -47800;
texarkana_r = 4254;

-- Chapel station SB-7 (where Princess Frogg is stranded)
chapel_x = 45000;
chapel_y = 700;
chapel_z = 35000;
sb7_number = 7;

-- Obstacles on return leg (between Texarkana (0,0) and Atlanta (100k, 100k))
speed_trap_x = 22000;
speed_trap_y = 500;
speed_trap_z = 22000;

asteroid_field_x = 50000;
asteroid_field_y = 500;
asteroid_field_z = 50000;
asteroid_field_radius = 16000;
asteroid_field_count = 130;

black_hole_x = 58000;
black_hole_y = 400;
black_hole_z = 46000;
black_hole_radius = 1500;

nebula_x = 75000;
nebula_y = 500;
nebula_z = 75000;
nebula_radius = 12000;

roadblock_x = 88000;
roadblock_y = 500;
roadblock_z = 88000;

-- Faction IDs
faction_neutral   = 0;
faction_wallunni  = 1;
faction_schaazbaut = 2;
faction_zarkon    = 3; -- Beer Patrol
faction_vekkazi   = 4;

-- Ship classes (from share/snis/ship_types.txt)
ship_cruiser      = 0;
ship_destroyer    = 1;
ship_freighter    = 2;
ship_tanker       = 3;
ship_skorpio      = 11;
ship_dragonhawk   = 10;
ship_scrambler    = 15;
ship_wombat       = 17;
ship_enforcer     = 20;
ship_escapepod    = 21;

-- State tracking
mission_state = "SETUP"; -- SETUP, OFFERED, TO_PICKUP, AT_PICKUP, HAULING, WON, LOST
time_remaining = time_limit_seconds;
elapsed_seconds = 0;
warp_disqualified = false;
mission_over = false;

beer_cargo_index = nil;
beer_container_id = nil;
beer_tag = "BEER7";
beer_on_board = false;

frogg_passenger_id = 0;
frogg_on_board = false;
frogg_convoy_charges = 2;
frogg_convoy_cooldown = 0;
convoy_ships = {};
convoy_active = false;
convoy_timer = 0;

sheriff_ship_id = nil;
sheriff_deaths = 0;
sheriff_respawn_timer = 0;
deputy_ships = {};
speed_trap_tripped = false;
roadblock_tripped = false;
last_sheriff_taunt_time = 0;
last_sheriff_hit_time = 0;

-- -------------------------------------------------------------
-- COMMODITY SETUP: COORS BEER
-- -------------------------------------------------------------
beer_cargo_index = lookup_commodity("COORS BEER");
if beer_cargo_index == nil then
	-- add_commodity(category, name, unit, scans_as, base_price, volatility, legality, odds)
	-- Legality 0.1 makes it bootleg contraband, odds 0 means never randomly generated!
	beer_cargo_index = add_commodity("liquor", "COORS BEER", "kegs", "water and ethanol",
	                                 150.0, 0.2, 0.1, 0.0);
end

-- -------------------------------------------------------------
-- DIALOGUE TABLES (Humorous, original PG Southern-fried lines)
-- -------------------------------------------------------------
sheriff_alert_taunts = {
	"What in the nine rings o' Saturn do you think you're pullin', boy?! Pull that rustbucket over right now!",
	"Attention leadfoot: this is Sheriff Buford T. Justus of the Galactic Beer Patrol! Cut your impulse drives or I'll dismantle you piece by piece!",
	"I got your scan, speeder! Nobody outruns Buford T. Justus in his own star sector, nobody!",
	"You got ten seconds to surrender that bootleg cargo before my laser batteries introduce themselves to your hull!"
};

sheriff_chase_taunts = {
	"Junior! Keep your useless eyes on their thermal trail and put down that planetary corndog!",
	"You think you're slick slippin' through sublight, don't you? My grandma maneuvers better than that, and she's frozen in cryo-stasis!",
	"When I catch up with you, I'm personally sendin' your captain to the penal moon o' Desolation!",
	"There is no way, no way in the whole galaxy, that you came from my patrol sector!",
	"Keep runnin', scofflaw! You're only addin' light-years onto your eventual sentence!"
};

sheriff_hit_taunts = {
	"Ouch! You scratched my fresh patrol enamel, you no-good interstellar hoodlum!",
	"Junior, that scoundrel just scorched my favorite port fin! Return fire before I disown you!",
	"That did not hurt one bit! My armor's grade-A titanium alloy, you rascal!"
};

sheriff_nebula_taunts = {
	"Where'd they go?! Junior, don't just stare at the purple gas clouds, tune the sensors!",
	"Hidin' in space clouds like cowards! You can't stay in that ionizing soup forever, boy!",
	"They're sneakin' through the dust! Sweep every grid, Junior, or you're walkin' home!"
};

sheriff_respawn_taunts = {
	"You thought blowin' up my cruiser would shake me?! I commandeered another ship, boy! Justus never sleeps!",
	"I'm back, you scamp! And this time I am person-ally offended! Prepare for boardin'!",
	"Junior, grab the emergency steering lever! We're back in the hunt, leadfoot!"
};

sheriff_junior_lines = {
	"Junior! If your brains was rocket propellant, you couldn't lift a peanut off the deck!",
	"Junior, hush your mouth! A boy who can't target a sublight freighter don't get to speak on tactical comms!",
	"I told your mother takin' you on patrol duty was a mistake, and by thunder I was right!"
};

function pick_random_line(tbl)
	return tbl[math.random(1, #tbl)];
end

-- -------------------------------------------------------------
-- UNIVERSE GENERATION & PLACEMENT
-- -------------------------------------------------------------
function add_planet_plus_starbase(name, x, y, z, r, s, t, sb_num)
	p = add_planet(name, x, y, z, r, s, t);
	sb_offset = r + 1500;
	sb = add_starbase(x + sb_offset, y + 200, z + sb_offset, sb_num);
	return p, sb;
end

-- Find player ship
player_ids = get_player_ship_ids();
player_id = player_ids[1];

if not player_id then
	demon_print("BEERRUN: No player ship found, aborting mission.");
	exit();
end

player_name = get_object_name(player_id);

-- Create Atlanta & Home Starbase 19
atlanta_planet, atlanta_sb = add_planet_plus_starbase("ATLANTA", atlanta_x, atlanta_y, atlanta_z,
                                                      atlanta_r, 2, 0, sb19_number);
set_planet_description(atlanta_planet,
	"Prosperous trade capital of the sector. Famous for lively markets,\n" ..
	"bustling orbital traffic, and wealthy eccentric bettors.");
set_planet_government(atlanta_planet, 6); -- Democracy
set_planet_tech_level(atlanta_planet, 5);  -- Space age
set_planet_economy(atlanta_planet, 0);     -- Rich Industrial

-- Create Texarkana & Golden City area
texarkana_planet, texarkana_sb = add_planet_plus_starbase("TEXARKANA", texarkana_x, texarkana_y, texarkana_z,
                                                          texarkana_r, 1, 0, 4);
set_planet_description(texarkana_planet,
	"Lush world known for pristine mountain spring water and the legendary\n" ..
	"breweries of Golden City. Export of authentic brew across the sector is strictly regulated.");
set_planet_government(texarkana_planet, 5); -- Confederacy
set_planet_tech_level(texarkana_planet, 5);  -- Space age
set_planet_economy(texarkana_planet, 5);     -- Rich Agricultural

-- Chapel Station SB-7 (Princess Frogg's waypoint)
chapel_sb = add_starbase(chapel_x, chapel_y, chapel_z, sb7_number);
set_faction(chapel_sb, faction_neutral);
set_plot_armor(chapel_sb, 1);

-- Set starbase factions
set_faction(atlanta_sb, faction_neutral);
set_plot_armor(atlanta_sb, 1);
set_faction(texarkana_sb, faction_neutral);
set_plot_armor(texarkana_sb, 1);

-- Place the beer cargo container hovering right above Golden City on Texarkana
beer_x = texarkana_x + texarkana_r + 2000;
beer_y = texarkana_y + 300;
beer_z = texarkana_z + texarkana_r + 1800;

beer_container_id = add_cargo_container(beer_x, beer_y, beer_z, 0, 0, 0);
if beer_container_id then
	mark_cargo_container_not_for_npc_ships(beer_container_id);
	set_commodity_contents(beer_container_id, beer_cargo_index, 400.0);
	set_cargo_container_transporter_tag(beer_container_id, beer_tag);
	attach_science_text(beer_container_id,
		"CONTRABAND: 400 kegs of genuine Coors Beer brewed in Golden City.\n" ..
		"Transporter tag: " .. beer_tag .. ". Requires transporter beam within 1500 units.");
end

-- Princess Frogg passenger setup at Chapel Station SB-7
create_passenger(frogg_passenger_id, "Princess Frogg", chapel_sb, atlanta_sb, 0);
set_passenger_transporter_tag(frogg_passenger_id, "FROGG");

-- Obstacles: Asteroid Field
function create_asteroid_hazard(n, cx, cy, cz, radius)
	for i = 1, n do
		repeat
			dx = math.random(-radius, radius);
			dy = math.random(-math.floor(radius * 0.2), math.floor(radius * 0.2));
			dz = math.random(-radius, radius);
			r2 = dx * dx + dy * dy + dz * dz;
		until r2 < radius * radius;
		ast = add_asteroid(cx + dx, cy + dy, cz + dz);
		if ast and ast >= 0 then
			set_asteroid_speed(ast, 0.0);
		end
	end
end

create_asteroid_hazard(asteroid_field_count, asteroid_field_x, asteroid_field_y,
                      asteroid_field_z, asteroid_field_radius);

-- Black Hole near the asteroid field gap
black_hole_id = add_black_hole("GULF MAW", black_hole_x, black_hole_y, black_hole_z, black_hole_radius);

-- Mississippi Nebula
nebula_id = add_nebula("MISSISSIPPI NEBULA", nebula_x, nebula_y, nebula_z, nebula_radius);

-- A few neutral civilian freighters along the route for flavor
add_ship("LONG HAULER 1", 30000, 400, 30000, ship_freighter, faction_neutral, 0);
add_ship("WAYFARER 9", 70000, 600, 68000, ship_tanker, faction_neutral, 0);

-- -------------------------------------------------------------
-- SHIP INITIALIZATION
-- -------------------------------------------------------------
reset_player_ship(player_id);
refuel_player_ship(player_id);
set_faction(player_id, faction_neutral);

-- Empty player cargo bays to make room
for bay = 0, 7 do
	set_commodity_contents(player_id, -1, 0, bay);
end

-- Dock player at SB-19
dock_player_to_starbase(player_id, atlanta_sb);

-- -------------------------------------------------------------
-- UTILITY FUNCTIONS
-- -------------------------------------------------------------
function format_time(seconds)
	if seconds < 0 then seconds = 0; end
	m = math.floor(seconds / 60);
	s = seconds % 60;
	return string.format("%02d:%02d", m, s);
end

function check_beer_aboard(pid)
	for bay = 0, 7 do
		item = get_ship_attribute(pid, "cargo[" .. bay .. "].contents.item");
		qty  = get_ship_attribute(pid, "cargo[" .. bay .. "].contents.qty");
		if item == beer_cargo_index and qty > 0 then
			return true, qty;
		end
	end
	return false, 0;
end

function remove_beer_from_ship(pid)
	for bay = 0, 7 do
		item = get_ship_attribute(pid, "cargo[" .. bay .. "].contents.item");
		if item == beer_cargo_index then
			set_commodity_contents(pid, -1, 0, bay);
		end
	end
end

-- -------------------------------------------------------------
-- MISSION FLOW & COMMUNICATING WITH PLAYERS
-- -------------------------------------------------------------
function intro_announcement()
	show_timed_text(-1, 20,
		"=== BEER RUN ===\n" ..
		"A high-stakes wager by Big Enos & Little Enos.\n" ..
		"Haul 400 kegs of Coors Beer from Texarkana back to Atlanta!\n" ..
		"STRICT RULE: SUB-LIGHT IMPULSE ONLY. NO WARP DRIVE ALLOWED.\n" ..
		"Time limit: " .. math.floor(time_limit_seconds / 60) .. " minutes. Prize: $" .. bet_amount .. "!");
end

function start_bet_offer()
	comms_transmission(atlanta_sb, "---");
	comms_transmission(atlanta_sb, "Breaker one-nine! Big Enos here aboard SB-19 Atlanta!");
	comms_transmission(atlanta_sb, "My boy Little Enos and I got ourselves a terrible thirst and a");
	comms_transmission(atlanta_sb, "big pile o' credits. We're wagerin' $" .. bet_amount .. " that");
	comms_transmission(atlanta_sb, "nobody's got the grit to fly out to Golden City on Texarkana,");
	comms_transmission(atlanta_sb, "load up 400 kegs of genuine Coors Beer, and haul it back here");
	comms_transmission(atlanta_sb, " under " .. math.floor(time_limit_seconds / 60) .. " minutes!");
	comms_transmission(atlanta_sb, "RULE IS SIMPLE: ALL ON IMPULSE POWER. ENGAGE WARP DRIVE ONCE,");
	comms_transmission(atlanta_sb, "hAND YOU FORFEIT $" .. bet_amount .. "!");
	comms_transmission(atlanta_sb, "---");

	menu = comms_menu_create(
		"1. TAKE THE BET ($" .. bet_amount .. " on the line, NO WARP!)\n" ..
		"2. DECLINE (Too risky for this crew)",
		"bet_menu_callback");
	comms_menu_invoke(menu, player_id);
end

function bet_menu_callback(choice, pid)
	comms_menu_destroy(menu);
	if choice == 0 then
		-- Accepted!
		mission_state = "TO_PICKUP";
		demon_print("BEERRUN: Bet accepted by player at " .. format_time(elapsed_seconds));
		comms_transmission(atlanta_sb, "THAT'S THE SPIRIT! The clock is runnin' right now!");
		comms_transmission(atlanta_sb, "Head for planet Texarkana!");
		comms_transmission(atlanta_sb, "Golden City has the brew staged in orbital containers.");
		comms_transmission(atlanta_sb, "Don't use warp or you owe us $" .. bet_amount .. "!");
		text_to_speech(player_id, "The bet is on. Maintain sublight impulse. Do not engage warp drive.");

		-- Enable NAV countdown button (screen 0 = NAV)
		set_custom_button_label(player_id, 0, "T-" .. format_time(time_remaining));
		enable_custom_button(player_id, 0);

		-- Listen for warp rule violations
		register_callback("player-warp-travel-event", "player_warp_event");
	else
		-- Declined
		mission_state = "DECLINED";
		comms_transmission(atlanta_sb, "Hah! Figured as much. Come back when you find your backbone!");
		demon_print("BEERRUN: Player declined the bet.");
	end
end

-- -------------------------------------------------------------
-- WARP DISQUALIFICATION
-- -------------------------------------------------------------
function player_warp_event(pid, x1, y1, z1, x2, y2, z2, warpfactor)
	if mission_state == "TO_PICKUP" or mission_state == "AT_PICKUP" or mission_state == "HAULING" then
		warp_disqualified = true;
		demon_print("BEERRUN: Player violated the no-warp rule at " .. format_time(elapsed_seconds));
		mission_lost("WARP_USED");
	end
end

-- -------------------------------------------------------------
-- SPEED TRAP & BEER PATROL SPAWNS
-- -------------------------------------------------------------
function spawn_speed_trap()
	sheriff_ship_id = add_ship("PATROL CRUISER 1", speed_trap_x, speed_trap_y, speed_trap_z,
	                           ship_enforcer, faction_zarkon, 0);
	set_plot_armor(sheriff_ship_id, 0);

	deputy_ships[1] = add_ship("JUNIOR's CRUISER", speed_trap_x + 600, speed_trap_y, speed_trap_z + 400,
	                           ship_skorpio, faction_zarkon, 0);
	set_plot_armor(deputy_ships[1], 0);

	deputy_ships[2] = add_ship("DEPUTY SNEED", speed_trap_x - 600, speed_trap_y, speed_trap_z - 400,
	                           ship_dragonhawk, faction_zarkon, 0);
	set_plot_armor(deputy_ships[2], 0);
end

function alert_beer_patrol()
	if speed_trap_tripped then return; end
	speed_trap_tripped = true;
	demon_print("BEERRUN: Speed trap triggered by player at " .. format_time(elapsed_seconds));

	play_sound(player_id, "sounds/alarm_buzzer.ogg");
	text_to_speech(player_id, "Alert. Galactic Beer Patrol ships approaching.");

	line = pick_random_line(sheriff_alert_taunts);
	comms_channel_transmit("Sheriff Buford T. Justus", 0, line);

	if sheriff_ship_id then
		ai_push_attack(sheriff_ship_id, player_id);
	end
	for i, dep in pairs(deputy_ships) do
		ai_push_attack(dep, player_id);
	end
end

function spawn_roadblock()
	if roadblock_tripped then return; end
	roadblock_tripped = true;
	demon_print("BEERRUN: Roadblock spawned in front of Atlanta at " .. format_time(elapsed_seconds));

	comms_channel_transmit("Galactic Beer Patrol", 0, "ROADBLOCK IN PLACE! All ships approachin' Atlanta, prepare for contraband inspection!");
	text_to_speech(player_id, "Warning. Roadblock detected ahead.");

	rb1 = add_ship("PATROL INTERCEPTOR 1", roadblock_x, roadblock_y, roadblock_z, ship_enforcer, faction_zarkon, 0);
	rb2 = add_ship("PATROL INTERCEPTOR 2", roadblock_x + 1200, roadblock_y, roadblock_z - 1000, ship_skorpio, faction_zarkon, 0);
	rb3 = add_ship("PATROL INTERCEPTOR 3", roadblock_x - 1200, roadblock_y, roadblock_z + 1000, ship_dragonhawk, faction_zarkon, 0);

	ai_push_attack(rb1, player_id);
	ai_push_attack(rb2, player_id);
	ai_push_attack(rb3, player_id);
end

-- -------------------------------------------------------------
-- PRINCESS FROGG & TRUCKER CONVOY
-- -------------------------------------------------------------
function check_frogg_boarded()
	if frogg_on_board then return; end
	loc = get_passenger_location(frogg_passenger_id);
	if loc == player_id then
		frogg_on_board = true;
		demon_print("BEERRUN: Princess Frogg boarded player ship at " .. format_time(elapsed_seconds));
		play_sound(player_id, "sounds/crewmember-has-joined.ogg");
		text_to_speech(player_id, "Princess Frogg has boarded the ship.");

		comms_channel_transmit("Princess Frogg", 0, "Thanks for the lift! I just left Junior standin' at the altar on Chapel Station.");
		comms_channel_transmit("Princess Frogg", 0, "If Buford's deputies start crowdin' us, use the FROGG CB-19 button on COMMS! I've got trucker friends all over this sector!");

		-- Enable Frogg convoy call button on COMMS (screen 5 = COMMS)
		set_custom_button_label(player_id, 5, "FROGG: CB 19");
		enable_custom_button(player_id, 5);
	end
end

function custom_button_press_callback(pid, screen)
	if screen == 5 and frogg_on_board then
		call_trucker_convoy();
	end
end

function call_trucker_convoy()
	if frogg_convoy_charges <= 0 then
		comms_channel_transmit("Princess Frogg", 0, "All my trucker buddies are out of range now! We're on our own!");
		return;
	end
	if frogg_convoy_cooldown > 0 then
		comms_channel_transmit("Princess Frogg", 0, "Convoy is still reorganizin', hold on!");
		return;
	end

	frogg_convoy_charges = frogg_convoy_charges - 1;
	frogg_convoy_cooldown = 120; -- 2 minutes between calls
	convoy_active = true;
	convoy_timer = 90;           -- Convoy stays for 90 seconds

	demon_print("BEERRUN: Trucker convoy called by Princess Frogg at " .. format_time(elapsed_seconds));

	px, py, pz = get_object_location(player_id);
	if not px then return; end

	comms_channel_transmit("Princess Frogg", 19, "Breaker breaker one-nine! Any big rigs got their ears on? We got Beer Patrol ridin' our tail!");
	comms_channel_transmit("Big Bear", 19, "We copy you, Princess! Hang on, Snowman, the cavalry is droppin' out o' warp right now! 10-4!");
	text_to_speech(player_id, "Trucker convoy arriving on channel 19.");

	convoy_ships[1] = add_ship("BIG BEAR (RIG)", px + 1500, py + 200, pz + 1000, ship_tanker, faction_wallunni, 0);
	convoy_ships[2] = add_ship("SNOWMAN'S RIG", px - 1500, py - 200, pz + 1200, ship_freighter, faction_wallunni, 0);
	convoy_ships[3] = add_ship("DIRT ROAD WRECKER", px + 800, py + 400, pz - 1200, ship_wombat, faction_wallunni, 0);

	set_plot_armor(convoy_ships[1], 1);
	set_plot_armor(convoy_ships[2], 1);
	set_plot_armor(convoy_ships[3], 1);

	redirect_convoy_to_patrol();
end

function redirect_convoy_to_patrol()
	if not convoy_active then return; end
	-- Point convoy ships at sheriff or nearby patrol
	target = sheriff_ship_id;
	if not target then
		for _, dep in pairs(deputy_ships) do
			if dep then target = dep; break; end
		end
	end

	if target then
		for _, cship in pairs(convoy_ships) do
			if cship then
				ai_push_attack(cship, target);
			end
		end
	end
end

function dismiss_convoy()
	convoy_active = false;
	demon_print("BEERRUN: Trucker convoy leaving at " .. format_time(elapsed_seconds));
	comms_channel_transmit("Big Bear", 19, "We gotta hammer down and make our own delivery schedule, Princess! Keep that hammer down! Catch you on the flip-flop! 10-4!");
	for _, cship in pairs(convoy_ships) do
		if cship then
			delete_object(cship);
		end
	end
	convoy_ships = {};
end

-- -------------------------------------------------------------
-- BEER PICKUP & TRANSPORTER EVENTS
-- -------------------------------------------------------------
function check_pickup_proximity()
	if mission_state ~= "TO_PICKUP" then return; end
	dist = object_distance(player_id, texarkana_planet);
	if dist and dist < 22000 then
		mission_state = "AT_PICKUP";
		demon_print("BEERRUN: Player arrived at Texarkana pickup at " .. format_time(elapsed_seconds));
		comms_channel_transmit("Brewmaster (Golden City)", 0, "---");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "Atlanta hauler, we see you on approach!");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "400 kegs of cold Coors are staged in orbit.");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "CONTAINER TRANSPORTER TAG IS '" .. beer_tag .. "'.");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "GET WITHIN TRANSPORTER RANGE (1500 units)");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "AND ENERGIZE TO LOAD CARGO!");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "---");
		text_to_speech(player_id, "Brewery contacted. Transporter tag is " .. beer_tag .. ". Close to fifteen hundred units.");
	end
end

function cargo_acquired_callback(pid, item_index, qty)
	if item_index == beer_cargo_index and (mission_state == "AT_PICKUP" or mission_state == "TO_PICKUP") then
		beer_on_board = true;
		mission_state = "HAULING";
		demon_print("BEERRUN: Beer loaded aboard player ship at " .. format_time(elapsed_seconds));
		play_sound(player_id, "sounds/changescreen.ogg");
		text_to_speech(player_id, "Beer loaded. Turn around and head for Atlanta. Watch for Beer Patrol.");

		comms_channel_transmit("Brewmaster (Golden City)", 0, "The payload is aboard!");
		comms_channel_transmit("Brewmaster (Golden City)", 0, "Get goin', boy, before Buford catches yer ass!");
		comms_transmission(atlanta_sb, "Big Enos here! We see you got the suds loaded!");
		comms_transmission(atlanta_sb, "Half the clock is tickin' away, bring 'em home!");

		-- Spawn the initial speed trap en route to Atlanta
		spawn_speed_trap();
	end
end

-- -------------------------------------------------------------
-- COMBAT & DEATH CALLBACKS
-- -------------------------------------------------------------
function object_hit_callback(victim, shooter)
	if victim == sheriff_ship_id and (elapsed_seconds - last_sheriff_hit_time > 15) then
		last_sheriff_hit_time = elapsed_seconds;
		comms_channel_transmit("Sheriff Buford T. Justus", 0, pick_random_line(sheriff_hit_taunts));
	end
end

function object_death_callback(oid)
	if oid == sheriff_ship_id then
		sheriff_deaths = sheriff_deaths + 1;
		demon_print("BEERRUN: Sheriff ship destroyed (kill count: " .. sheriff_deaths .. ") at " .. format_time(elapsed_seconds));
		comms_channel_transmit("Sheriff Buford T. Justus", 0, "Junior! Don't just sit there screamin', get the distress beacon fired!");
		sheriff_ship_id = nil;
		sheriff_respawn_timer = 30; -- Respawns in 30 seconds
	end
	for i, dep in pairs(deputy_ships) do
		if oid == dep then
			deputy_ships[i] = nil;
		end
	end
end

function player_death_callback(pid)
	if pid == player_id and not mission_over then
		demon_print("BEERRUN: Player ship destroyed at " .. format_time(elapsed_seconds));
		mission_lost("PLAYER_DESTROYED");
	end
end

-- -------------------------------------------------------------
-- DOCKING AT DESTINATION (WIN / LOSE CHECK)
-- -------------------------------------------------------------
function player_docked_callback(pid, station_id)
	if station_id == atlanta_sb and mission_state == "HAULING" and not mission_over then
		has_beer, qty = check_beer_aboard(pid);
		if has_beer and not warp_disqualified and time_remaining > 0 then
			mission_won();
		else
			if not has_beer then
				mission_lost("NO_BEER");
			elseif warp_disqualified then
				mission_lost("WARP_USED");
			else
				mission_lost("TIMEOUT");
			end
		end
	elseif station_id == chapel_sb and not frogg_on_board then
		check_frogg_boarded();
	end
end

-- -------------------------------------------------------------
-- WIN / LOSE OUTCOMES
-- -------------------------------------------------------------
function mission_won()
	if mission_over then return; end
	mission_over = true;
	mission_state = "WON";

	remove_beer_from_ship(player_id);
	update_player_wallet(player_id, bet_amount);
	demon_print("BEERRUN: WINNER! Player delivered beer on time at " .. format_time(elapsed_seconds));

	play_sound(player_id, "sounds/entering-high-security-area.ogg");
	text_to_speech(player_id, "Delivery complete. Congratulations. Bet won.");

	comms_transmission(atlanta_sb, "===============================================");
	comms_transmission(atlanta_sb, "YOU DID IT, YOU MAGNIFICENT BASTARDS! YOU DID IT!");
	comms_transmission(atlanta_sb, "Cold Coors Beer right here in Atlanta, and not a ");
	comms_transmission(atlanta_sb, "drop of warp drive used!");
	comms_transmission(atlanta_sb, "Here is your $" .. bet_amount .. "! Big Enos and");
	comms_transmission(atlanta_sb, "Little Enos always pay their debts!");
	comms_transmission(atlanta_sb, "===============================================");

	if frogg_on_board then
		comms_channel_transmit("Princess Frogg", 0, "We actually made it! Drinks are ");
		comms_channel_transmit("Princess_Frogg", 0, "on Big Enos tonight!");
	end

	set_custom_button_label(player_id, 0, "WINNER! +$80K");
	cleanup_mission();
end

function mission_lost(reason)
	if mission_over then return; end
	mission_over = true;
	mission_state = "LOST";

	-- Deduct $80,000 for lost bet
	update_player_wallet(player_id, -bet_amount);
	demon_print("BEERRUN: MISSION LOST! Reason: " .. reason .. " at " .. format_time(elapsed_seconds));

	play_sound(player_id, "sounds/fuel-levels-critical.ogg");

	comms_transmission(atlanta_sb, "===============================================");
	if reason == "WARP_USED" then
		text_to_speech(player_id, "Warp drive engaged. Bet lost.");
		comms_transmission(atlanta_sb, "CHEATER! Our sensors picked up your warp");
		comms_transmission(atlanta_sb, "signature! The bet was NO WARP!");
		comms_transmission(atlanta_sb, "That'll cost you $" .. bet_amount .. ", fair");
		comms_transmission(atlanta_sb, "and square! Pay up!");
		comms_channel_transmit("Sheriff Buford T. Justus", 0, "Hah! Caught 'em red-handed");
		comms_channel_transmit("Sheriff Buford T. Justus", 0, "violatin' space transit statutes!");
		comms_channel_transmit("Sheriff Buford T. Justus", 0, "Justice wins again!");
	elseif reason == "TIMEOUT" then
		text_to_speech(player_id, "Time has expired. Bet lost.");
		comms_transmission(atlanta_sb, "TIME IS UP! The party is over and the ");
		comms_transmission(atlanta_sb, "beer ain't here!");
		comms_transmission(atlanta_sb, "A bet's a bet! You owe Big Enos and Little ");
		comms_transmission(atlanta_sb, "Enos $" .. bet_amount .. "!");
	elseif reason == "NO_BEER" then
		text_to_speech(player_id, "Cargo bay contains no Coors?");
		comms_transmission(atlanta_sb, "You docked without the Coors?! Where are ");
		comms_transmission(atlanta_sb, "the 400 kegs?!");
		comms_transmission(atlanta_sb, "No beer, no win!");
		comms_transmission(atlanta_sb, "Pay up the $" .. bet_amount .. "!");
	elseif reason == "PLAYER_DESTROYED" then
		text_to_speech(player_id, "Ship destroyed. Mission failed.");
		comms_transmission(atlanta_sb, "Looks like the Beer Patrol scrapped you!");
		comms_transmission(atlanta_sb, "That's $" .. bet_amount .. " you owe us!");
	end
	comms_transmission(atlanta_sb, "===============================================");

	set_custom_button_label(player_id, 0, "LOST -$80K");
	cleanup_mission();
end

function cleanup_mission()
	disable_custom_button(player_id, 5);
	if convoy_active then
		dismiss_convoy();
	end
end

-- -------------------------------------------------------------
-- PERIODIC 1-SECOND GAME TICK
-- -------------------------------------------------------------
function mission_tick(cookie)
	if mission_over then return; end

	elapsed_seconds = elapsed_seconds + 1;
	time_remaining = time_limit_seconds - elapsed_seconds;

	-- Update NAV countdown button
	if mission_state == "TO_PICKUP" or mission_state == "AT_PICKUP" or mission_state == "HAULING" then
		set_custom_button_label(player_id, 0, "T-" .. format_time(time_remaining));
	end

	-- Time milestones & alerts
	if time_remaining == math.floor(time_limit_seconds * 0.5) then
		comms_transmission(atlanta_sb, "Little Enos here: Half your time is gone!");
		comms_transmission(atlanta_sb, "Pick up the pace!");
	elseif time_remaining == math.floor(time_limit_seconds * 0.25) then
		comms_transmission(atlanta_sb, "Big Enos: One quarter time left!");
		comms_transmission(atlanta_sb, "You boys better have impulse pegged to the wall!");
	elseif time_remaining == 300 then
		text_to_speech(player_id, "Five minutes remaining.");
		comms_transmission(atlanta_sb, "FIVE MINUTES REMAINING!");
		comms_transmission(atlanta_sb, "We can already taste that $" .. bet_amount .. "!");
	elseif time_remaining == 60 then
		text_to_speech(player_id, "One minute remaining.");
		comms_transmission(atlanta_sb, "ONE MINUTE! You're runnin' out o' sky!");
	elseif time_remaining <= 0 and (mission_state == "TO_PICKUP" or mission_state == "AT_PICKUP" or mission_state == "HAULING") then
		mission_lost("TIMEOUT");
		return;
	end

	-- Check pickup proximity if heading to Texarkana
	if mission_state == "TO_PICKUP" then
		check_pickup_proximity();
	end

	-- Check Princess Frogg proximity/boarding
	if not frogg_on_board then
		dist_chapel = object_distance(player_id, chapel_sb);
		if dist_chapel and dist_chapel < 25000 and not frogg_hailed then
			frogg_hailed = true;
			comms_channel_transmit("Princess Frogg", 0, "Breaker one-nine! Any ship passin' Chapel Station SB-7");
			comms_channel_transmit("Princess Frogg", 0, "can I hitch a ride out of here before Junior finds me?!");
		end
		check_frogg_boarded();
	end

	-- Convoy cooldown & duration handling
	if frogg_convoy_cooldown > 0 then
		frogg_convoy_cooldown = frogg_convoy_cooldown - 1;
	end
	if convoy_active then
		convoy_timer = convoy_timer - 1;
		if (convoy_timer % 15 == 0) then
			redirect_convoy_to_patrol();
		end
		if convoy_timer <= 0 then
			dismiss_convoy();
		end
	end

	-- Check speed trap trip (when hauling beer)
	if mission_state == "HAULING" and not speed_trap_tripped and sheriff_ship_id then
		dist_trap = object_distance(player_id, sheriff_ship_id);
		if dist_trap and dist_trap < 30000 then
			alert_beer_patrol();
		end
	end

	-- Check roadblock trip (approaching Atlanta)
	if mission_state == "HAULING" and not roadblock_tripped then
		dist_rb = object_distance(player_id, atlanta_sb);
		if dist_rb and dist_rb < 35000 then
			spawn_roadblock();
		end
	end

	-- Sheriff respawn handling
	if sheriff_respawn_timer > 0 then
		sheriff_respawn_timer = sheriff_respawn_timer - 1;
		if sheriff_respawn_timer == 0 and not mission_over then
			px, py, pz = get_object_location(player_id);
			if px then
				stype = ship_skorpio;
				if sheriff_deaths >= 2 then
					stype = ship_escapepod;
				end
				sheriff_ship_id = add_ship("SHERIFF'S REPLACEMENT", px - 7000, py, pz - 7000, stype, faction_zarkon, 0);
				set_plot_armor(sheriff_ship_id, 0);
				ai_push_attack(sheriff_ship_id, player_id);
				comms_channel_transmit("Sheriff Buford T. Justus", 0, pick_random_line(sheriff_respawn_taunts));
			end
		end
	end

	-- Maintain Sheriff AI focus and check nebula shelter
	if sheriff_ship_id then
		dist_neb = object_distance(player_id, nebula_id);
		if dist_neb and dist_neb < nebula_radius then
			-- Inside nebula: Patrol gets confused!
			if math.random(1, 40) == 1 then
				comms_channel_transmit("Sheriff Buford T. Justus", 0, pick_random_line(sheriff_nebula_taunts));
			end
			px, py, pz = get_object_location(player_id);
			ai_push_patrol(sheriff_ship_id, 2, px + 2000, py, pz, px - 2000, py, pz);
		else
			-- In open space: Keep attacking!
			ai_push_attack(sheriff_ship_id, player_id);
			if (elapsed_seconds - last_sheriff_taunt_time > 75) and (math.random(1, 2) == 1) then
				last_sheriff_taunt_time = elapsed_seconds;
				comms_channel_transmit("Sheriff Buford T. Justus", 0, pick_random_line(sheriff_chase_taunts));
				if math.random(1, 3) == 1 then
					comms_channel_transmit("Sheriff Buford T. Justus", 0, pick_random_line(sheriff_junior_lines));
				end
			end
		end
	end

	-- Re-register timer for next 1 second (10 ticks)
	register_timer_callback("mission_tick", 10, 0);
end

-- -------------------------------------------------------------
-- STARTUP
-- -------------------------------------------------------------
function start_mission()
	register_callback("player-docked-event", "player_docked_callback");
	register_callback("cargo-container-acquired-event", "cargo_acquired_callback");
	register_callback("custom-button-press-event", "custom_button_press_callback");
	register_callback("object-hit-event", "object_hit_callback");
	register_callback("object-death-event", "object_death_callback");
	register_callback("player-death-event", "player_death_callback");

	intro_announcement();
	register_timer_callback("start_bet_offer", 50, 0);
	register_timer_callback("mission_tick", 10, 0);
end

start_mission();
