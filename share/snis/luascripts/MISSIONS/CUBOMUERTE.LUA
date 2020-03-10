

-- (spoilers follow)
--
-- 1. player recieves message: fleet of zarkon ships are transporting a powerful black hole generator
--    they mean to use as a weapon. You are to intercept them at such and such location, destroy them
--    and retrieve the black hole generator intact and bring it back to starbase 2.
--
-- 2. On returning to starbase 2 with the black hole generator: Player is informed about El Cubo de la Muerte.
--    a giant cube of death which aims to destroy a planet. Player is given hints about how to locate and
--    enter the cube, deploy the black hole generator, escape from the cube and trigger the black hole
--    generator, destroying the cube.

-- TODO:
--*1. Write intro voice-over
--*2. Record intro voice-overk
--*3. Trigger playing intro voice over
--*4. Add custom button on comms to play last voice overs
--*5. Add notification to comms to play voice overs
--*6. Add zarkon fleet
--*7. Add black hole generator commodity
--*8. Add code to allow deploying black hole generator and detonating it.
-- 9. Add details to death cube model, maze, guns, defenses
--*10. Add win condition
-- 11. Add loss condition.

adjx = -300000;
adjz = -300000;

clear_all();

secs = 10;
minutes = secs * 60;
cubo = -1;

-- Once the player docks with starbase 2 while carrying the black hole generator
-- how much time remains before elnox is destroyed?
elnox_time_limit = 15 * minutes;

function add_cubo_de_la_muerte(x, y, z)
  cubo = add_block(-1, x, y, z, 100, 100, 100, 1, 0, 0, 0, 0, 0);
  set_object_rotational_velocity(cubo, 1, 0, 0, 0);

  add_block(cubo, 0, -2000, 0, 4000, 100, 4000, 1, 0, 0, 0, 0, 0);
  add_block(cubo, 0, 2000, 0, 4000, 100, 4000, 1, 0, 0, 0, 0, 0);
  add_block(cubo, 0, 0, 2000, 4000, 4000, 100, 1, 0, 0, 0, 0, 0);
  add_block(cubo, 0, 0, -2000, 4000, 4000, 100, 1, 0, 0, 0, 0, 0);
  add_block(cubo, 2000, 0, 0, 100, 4000, 4000, 1, 0, 0, 0, 0, 0);
  add_block(cubo, -1800, 150, 0, 200, 4000, 3850, 1, 0, 0, 0, 0, 0);
  add_block(cubo, -2000, 0, -1050, 200, 4000, 1800, 1, 0, 0, 0, 1, 0);
  add_block(cubo, -2000, 0, 1050, 200, 4000, 1800, 1, 0, 0, 0, 1, 0);
end

planet = {};
asteroid = {};
starbase = {};
nebula = {};
zarkon = {};

function add_adjusted_asteroid(x, y, z)
  return add_asteroid(x + adjx, y, z + adjz);
end

function set_up_environment()

	-- planets
	planet[1] = add_planet("ASTU", adjx + 69497, 0.000000, adjz + 557342, 5177.000000, 0, 0);
	set_faction(planet[1], 3);
	planet[2] = add_planet("ELNOX", adjx + 517873, 0.000000, adjz + 71005, 3112.000000, 0, 2);
	set_faction(planet[2], 0);
	set_planet_security(planet[2], 2); -- high security
	planet[3] = add_planet("AURES", adjx + 507129, 0.000000, adjz + 517948, 7102.000000, 0, 0);
	set_faction(planet[3], 3);
	planet[4] = add_planet("NAVOXA", adjx + 65199, 0.000000, adjz + 76735, 4883.000000, 0, 1);
	set_faction(planet[4], 0);


	--starbases
	starbase[1] = add_starbase(adjx + 73255.281184, 13719.010732, adjz + 553144.749139, 0);
	set_faction(starbase[1], 3);
	starbase[2] = add_starbase(adjx + 514647.156240, 4366.714509, adjz + 517762.530836, 1);
	set_faction(starbase[2], 3);
	starbase[3] = add_starbase(adjx + 518266.937005, 7289.611552, adjz + 74720.171874, 2);
	set_faction(starbase[3], 0);

	-- asteroids
	asteroid[1] = add_adjusted_asteroid(95414.927866, 0.000000, 77161.661320);
	set_asteroid_speed(asteroid[1], 1.000000);
	asteroid[2] = add_adjusted_asteroid(102862.305576, 0.000000, 73172.894773);
	set_asteroid_speed(asteroid[2], 1.000000);
	asteroid[3] = add_adjusted_asteroid(86773.181114, 0.000000, 70199.860929);
	set_asteroid_speed(asteroid[3], 1.000000);
	asteroid[4] = add_adjusted_asteroid(77457.113956, 0.000000, 71832.868725);
	set_asteroid_speed(asteroid[4], 1.000000);
	asteroid[5] = add_adjusted_asteroid(71231.722806, 0.000000, 77988.007149);
	set_asteroid_speed(asteroid[5], 1.000000);
	asteroid[6] = add_adjusted_asteroid(67393.111270, 0.000000, 85682.644496);
	set_asteroid_speed(asteroid[6], 1.000000);
	asteroid[7] = add_adjusted_asteroid(77877.873861, 0.000000, 80524.754717);
	set_asteroid_speed(asteroid[7], 1.000000);
	asteroid[8] = add_adjusted_asteroid(79283.577845, 0.000000, 78215.648870);
	set_asteroid_speed(asteroid[8], 1.000000);
	asteroid[9] = add_adjusted_asteroid(85435.382020, 0.000000, 81977.331829);
	set_asteroid_speed(asteroid[9], 1.000000);
	asteroid[10] = add_adjusted_asteroid(86990.772721, 0.000000, 85184.571669);
	set_asteroid_speed(asteroid[10], 1.000000);
	asteroid[11] = add_adjusted_asteroid(87888.934447, 0.000000, 80645.050912);
	set_asteroid_speed(asteroid[11], 1.000000);
	asteroid[12] = add_adjusted_asteroid(86484.212646, 0.000000, 75455.281595);
	set_asteroid_speed(asteroid[12], 1.000000);
	asteroid[13] = add_adjusted_asteroid(83113.729638, 0.000000, 74353.634724);
	set_asteroid_speed(asteroid[13], 1.000000);
	asteroid[14] = add_adjusted_asteroid(63880.768634, 0.000000, 77525.800333);
	set_asteroid_speed(asteroid[14], 1.000000);
	asteroid[15] = add_adjusted_asteroid(53997.441309, 0.000000, 73212.809239);
	set_asteroid_speed(asteroid[15], 1.000000);
	asteroid[16] = add_adjusted_asteroid(55316.482176, 0.000000, 70384.417361);
	set_asteroid_speed(asteroid[16], 1.000000);
	asteroid[17] = add_adjusted_asteroid(55607.178206, 0.000000, 60992.366835);
	set_asteroid_speed(asteroid[17], 1.000000);
	asteroid[18] = add_adjusted_asteroid(69159.345715, 0.000000, 73726.640946);
	set_asteroid_speed(asteroid[18], 1.000000);
	asteroid[19] = add_adjusted_asteroid(93506.873686, 0.000000, 78459.411383);
	set_asteroid_speed(asteroid[19], 1.000000);
	asteroid[20] = add_adjusted_asteroid(143179.155766, 0.000000, 77985.358303);
	set_asteroid_speed(asteroid[20], 1.000000);
	asteroid[21] = add_adjusted_asteroid(149703.551926, 0.000000, 74042.224304);
	set_asteroid_speed(asteroid[21], 1.000000);
	asteroid[22] = add_adjusted_asteroid(133004.679237, 0.000000, 77537.748033);
	set_asteroid_speed(asteroid[22], 1.000000);
	asteroid[23] = add_adjusted_asteroid(121313.609847, 0.000000, 82049.372151);
	set_asteroid_speed(asteroid[23], 1.000000);
	asteroid[24] = add_adjusted_asteroid(117029.253653, 0.000000, 89757.223943);
	set_asteroid_speed(asteroid[24], 1.000000);
	asteroid[25] = add_adjusted_asteroid(98795.327096, 0.000000, 101109.411673);
	set_asteroid_speed(asteroid[25], 1.000000);
	asteroid[26] = add_adjusted_asteroid(82479.384003, 0.000000, 104499.909150);
	set_asteroid_speed(asteroid[26], 1.000000);
	asteroid[27] = add_adjusted_asteroid(70083.282422, 0.000000, 97047.632977);
	set_asteroid_speed(asteroid[27], 1.000000);
	asteroid[28] = add_adjusted_asteroid(52976.707607, 0.000000, 100888.133568);
	set_asteroid_speed(asteroid[28], 1.000000);
	asteroid[29] = add_adjusted_asteroid(47503.670678, 0.000000, 108680.531530);
	set_asteroid_speed(asteroid[29], 1.000000);
	asteroid[30] = add_adjusted_asteroid(71080.184301, 0.000000, 108939.851285);
	set_asteroid_speed(asteroid[30], 1.000000);
	asteroid[31] = add_adjusted_asteroid(94066.727905, 0.000000, 107655.509851);
	set_asteroid_speed(asteroid[31], 1.000000);
	asteroid[32] = add_adjusted_asteroid(110670.538650, 0.000000, 100106.460551);
	set_asteroid_speed(asteroid[32], 1.000000);
	asteroid[33] = add_adjusted_asteroid(116636.218148, 0.000000, 89282.544499);
	set_asteroid_speed(asteroid[33], 1.000000);
	asteroid[34] = add_adjusted_asteroid(513708.648407, 0.000000, 105873.460767);
	set_asteroid_speed(asteroid[34], 1.000000);
	asteroid[35] = add_adjusted_asteroid(535254.825130, 0.000000, 128718.825732);
	set_asteroid_speed(asteroid[35], 1.000000);
	asteroid[36] = add_adjusted_asteroid(543835.718675, 0.000000, 114837.334126);
	set_asteroid_speed(asteroid[36], 1.000000);
	asteroid[37] = add_adjusted_asteroid(514138.086417, 0.000000, 108154.125783);
	set_asteroid_speed(asteroid[37], 1.000000);
	asteroid[38] = add_adjusted_asteroid(467115.242854, 0.000000, 99303.738613);
	set_asteroid_speed(asteroid[38], 1.000000);
	asteroid[39] = add_adjusted_asteroid(489546.791367, 0.000000, 105788.939210);
	set_asteroid_speed(asteroid[39], 1.000000);
	asteroid[40] = add_adjusted_asteroid(508270.523903, 0.000000, 109012.578550);
	set_asteroid_speed(asteroid[40], 1.000000);
	asteroid[41] = add_adjusted_asteroid(531519.943940, 0.000000, 102360.738685);
	set_asteroid_speed(asteroid[41], 1.000000);
	asteroid[42] = add_adjusted_asteroid(511436.659534, 0.000000, 78963.534559);
	set_asteroid_speed(asteroid[42], 1.000000);
	asteroid[43] = add_adjusted_asteroid(494404.233334, 0.000000, 65590.250270);
	set_asteroid_speed(asteroid[43], 1.000000);
	asteroid[44] = add_adjusted_asteroid(462305.653466, 0.000000, 86549.323976);
	set_asteroid_speed(asteroid[44], 1.000000);
	asteroid[45] = add_adjusted_asteroid(450171.577896, 0.000000, 107533.090042);
	set_asteroid_speed(asteroid[45], 1.000000);
	asteroid[46] = add_adjusted_asteroid(469457.506098, 0.000000, 120521.095738);
	set_asteroid_speed(asteroid[46], 1.000000);
	asteroid[47] = add_adjusted_asteroid(485587.021865, 0.000000, 121013.669529);
	set_asteroid_speed(asteroid[47], 1.000000);
	asteroid[48] = add_adjusted_asteroid(512300.181898, 0.000000, 122730.512263);
	set_asteroid_speed(asteroid[48], 1.000000);
	asteroid[49] = add_adjusted_asteroid(518777.681486, 0.000000, 117789.979278);
	set_asteroid_speed(asteroid[49], 1.000000);
	asteroid[50] = add_adjusted_asteroid(519479.747408, 0.000000, 114893.436996);
	set_asteroid_speed(asteroid[50], 1.000000);
	asteroid[51] = add_adjusted_asteroid(545513.117869, 0.000000, 107371.791756);
	set_asteroid_speed(asteroid[51], 1.000000);
	asteroid[52] = add_adjusted_asteroid(559752.980490, 0.000000, 104994.409951);
	set_asteroid_speed(asteroid[52], 1.000000);
	asteroid[53] = add_adjusted_asteroid(579048.405742, 0.000000, 85694.593634);
	set_asteroid_speed(asteroid[53], 1.000000);
	asteroid[54] = add_adjusted_asteroid(586793.637894, 0.000000, 78059.641516);
	set_asteroid_speed(asteroid[54], 1.000000);
	asteroid[55] = add_adjusted_asteroid(559776.417508, 0.000000, 58472.343005);
	set_asteroid_speed(asteroid[55], 1.000000);
	asteroid[56] = add_adjusted_asteroid(527772.265218, 0.000000, 55743.817305);
	set_asteroid_speed(asteroid[56], 1.000000);
	asteroid[57] = add_adjusted_asteroid(526687.105425, 0.000000, 56282.606167);
	set_asteroid_speed(asteroid[57], 1.000000);
	asteroid[58] = add_adjusted_asteroid(531443.392211, 0.000000, 69459.686113);
	set_asteroid_speed(asteroid[58], 1.000000);
	asteroid[59] = add_adjusted_asteroid(553037.536943, 0.000000, 73564.049445);
	set_asteroid_speed(asteroid[59], 1.000000);
	asteroid[60] = add_adjusted_asteroid(554327.107048, 0.000000, 79836.084621);
	set_asteroid_speed(asteroid[60], 1.000000);
	asteroid[61] = add_adjusted_asteroid(531003.818606, 0.000000, 81808.560570);
	set_asteroid_speed(asteroid[61], 1.000000);
	asteroid[62] = add_adjusted_asteroid(496522.164740, 0.000000, 75787.883271);
	set_asteroid_speed(asteroid[62], 1.000000);
	asteroid[63] = add_adjusted_asteroid(493027.124032, 0.000000, 83576.901638);
	set_asteroid_speed(asteroid[63], 1.000000);
	asteroid[64] = add_adjusted_asteroid(503680.099142, 0.000000, 97043.220784);
	set_asteroid_speed(asteroid[64], 1.000000);
	asteroid[65] = add_adjusted_asteroid(514967.796824, 0.000000, 103862.404751);
	set_asteroid_speed(asteroid[65], 1.000000);
	asteroid[66] = add_adjusted_asteroid(520578.345588, 0.000000, 108055.782871);
	set_asteroid_speed(asteroid[66], 1.000000);
	asteroid[67] = add_adjusted_asteroid(528398.734999, 0.000000, 109934.997894);
	set_asteroid_speed(asteroid[67], 1.000000);
	asteroid[68] = add_adjusted_asteroid(528998.850002, 0.000000, 120221.073369);
	set_asteroid_speed(asteroid[68], 1.000000);
	asteroid[69] = add_adjusted_asteroid(456360.300612, 0.000000, 509725.586606);
	set_asteroid_speed(asteroid[69], 1.000000);
	asteroid[70] = add_adjusted_asteroid(498386.231450, 0.000000, 501860.638486);
	set_asteroid_speed(asteroid[70], 1.000000);
	asteroid[71] = add_adjusted_asteroid(514874.593898, 0.000000, 499819.438861);
	set_asteroid_speed(asteroid[71], 1.000000);
	asteroid[72] = add_adjusted_asteroid(522255.963033, 0.000000, 512272.356393);
	set_asteroid_speed(asteroid[72], 1.000000);
	asteroid[73] = add_adjusted_asteroid(532142.201717, 0.000000, 543125.420590);
	set_asteroid_speed(asteroid[73], 1.000000);
	asteroid[74] = add_adjusted_asteroid(531872.544550, 0.000000, 559174.972425);
	set_asteroid_speed(asteroid[74], 1.000000);
	asteroid[75] = add_adjusted_asteroid(440934.528979, 0.000000, 571609.145182);
	set_asteroid_speed(asteroid[75], 1.000000);
	asteroid[76] = add_adjusted_asteroid(436760.778363, 0.000000, 565651.098382);
	set_asteroid_speed(asteroid[76], 1.000000);
	asteroid[77] = add_adjusted_asteroid(430707.605302, 0.000000, 524276.186723);
	set_asteroid_speed(asteroid[77], 1.000000);
	asteroid[78] = add_adjusted_asteroid(440988.490088, 0.000000, 503229.645467);
	set_asteroid_speed(asteroid[78], 1.000000);
	asteroid[79] = add_adjusted_asteroid(414362.505646, 0.000000, 464994.345905);
	set_asteroid_speed(asteroid[79], 1.000000);
	asteroid[80] = add_adjusted_asteroid(369802.146369, 0.000000, 452630.956399);
	set_asteroid_speed(asteroid[80], 1.000000);
	asteroid[81] = add_adjusted_asteroid(320891.860341, 0.000000, 468842.266082);
	set_asteroid_speed(asteroid[81], 1.000000);
	asteroid[82] = add_adjusted_asteroid(247015.176190, 0.000000, 458099.363081);
	set_asteroid_speed(asteroid[82], 1.000000);
	asteroid[83] = add_adjusted_asteroid(228439.593784, 0.000000, 464539.914206);
	set_asteroid_speed(asteroid[83], 1.000000);
	asteroid[84] = add_adjusted_asteroid(272220.001790, 0.000000, 467282.597064);
	set_asteroid_speed(asteroid[84], 1.000000);
	asteroid[85] = add_adjusted_asteroid(328613.700565, 0.000000, 499031.886322);
	set_asteroid_speed(asteroid[85], 1.000000);
	asteroid[86] = add_adjusted_asteroid(360924.117768, 0.000000, 513445.485865);
	set_asteroid_speed(asteroid[86], 1.000000);
	asteroid[87] = add_adjusted_asteroid(389496.927886, 0.000000, 510859.238493);
	set_asteroid_speed(asteroid[87], 1.000000);
	asteroid[88] = add_adjusted_asteroid(421978.372657, 0.000000, 502809.626151);
	set_asteroid_speed(asteroid[88], 1.000000);
	asteroid[89] = add_adjusted_asteroid(462524.651660, 0.000000, 491361.599315);
	set_asteroid_speed(asteroid[89], 1.000000);
	asteroid[90] = add_adjusted_asteroid(500758.858402, 0.000000, 494378.921726);
	set_asteroid_speed(asteroid[90], 1.000000);

	nebula[1] = add_nebula("", adjx + 300000.311946, 836999.374273, adjz + 299999.624630, 439.000000);
	nebula[2] = add_nebula("", adjx + 478825.499340, 7964.717973, adjz + 121295.405981, 3348.000000);
	nebula[3] = add_nebula("", adjx + 481241.421998, 0.000000, adjz + 120925.756228, 2913.000000);
end


player_ids = get_player_ship_ids();
if (player_ids[1]) then
	reset_player_ship(player_ids[1]);
	move_object(player_ids[1], adjx + 0, 0, adjz + 0); -- move player out of way of planets
end

set_up_environment();
set_variable("SUPPRESS_STARBASE_COMPLAINTS", "1");
set_variable("MISSILE_FIRE_CHANCE", "0");

-- set player 1 location
if (player_ids[1]) then
	dock_player_to_starbase(player_ids[1], starbase[3]);
	x,y,z = get_object_location(player_ids[1]);
	x = x + 10000;
	add_cubo_de_la_muerte(x, y, z);
end

last_message = 1;

audio_message = {
	"sounds/admiral-thompson-msg-1.ogg",
	"sounds/admiral-thompson-msg-2.ogg",
	"sounds/admiral-thompson-msg-3.ogg",
	"sounds/admiral-thompson-msg-4.ogg",
};

function destroy_elnox(cookie)
	cx, cy, cz = get_object_location(cubo);
	ex, ey, ez = get_object_location(planet[2]);
	nx = 0.8 * (ex - cx) + cx;
	ny = 0.8 * (ey - cy) + cy;
	nz = 0.8 * (ez - cz) + cz;
	move_object(cubo, nx, ny, nz);
	delete_object(planet[2]);
	delete_object(starbase[3]);
	add_explosion(ex, ey, ez, 150, 350, 250, 13); -- 13 is planet
	text_to_speech(player_ids[1], "Elnox has been destroyed. You have failed the mission.");
end

function transmit_cubo_location(cookie)
	x, y, z = get_object_location(planet[2]); -- LOCATION ELNOX
	dx, dy, dz = random_point_on_sphere(200000);
	x = x + dx;
	y = y + dy;
	z = z + dz;
	move_object(cubo, x, y, z);
	comms_transmission(starbase[3], "THE LAST KNOWN LOCATION OF THE DEATH CUBE WAS");
	comms_transmission(starbase[3], string.format("( %d , %d , %d )", x, y, z));
	register_timer_callback("destroy_elnox", elnox_time_limit, 0);
end

function send_audio_message(to_ship_id, from_id, from_name, audio_message_number)
	comms_transmission(from_id, "INCOMING AUDIO TRANSMISSION FROM");
	comms_transmission(from_id, string.format("%s ( USE PLAY MESSAGE BUTTON )", from_name));
	play_sound(to_ship_id, "sounds/quindar-intro.ogg");
	last_message = audio_message_number;
	set_custom_button_label(to_ship_id, 5, "PLAY MESSAGE");
	enable_custom_button(to_ship_id, 5);
end

function scheduled_audio_message(message_number)
	send_audio_message(player_ids[1], starbase[3], "ADMIRAL THOMPSON", message_number);
end

function play_message_button_pressed(ship_id, screen)
	if screen ~= 5 then
		return;
	end
	play_sound(ship_id, audio_message[last_message]);
end

black_hole_generator = add_commodity("BLACK HOLE GENERATOR",
		"UNIT", "BLACK HOLE GENERATOR", 100000.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
bhg_cargo_container = -1;

function add_zarkon_fleet(x, y, z)
	shipname = { "KRONBO", "BLANTO", "GURFON", "ZLERBA", "GORBOZ"};
	for i = 1, 5 do
		a, b, c = random_point_on_sphere(500);
		zarkon[i] = add_ship(shipname[i], x + a, y + b, z + c, 11, 3, 0);
		x1,y1,z1 = get_object_location(starbase[1]);
		x2,y2,z2 = get_object_location(starbase[2]);
		ai_push_patrol(zarkon[i], 2, x1 + 400, y1 + 400, z1, x2 + 400, y2 + 400, z2);
	end
	-- Hmm, I should probably give lua a way to get random numbers.
	a, b, c = random_point_on_sphere(1);
	if a < 0 then
		set_commodity_contents(zarkon[1], black_hole_generator, 1, 0); -- KRONBO has it
	else
		set_commodity_contents(zarkon[2], black_hole_generator, 1, 0); -- BLANTO has it
	end
end

-- returns true if player has the black hole generator in one of his cargo
-- bays, false otherwise.
function player_has_recovered_weapon(id)
	ncargo_bays = get_ship_attribute(id, "ncargo_bays");
	if ncargo_bays == nil then
		return false;
	end
	for i = 0, ncargo_bays - 1 do
		cargo_bay_item_str = string.format("cargo[%d].contents.item", i);
		cargo_bay_qty_str = string.format("cargo[%d].contents.qty", i);
		contents = get_ship_attribute(id, cargo_bay_item_str);
		qty = get_ship_attribute(id, cargo_bay_qty_str);
		if contents == black_hole_generator and qty > 0 then
			return true;
		end
	end
	return false;
end

already_docked_with_weapon = false;


-- This gets called whenever a player docks with a starbase
function player_docked_callback(player, sb)

	if sb ~= starbase[3] then
		return; -- we only care about starbase 2
	end

	if already_docked_with_weapon then
		return;
	end

	if not player_has_recovered_weapon(player) then
		return;
	end;

	-- At this point, we know that this is the first time the player has docked with
	-- starbase2 while carrying the black hole generator.

	already_docked_with_weapon = false; -- ensure the following msg happens only once.
	register_timer_callback("scheduled_audio_message", 10 * secs, 2);
	register_timer_callback("transmit_cubo_location", 15 * secs, 0);
end

function player_ejected_cargo_callback(ship_id, cargo_id, x, y, z, item, qty)
	if ship_id ~= player_ids[1] then
		return;
	end

	if cargo_id < 0 then
		-- Uh oh, this means the cargo container didn't get allocated and the
		-- black hole generator is lost. It really shouldn't happen, esp. since
		-- this scenario runs with a low number of objects in the universe.
		demon_print("OOPS WE HAVE A PROBLEM, THE BLACK HOLE GENERATOR WAS LOST.");
		return;
	end

	if item == black_hole_generator and qty > 0 then
		bhg_cargo_container = cargo_id;
		set_object_velocity(cargo_id, 0, 0, 0);
	end
end

already_gave_bhg_operating_instructions = false;
function check_bhg_position(cookie)

	register_timer_callback("check_bhg_position", 10, 0); -- schedule next callback

	-- If the BHG cargo container is within 1000 units of the CUBO
	-- give the player instructions about how to detonate the BHG.

	if bhg_cargo_container < 0 then
		-- BHG hasn't been ejected by player yet.
		return;
	end

	d = object_distance(bhg_cargo_container, cubo);
	if (d == nil) then
		-- maybe the BHG has been ejected but then scooped up by the player
		return;
	end

	if d < 1000.0 and not already_gave_bhg_operating_instructions then
		already_gave_bhg_operating_instructions = true;
		register_timer_callback("scheduled_audio_message", 10 * secs, 3);
	end
end

the_black_hole = -1;
bhg_state = 0;
bhg_channel = 666
bhg_msg = "123"
bhg_name = "BHG 1.7";

function bhg_countdown(count)
	comms_channel_transmit(bhg_name, bhg_channel, string.format("T MINUS %d", count));
	if count == 0 then
		demon_print("BHG COUNTDOWN REACHED ZERO");
		x, y, z = get_object_location(bhg_cargo_container);
		if (x == nil) then
			demon_print("BHG CC LOCATION IS NOT KNOWN");
		else
			demon_print("FOUND BHG CC LOCATION");
		end
		delete_object(bhg_cargo_container);
		bhg_cargo_container = -1;
		if x ~= nil then
			demon_print("ADDING BLACKHOLE");
			the_black_hole = add_black_hole("BHG", x, y, z, 3000);
			if the_black_hole < 0 then
				demon_print("FAILED TO CREATE BLACK HOLE");
			else
				demon_print("ADDED BLACKHOLE");
			end
			bhg_state = 0;
			d = object_distance(the_black_hole, cubo);
			if d < 3000 then
				move_object(cubo, x + 1, y + 1, z + 1); -- should make the black hole eat the cubo
			end
		else
			demon_print("NOT ADDING BLACKHOLE");
		end
	else
		register_timer_callback("bhg_countdown", 10, count - 1);
	end
end

function bhg_listener(from, channel, message)

	-- Ignore our own transmissions
	if from == bhg_name or from == "" then
		return;
	end;

	if channel ~= bhg_channel then
		return;
	end;

	if bhg_cargo_container == -1 then
		comms_channel_transmit("", bhg_channel, "... no response ...");
		return;
	end;

	if message == bhg_msg then
		bhg_state = 1;
	end;

	if bhg_state == 1 then
		comms_channel_transmit(bhg_name, bhg_channel,
			"WELCOME! I AM A MEGAMASS INDUSTRIES BLACK HOLE GENERATOR!");
		comms_channel_transmit(bhg_name, bhg_channel,
			"BEFORE WE GET STARTED, I JUST HAVE A FEW QUESTIONS.");
		comms_channel_transmit(bhg_name, bhg_channel,
			"FOR WHAT PURPOSE DO YOU WISH TO CREATE A BLACK HOLE?");
		bhg_state = 2;
		return;
	end;

	if bhg_state == 2 then
		comms_channel_transmit(bhg_name, bhg_channel, "I SEE. THAT SOUNDS LOVELY.");
		comms_channel_transmit(bhg_name, bhg_channel, "AND WHAT IS YOUR NAME?");
		bhg_state = 3;
		return;
	end;

	if bhg_state == 3 then
		comms_channel_transmit(bhg_name, bhg_channel, "WHAT A NICE NAME.");
		comms_channel_transmit(bhg_name, bhg_channel, "AND ARE YOU REALLY SURE YOU WANT TO CREATE A BLACK HOLE?");
		bhg_state = 4;
		return;
	end;

	if bhg_state == 4 then
		if string.upper(message) == "NO" then
			comms_channel_transmit(bhg_name, bhg_channel, "IN THAT CASE WE WON'T DO IT.  THANK YOU.");
			bhg_state = 0;
			return;
		else if string.upper(message) == "YES" then
				comms_channel_transmit(bhg_name, bhg_channel, "OK.");
				comms_channel_transmit(bhg_name, bhg_channel, "WHAT IS YOUR FAVORITE COLOR?");
				bhg_state = 5;
				return;
			else
				comms_channel_transmit(bhg_name, bhg_channel, "PLEASE ANSWER YES OR NO");
				comms_channel_transmit(bhg_name, bhg_channel, "ARE YOU REALLY SURE YOU WANT TO CREATE A BLACK HOLE?");
				return;
			end;
		end;
	end;

	if bhg_state == 5 then
		comms_channel_transmit(bhg_name, bhg_channel, "WHAT IS 6 TIMES 7?");
		bhg_state = 6;
		return;
	end;

	if bhg_state == 6 then
		if message == "42" then
			comms_channel_transmit(bhg_name, bhg_channel, "QUITE AN IMPORTANT NUMBER, DON'T YOU AGREE?")
			bhg_state = 7;
		else
			comms_channel_transmit(bhg_name, bhg_channel, "DO YOU REALLY THINK SO?");
			bhg_state = 7;
		end
		return;
	end;

	if bhg_state == 7 then
		if string.upper(message) == "NO" or string.upper(message) == "YES" then
			comms_channel_transmit(bhg_name, bhg_channel, "WELL I SUPPOSE I SHOULD GET ON WITH IT...");
			comms_channel_transmit(bhg_name, bhg_channel, "GOODBYE, CRUEL WORLD...");
			comms_channel_unlisten(bhg_name, bhg_channel);
			register_timer_callback("bhg_countdown", 10, 10);
			bhg_state = 8;
			return;
		else
			comms_channel_transmit(bhg_name, bhg_channel, "PLEASE ANSWER YES OR NO");
			return;
		end;
	end;
end;

function black_hole_consumed_object(black_hole_id, object_id, object_type)
	if the_black_hole == -1 then
		return;
	end;
	if black_hole_id ~= the_black_hole then
		return;
	end;
	if object_id == cubo then
		register_timer_callback("scheduled_audio_message", 10 * secs, 4);
		cubo = -2;
	end
end;

function player_death(object_id)
	if object_id ~= player_ids[1] then
		return;
	end;
	if cubo ~= -2 then
		text_to_speech(player_ids[1], "You have failed the mission.");
	else
		text_to_speech(player_ids[1], "Although you and your ship have been destroyed, so was the death cube.");
		text_to_speech(player_ids[1], "You died with honor.");
	end
end

initial_audio_message_time = 5 * secs;
add_zarkon_fleet(0, 0, 0);
comms_channel_listen("MEGAMASS BHG 1.7", 123, "bhg_listener");

register_timer_callback("scheduled_audio_message", initial_audio_message_time, 1);
register_timer_callback("check_bhg_position", 10, 0);
register_callback("custom-button-press-event", "play_message_button_pressed");
register_callback("player-docked-event", "player_docked_callback");
register_callback("player-ejected-cargo-event", "player_ejected_cargo_callback");
register_callback("black-hole-consumed-object-event", "black_hole_consumed_object");
register_callback("player-death-callback", "player_death");
