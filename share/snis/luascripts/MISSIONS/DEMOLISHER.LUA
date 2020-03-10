-- demolisher mission (work in progress)
clear_all();

-- find the player
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    player_name = get_object_name(player_ids[1]);
    x, y, z = get_object_location(player_ids[1]);
end

days_left = 30;

door1 = -1;
door2 = -1;
door3 = -1;
door4 = -1;
demolisher_id = -1;

adjx = -300000;
adjz = -300000;

math.randomseed(os.time())

function alphachar(n)
  return string.char(n % 26 + 65)
end

function string.random(length)
  if length == 0 then
    return ""
  end
  return string.random(length - 1) .. alphachar(math.random(0, 25))
end

tmpstr=string.random(24)
challenge=string.sub(tmpstr, 1, 8)
challenge_answer=string.sub(tmpstr, 9, 16)
opencmd=string.sub(tmpstr, 17, 20)
closecmd=string.sub(tmpstr, 21, 24)

door_channel = 1000 + math.random(8000)
sb01channel1 = 1000 + math.random(8000)
sb01channel2 = 1000 + math.random(8000)

demox = 100000 - adjx;
demoy = 100000;
demoz = 1000 - adjz;

function add_demolisher_ship(x, y, z)

  m = 500;
  l = 20 * m;
  w = 3 * m;
  thick = 0.3 * m;
  degrees = (math.pi / 180.0);
  zfight = 10;

  -- This is our main, unparented block of the ship to which all other
  -- blocks are attached:
  id = add_block(-1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0);
  set_object_rotational_velocity(id, 1, 0, 0, 0);  -- Stop default spinning
  set_object_orientation(id, 1, 0, 0, 0);  -- unrotated

  top = add_block(id, 0, 0.3 * w, 0, l * 1.1, thick, w, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(top, 3, 8, 2);
  bottom = add_block(id, 0, -0.3 * w, 0, l, thick, w, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(bottom, 2, 8, 2);

  -- first side
  add_block(id, 0,  0.15 * w, 0.8 * w, l, thick, w, 1, 0, 0,  20 * degrees, 0, 0);
  add_block(id, 0, -0.15 * w, 0.8 * w, l * 1.1, thick, w, 1, 0, 0, -20 * degrees, 0, 0);

  -- second side
  add_block(id, 0,  0.15 * w, -0.8 * w, l, thick, w, 1, 0, 0, -20 * degrees, 0, 0);
  add_block(id, 0, -0.15 * w, -0.8 * w, l * 1.1, thick, w, 1, 0, 0,  20 * degrees, 0, 0);

  wallheight = 0.5 * w;
  walllength = w * 1.0;
  walloffset = w * 1.0;

  -- ribs
  door1 = add_block(id, 0.45 * l + zfight, 0, 0.55 * -walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0, 0);
  door2 = add_block(id, 0.45 * l, 0, 0.55 *  walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0, 0);
  b = add_block(id, 0.35 * l, 0, 0.75 * -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  b = add_block(id, 0.35 * l, 0, 0.75 *  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);

  b = add_block(id, 0.25 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, 0.25 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, 0.15 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, 0.15 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, 0.05 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, 0.05 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, -0.05 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, -0.05 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, -0.15 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, -0.15 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, -0.25 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, -0.25 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);

  b = add_block(id, -0.35 * l, 0, 0.75 * -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  b = add_block(id, -0.35 * l, 0, 0.75 *  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0, 0);
  door3 = add_block(id, -0.45 * l + zfight, 0, 0.55 * -walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0, 0);
  door4 = add_block(id, -0.45 * l, 0, 0.55 *  walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0, 0);


  move_object(id, x, y, z);
  return id;

end


door_speed = 0.01;
door_open = 0.0;
door_direction = 0.0;

function set_door_position(door_open)
  set_object_relative_position(door1,  0.45 * l + zfight, 0, 0.55 * -walloffset - door_open * 0.3 * walloffset);
  set_object_relative_position(door2,  0.45 * l, 0, 0.55 *  walloffset + door_open * 0.3 * walloffset);
  set_object_relative_position(door3, -0.45 * l + zfight, 0, 0.55 * -walloffset - door_open * 0.3 * walloffset);
  set_object_relative_position(door4, -0.45 * l, 0, 0.55 *  walloffset + door_open * 0.3 * walloffset);
end

function door_mover(cookie)
  door_open = door_open + door_direction;
  if door_open > 1.0 and door_direction > 0.0 then
    door_direction = 0;
  end
  if door_open < 0.0 and door_direction < 0.0 then
    door_direction = 0;
  end
  set_door_position(door_open);
  register_timer_callback("door_mover", 1, 0);
end

function door_comms(from, channel, message)
  if message == opencmd then
    comms_channel_transmit("DEMOLISHER: ", door_channel, challenge);
  elseif message == challenge_answer then
    door_direction = door_speed;
    comms_channel_transmit("DEMOLISHER: ", door_channel, "COMMAND ACKNOWLEDGED, DOORS OPENING");
  end
  if message == closecmd then
    door_direction = -door_speed;
    comms_channel_transmit("DEMOLISHER: ", door_channel, "COMMAND ACKNOWLEDGED, DOORS CLOSING");
  end
end

show_timed_text(player_ids[1], 15,
        "DEMOLISHER\n" ..
        "A GIANT ALIEN SHIP HEADS TOWARD ZETA SCORPIO\n")

function add_adjusted_asteroid(x, y, z)
  return add_asteroid(x + adjx, y, z + adjz);
end

function add_adjusted_nebula(name, x, y, z, size)
  return add_nebula(name, x + adjx, y, z + adjx, size);
end

function add_adjusted_ship(name, x, y, z, type, faction, auto_respawn)
  add_ship(name, adjx + x, y, adjz + z, type, faction, auto_respawn)
end

-- Dump a bunch of stuff into the universe...

planet = {};
planet[1] = add_planet("ORBO", adjx + 230191.279310, 0.000000, adjz + 35491.815598, 5197.000000, 0);
planet[2] = add_planet("ZETA SCORPIO", adjx + 37599.374744, 0.000000, adjz + 518691.215533, 6148.000000, 0);
asteroid = {};
asteroid[1] = add_adjusted_asteroid(218089.092942, 0.000000, 538767.117693);
set_asteroid_speed(asteroid[1], 1.000000);
asteroid[2] = add_adjusted_asteroid(196327.230145, 0.000000, 524045.165924);
set_asteroid_speed(asteroid[2], 1.000000);
asteroid[3] = add_adjusted_asteroid(141543.169936, 0.000000, 480521.190809);
set_asteroid_speed(asteroid[3], 1.000000);
asteroid[4] = add_adjusted_asteroid(116971.120209, 0.000000, 451777.908595);
set_asteroid_speed(asteroid[4], 1.000000);
asteroid[5] = add_adjusted_asteroid(109315.915242, 0.000000, 430252.717988);
set_asteroid_speed(asteroid[5], 1.000000);
asteroid[6] = add_adjusted_asteroid(123126.746175, 0.000000, 447440.930596);
set_asteroid_speed(asteroid[6], 1.000000);
asteroid[7] = add_adjusted_asteroid(181995.372057, 0.000000, 547155.178704);
set_asteroid_speed(asteroid[7], 1.000000);
asteroid[8] = add_adjusted_asteroid(159417.592590, 0.000000, 517316.841535);
set_asteroid_speed(asteroid[8], 1.000000);
asteroid[9] = add_adjusted_asteroid(127901.007257, 0.000000, 456454.309465);
set_asteroid_speed(asteroid[9], 1.000000);
asteroid[10] = add_adjusted_asteroid(108884.710122, 0.000000, 403167.911155);
set_asteroid_speed(asteroid[10], 1.000000);
asteroid[11] = add_adjusted_asteroid(110246.967529, 0.000000, 341652.847499);
set_asteroid_speed(asteroid[11], 1.000000);
asteroid[12] = add_adjusted_asteroid(118206.126864, 0.000000, 257143.122759);
set_asteroid_speed(asteroid[12], 1.000000);
asteroid[13] = add_adjusted_asteroid(76885.520384, 0.000000, 216452.545072);
set_asteroid_speed(asteroid[13], 1.000000);
asteroid[14] = add_adjusted_asteroid(51887.335939, 0.000000, 208723.674500);
set_asteroid_speed(asteroid[14], 1.000000);
asteroid[15] = add_adjusted_asteroid(81440.490540, 0.000000, 291373.141665);
set_asteroid_speed(asteroid[15], 1.000000);
asteroid[16] = add_adjusted_asteroid(100686.025602, 0.000000, 329474.093215);
set_asteroid_speed(asteroid[16], 1.000000);
asteroid[17] = add_adjusted_asteroid(109357.949979, 0.000000, 142889.408150);
set_asteroid_speed(asteroid[17], 1.000000);
asteroid[18] = add_adjusted_asteroid(152458.802410, 0.000000, 121203.119804);
set_asteroid_speed(asteroid[18], 1.000000);
asteroid[19] = add_adjusted_asteroid(173851.574867, 0.000000, 114372.814248);
set_asteroid_speed(asteroid[19], 1.000000);
asteroid[20] = add_adjusted_asteroid(229389.372157, 0.000000, 100696.631268);
set_asteroid_speed(asteroid[20], 1.000000);
asteroid[21] = add_adjusted_asteroid(334060.593094, 0.000000, 110262.073519);
set_asteroid_speed(asteroid[21], 1.000000);
asteroid[22] = add_adjusted_asteroid(429114.227828, 0.000000, 73947.193622);
set_asteroid_speed(asteroid[22], 1.000000);
asteroid[23] = add_adjusted_asteroid(437537.517620, 0.000000, 70590.981232);
set_asteroid_speed(asteroid[23], 1.000000);
asteroid[24] = add_adjusted_asteroid(323858.002560, 0.000000, 76850.700123);
set_asteroid_speed(asteroid[24], 1.000000);
asteroid[25] = add_adjusted_asteroid(395599.449767, 0.000000, 120683.593754);
set_asteroid_speed(asteroid[25], 1.000000);
asteroid[26] = add_adjusted_asteroid(453649.514499, 0.000000, 153077.781708);
set_asteroid_speed(asteroid[26], 1.000000);
asteroid[27] = add_adjusted_asteroid(485986.403323, 0.000000, 207721.996309);
set_asteroid_speed(asteroid[27], 1.000000);
asteroid[28] = add_adjusted_asteroid(495759.497008, 0.000000, 232146.217254);
set_asteroid_speed(asteroid[28], 1.000000);
asteroid[29] = add_adjusted_asteroid(505284.850579, 0.000000, 163036.941439);
set_asteroid_speed(asteroid[29], 1.000000);
asteroid[30] = add_adjusted_asteroid(520252.513466, 0.000000, 213914.718584);
set_asteroid_speed(asteroid[30], 1.000000);
asteroid[31] = add_adjusted_asteroid(516520.564571, 0.000000, 304471.745352);
set_asteroid_speed(asteroid[31], 1.000000);
asteroid[32] = add_adjusted_asteroid(536041.834877, 0.000000, 345412.918002);
set_asteroid_speed(asteroid[32], 1.000000);
asteroid[33] = add_adjusted_asteroid(537134.060923, 0.000000, 445423.908820);
set_asteroid_speed(asteroid[33], 1.000000);
asteroid[34] = add_adjusted_asteroid(534266.051493, 0.000000, 453847.407438);
set_asteroid_speed(asteroid[34], 1.000000);
asteroid[35] = add_adjusted_asteroid(482023.634987, 0.000000, 503520.687584);
set_asteroid_speed(asteroid[35], 1.000000);
asteroid[36] = add_adjusted_asteroid(404621.135289, 0.000000, 508740.319450);
set_asteroid_speed(asteroid[36], 1.000000);
asteroid[37] = add_adjusted_asteroid(369644.749890, 0.000000, 518034.287913);
set_asteroid_speed(asteroid[37], 1.000000);
asteroid[38] = add_adjusted_asteroid(471376.088947, 0.000000, 473725.762026);
set_asteroid_speed(asteroid[38], 1.000000);
asteroid[39] = add_adjusted_asteroid(531376.616001, 0.000000, 436880.918643);
set_asteroid_speed(asteroid[39], 1.000000);
asteroid[40] = add_adjusted_asteroid(541871.280704, 0.000000, 414878.617482);
set_asteroid_speed(asteroid[40], 1.000000);
asteroid[41] = add_adjusted_asteroid(536579.621672, 0.000000, 414891.234164);
set_asteroid_speed(asteroid[41], 1.000000);
asteroid[42] = add_adjusted_asteroid(480653.887204, 0.000000, 530134.089571);
set_asteroid_speed(asteroid[42], 1.000000);
asteroid[43] = add_adjusted_asteroid(439107.784350, 0.000000, 572280.537387);
set_asteroid_speed(asteroid[43], 1.000000);
asteroid[44] = add_adjusted_asteroid(346894.879248, 0.000000, 574192.571834);
set_asteroid_speed(asteroid[44], 1.000000);
asteroid[45] = add_adjusted_asteroid(270216.200263, 0.000000, 574391.761077);
set_asteroid_speed(asteroid[45], 1.000000);
asteroid[46] = add_adjusted_asteroid(190092.121559, 0.000000, 575674.592169);
set_asteroid_speed(asteroid[46], 1.000000);
asteroid[47] = add_adjusted_asteroid(170662.576570, 0.000000, 568898.196363);
set_asteroid_speed(asteroid[47], 1.000000);
asteroid[48] = add_adjusted_asteroid(175183.094584, 0.000000, 533267.764125);
set_asteroid_speed(asteroid[48], 1.000000);
asteroid[49] = add_adjusted_asteroid(165997.315325, 0.000000, 526837.598139);
set_asteroid_speed(asteroid[49], 1.000000);
asteroid[50] = add_adjusted_asteroid(147429.663507, 0.000000, 526130.480734);
set_asteroid_speed(asteroid[50], 1.000000);
asteroid[51] = add_adjusted_asteroid(139741.131153, 0.000000, 527808.156275);
set_asteroid_speed(asteroid[51], 1.000000);
asteroid[52] = add_adjusted_asteroid(96698.271390, 0.000000, 534584.409002);
set_asteroid_speed(asteroid[52], 1.000000);
asteroid[53] = add_adjusted_asteroid(70506.199559, 0.000000, 534907.391753);
set_asteroid_speed(asteroid[53], 1.000000);
asteroid[54] = add_adjusted_asteroid(55291.336256, 0.000000, 539849.967225);
set_asteroid_speed(asteroid[54], 1.000000);
asteroid[55] = add_adjusted_asteroid(61146.997148, 0.000000, 497217.308671);
set_asteroid_speed(asteroid[55], 1.000000);
asteroid[56] = add_adjusted_asteroid(59747.963989, 0.000000, 481461.881338);
set_asteroid_speed(asteroid[56], 1.000000);
asteroid[57] = add_adjusted_asteroid(36992.452123, 0.000000, 482862.802873);
set_asteroid_speed(asteroid[57], 1.000000);
asteroid[58] = add_adjusted_asteroid(31148.890261, 0.000000, 481757.999852);
set_asteroid_speed(asteroid[58], 1.000000);
asteroid[59] = add_adjusted_asteroid(-14505.240982, 0.000000, 474555.220264);
set_asteroid_speed(asteroid[59], 1.000000);
asteroid[60] = add_adjusted_asteroid(-13186.996410, 0.000000, 493712.721138);
set_asteroid_speed(asteroid[60], 1.000000);
asteroid[61] = add_adjusted_asteroid(-6059.287682, 0.000000, 513789.770529);
set_asteroid_speed(asteroid[61], 1.000000);
asteroid[62] = add_adjusted_asteroid(27541.442174, 0.000000, 534384.960176);
set_asteroid_speed(asteroid[62], 1.000000);
asteroid[63] = add_adjusted_asteroid(33244.653257, 0.000000, 538831.421824);
set_asteroid_speed(asteroid[63], 1.000000);
asteroid[64] = add_adjusted_asteroid(51038.535000, 0.000000, 546938.391837);
set_asteroid_speed(asteroid[64], 1.000000);
asteroid[65] = add_adjusted_asteroid(75141.080006, 0.000000, 522923.779091);
set_asteroid_speed(asteroid[65], 1.000000);
asteroid[66] = add_adjusted_asteroid(72306.751777, 0.000000, 423914.753827);
set_asteroid_speed(asteroid[66], 1.000000);
asteroid[67] = add_adjusted_asteroid(46338.014344, 0.000000, 419331.445820);
set_asteroid_speed(asteroid[67], 1.000000);
asteroid[68] = add_adjusted_asteroid(51331.757988, 0.000000, 398387.604409);
set_asteroid_speed(asteroid[68], 1.000000);
asteroid[69] = add_adjusted_asteroid(62497.795939, 0.000000, 368926.375371);
set_asteroid_speed(asteroid[69], 1.000000);
asteroid[70] = add_adjusted_asteroid(97958.780963, 0.000000, 307248.384371);
set_asteroid_speed(asteroid[70], 1.000000);
asteroid[71] = add_adjusted_asteroid(97863.786947, 0.000000, 306341.826904);
set_asteroid_speed(asteroid[71], 1.000000);
asteroid[72] = add_adjusted_asteroid(78959.575897, 0.000000, 258445.348670);
set_asteroid_speed(asteroid[72], 1.000000);
asteroid[73] = add_adjusted_asteroid(112199.972644, 0.000000, 218737.925219);
set_asteroid_speed(asteroid[73], 1.000000);
asteroid[74] = add_adjusted_asteroid(167525.772301, 0.000000, 199442.584541);
set_asteroid_speed(asteroid[74], 1.000000);
asteroid[75] = add_adjusted_asteroid(214718.744294, 0.000000, 140894.938015);
set_asteroid_speed(asteroid[75], 1.000000);
asteroid[76] = add_adjusted_asteroid(186330.074118, 0.000000, 144907.038270);
set_asteroid_speed(asteroid[76], 1.000000);
asteroid[77] = add_adjusted_asteroid(108449.779206, 0.000000, 74727.805313);
set_asteroid_speed(asteroid[77], 1.000000);
asteroid[78] = add_adjusted_asteroid(108629.427631, 0.000000, 67946.547141);
set_asteroid_speed(asteroid[78], 1.000000);
asteroid[79] = add_adjusted_asteroid(130672.103582, 0.000000, 42981.193833);
set_asteroid_speed(asteroid[79], 1.000000);
asteroid[80] = add_adjusted_asteroid(131593.769561, 0.000000, 43087.102295);
set_asteroid_speed(asteroid[80], 1.000000);
asteroid[81] = add_adjusted_asteroid(136536.322112, 0.000000, 61020.297518);
set_asteroid_speed(asteroid[81], 1.000000);
asteroid[82] = add_adjusted_asteroid(289803.678891, 0.000000, 110126.973731);
set_asteroid_speed(asteroid[82], 1.000000);
asteroid[83] = add_adjusted_asteroid(304583.743073, 0.000000, 91324.098598);
set_asteroid_speed(asteroid[83], 1.000000);
asteroid[84] = add_adjusted_asteroid(281316.162016, 0.000000, 32757.657560);
set_asteroid_speed(asteroid[84], 1.000000);
asteroid[85] = add_adjusted_asteroid(276972.223627, 0.000000, 42019.112524);
set_asteroid_speed(asteroid[85], 1.000000);
asteroid[86] = add_adjusted_asteroid(254969.260633, 0.000000, 95709.500804);
set_asteroid_speed(asteroid[86], 1.000000);
asteroid[87] = add_adjusted_asteroid(318200.577332, 0.000000, 91120.851236);
set_asteroid_speed(asteroid[87], 1.000000);
asteroid[88] = add_adjusted_asteroid(366071.560685, 0.000000, 59575.728419);
set_asteroid_speed(asteroid[88], 1.000000);
asteroid[89] = add_adjusted_asteroid(432997.011186, 0.000000, 60252.740560);
set_asteroid_speed(asteroid[89], 1.000000);
asteroid[90] = add_adjusted_asteroid(528794.356736, 0.000000, 138624.684613);
set_asteroid_speed(asteroid[90], 1.000000);
asteroid[91] = add_adjusted_asteroid(565368.191263, 0.000000, 190340.641606);
set_asteroid_speed(asteroid[91], 1.000000);
asteroid[92] = add_adjusted_asteroid(580068.500363, 0.000000, 284165.759275);
set_asteroid_speed(asteroid[92], 1.000000);
asteroid[93] = add_adjusted_asteroid(574796.130826, 0.000000, 328710.499974);
set_asteroid_speed(asteroid[93], 1.000000);
asteroid[94] = add_adjusted_asteroid(578117.141589, 0.000000, 440400.220129);
set_asteroid_speed(asteroid[94], 1.000000);
asteroid[95] = add_adjusted_asteroid(572945.733766, 0.000000, 474310.202738);
set_asteroid_speed(asteroid[95], 1.000000);
asteroid[96] = add_adjusted_asteroid(536420.804581, 0.000000, 506024.555587);
set_asteroid_speed(asteroid[96], 1.000000);
asteroid[97] = add_adjusted_asteroid(438218.493503, 0.000000, 520924.395895);
set_asteroid_speed(asteroid[97], 1.000000);
asteroid[98] = add_adjusted_asteroid(370724.298877, 0.000000, 530489.062395);
set_asteroid_speed(asteroid[98], 1.000000);
asteroid[99] = add_adjusted_asteroid(360314.903882, 0.000000, 538619.810746);
set_asteroid_speed(asteroid[99], 1.000000);
asteroid[100] = add_adjusted_asteroid(323949.306645, 0.000000, 548208.835473);
set_asteroid_speed(asteroid[100], 1.000000);
nebula = {};
nebula[1] = add_adjusted_nebula("", 20403.669039, 0.000000, 412077.824963, 2907.000000);
nebula[2] = add_adjusted_nebula("", 50496.154302, 0.000000, 419815.894039, 4058.000000);
nebula[3] = add_adjusted_nebula("", 63392.932743, 0.000000, 443030.100150, 3129.000000);
nebula[4] = add_adjusted_nebula("", 53075.509543, 0.000000, 429273.532903, 4827.000000);
nebula[5] = add_adjusted_nebula("", 56309.864696, 0.000000, 444866.589664, 2468.000000);
nebula[6] = add_adjusted_nebula("", 468652.801434, 0.000000, 527232.245229, 1560.000000);
nebula[7] = add_adjusted_nebula("", 483581.571134, 0.000000, 525687.889627, 3778.000000);
nebula[8] = add_adjusted_nebula("", 490788.563942, 0.000000, 521569.606905, 1889.000000);
nebula[9] = add_adjusted_nebula("", 484611.141535, 0.000000, 516936.538982, 2729.000000);
nebula[10] = add_adjusted_nebula("", 473285.868239, 0.000000, 521569.606905, 4371.000000);
nebula[11] = add_adjusted_nebula("", 84108.265529, 0.000000, 530835.742751, 922.000000);
nebula[12] = add_adjusted_nebula("", 98007.464827, 0.000000, 528261.816748, 693.000000);
nebula[13] = add_adjusted_nebula("", 111391.880043, 0.000000, 539587.094514, 3655.000000);
nebula[14] = add_adjusted_nebula("", 120143.228453, 0.000000, 548338.445159, 993.000000);
nebula[15] = add_adjusted_nebula("", 413942.312269, 0.000000, 100087.880017, 1229.000000);
nebula[16] = add_adjusted_nebula("", 447894.471720, 0.000000, 115821.817571, 4192.000000);
nebula[17] = add_adjusted_nebula("", 452034.978779, 0.000000, 128243.346572, 2962.000000);
nebula[18] = add_adjusted_nebula("", 447894.490719, 0.000000, 122860.670519, 4337.000000);
nebula[19] = add_adjusted_nebula("", 420939.788791, 0.000000, 115904.615128, 4200.000000);
nebula[20] = add_adjusted_nebula("", 418331.268997, 0.000000, 120252.149608, 4703.000000);
nebula[21] = add_adjusted_nebula("", 430504.360623, 0.000000, 120252.149608, 522.000000);
nebula[22] = add_adjusted_nebula("", 436590.905318, 0.000000, 121121.656951, 4903.000000);
starbase = {};
starbase[1] = add_starbase(adjx + 220156.671582, 0.000000, adjz + 29097.111909, 0);
starbase[2] = add_starbase(adjx + 29231.803505, 0.000000, adjz + 520943.257455, 1);
ship = {};
ship[1] = add_adjusted_ship("WARPO REDSHIFT", 234316.030803, -7084.589074, 37965.765181, 20, 4, 1);
ship[2] = add_adjusted_ship("REVENGE QUARKION", 259153.987152, -0.199720, 9110.527181, 13, 1, 1);
ship[3] = add_adjusted_ship("COSMO FIRE", 216651.427789, -1877.001427, 30463.192881, 14, 1, 1);
ship[4] = add_adjusted_ship("ATHE CETUS", 36747.250979, 2728.394261, 518186.870344, 17, 2, 1);
ship[5] = add_adjusted_ship("SCIMITAR ALTA", 246972.935367, -42.093823, 102428.583639, 21, 3, 1);
ship[6] = add_adjusted_ship("LONTANA JAGUAR", 207090.858956, -6.749023, 89164.699594, 5, 2, 1);
ship[7] = add_adjusted_ship("WOLF REDSHIFT", 299949.465764, 0.043092, 87752.673783, 21, 1, 1);
ship[8] = add_adjusted_ship("DRACO POLARIS", 55308.855084, -0.148435, 477292.296918, 9, 1, 1);
ship[9] = add_adjusted_ship("EARLY VOLCAN", 54383.871425, 423.302192, 489010.608316, 14, 1, 1);
ship[10] = add_adjusted_ship("MOONGRAZER STARRY", 216708.256612, -2590.455333, 80411.180003, 0, 4, 1);
ship[11] = add_adjusted_ship("ZISTO LION", 170287.826887, 454.011689, 540788.188305, 5, 1, 1);
ship[12] = add_adjusted_ship("VOIDSKY CHARMANT", 229151.638235, -700.891378, 43541.474631, 17, 4, 1);
ship[13] = add_adjusted_ship("EKITH STARLIGHT", 172009.556186, -9.350276, 493708.079382, 13, 1, 1);
ship[14] = add_adjusted_ship("VOLARE TARANTULA", 141129.727247, -0.379267, 566280.054486, 2, 1, 1);
ship[15] = add_adjusted_ship("WANDERING SPEM", 92730.608878, -0.122077, 577969.263330, 21, 1, 1);
ship[16] = add_adjusted_ship("WOLF FARTHER", 62618.672112, 31.317226, 538591.735229, 21, 1, 1);
ship[17] = add_adjusted_ship("PANZER GALAXIE", 227353.202136, -5346.451044, 42176.595606, 22, 4, 1);
ship[18] = add_adjusted_ship("STARLIGHT UDI", 219589.729154, -498.450826, 33080.186423, 12, 1, 1);
ship[19] = add_adjusted_ship("CARINA ARROW", 40070.850604, -976.452978, 520685.656396, 9, 1, 1);
ship[20] = add_adjusted_ship("STARMAN ESHA", 215070.867500, -259.536985, 43051.277197, 11, 1, 1);
ship[21] = add_adjusted_ship("CETUS WALRUS", 239156.265913, -1536.948791, 74044.995097, 14, 4, 1);
ship[22] = add_adjusted_ship("VOLARE MONTAGNE", 497247.719624, 1.106800, 408507.108261, 6, 4, 1);
ship[23] = add_adjusted_ship("STARFALL BERYL", 235912.206732, -6737.201655, 37086.684835, 11, 1, 1);
ship[24] = add_adjusted_ship("MOONGRAZER SILENCIO", 237723.253504, -9.920802, 37657.503081, 5, 1, 1);
ship[25] = add_adjusted_ship("STELLAR GRYPHON", 66938.097608, 107.495300, 92158.689204, 14, 4, 1);
ship[26] = add_adjusted_ship("HIGH PHOTON", 248692.683966, 47.443824, 41931.411502, 15, 1, 1);
ship[27] = add_adjusted_ship("CETUS PHOENIX", 42065.608057, 0.628594, 164283.501405, 2, 3, 1);
ship[28] = add_adjusted_ship("PLANET STARGAZER", 35481.666496, 1.196026, 220544.967504, 15, 2, 1);
derelict = {};
derelict[1] = add_derelict("GOLDEN VALIANT", adjx + 169283.026223, 5438.820722, adjz + 551148.972539, 3, 4);
ship[29] = add_adjusted_ship("ALTO CERBERUS", 485680.491472, -268.774447, 39926.780964, 5, 0, 1);

-- set player 1 location
if (player_ids[1]) then
	move_object(player_ids[1], adjx + 198288.048317, -96759.000902, adjz + 40475.262355);
end

demolisher_id = add_demolisher_ship(demox, demoy, demoz);
demolisher_speed = 0;

function move_demolisher()
    zetax, zetay, zetaz = get_object_location(planet[2]);
    myx, myy, myz = get_object_location(demolisher_id);
    angle = align_object_towards(demolisher_id, zetax, zetay, zetaz, 0.1 * math.pi / 180.0);
    desired_demolisher_speed = 100;

    dx = zetax - myx;
    dy = zetay - myy;
    dz = zetaz - myz;

    dist = math.sqrt(dx * dx + dy * dy + dz * dz);
    if dist > 100000 then
       desired_demolisher_speed = 400;
    elseif dist > 60000 then
       desired_demolisher_speed = 200;
    elseif dist > 30000 then
       desired_demolisher_speed = 100;
    elseif dist > 25000 then
       desired_demolisher_speed = 50;
    else
       desired_demolisher_speed = 0;
    end
    if angle > 3.0 * math.pi / 180.0 then
	desired_demolisher_speed = 0;
    end
    if demolisher_speed < desired_demolisher_speed then
       demolisher_speed = demolisher_speed + 1
    elseif demolisher_speed > desired_demolisher_speed then
       demolisher_speed = demolisher_speed - 1
    end
    dx = demolisher_speed * dx / dist;
    dy = demolisher_speed * dy / dist;
    dz = demolisher_speed * dz / dist;
    move_object(demolisher_id, myx + dx, myy + dy, myz + dz);
    register_timer_callback("move_demolisher", 1, 0);
end

function long_warning_message()
	text_to_speech(player_ids[1], "Message received on emergency channel from unknown source.  Message follows");
	text_to_speech(player_ids[1], "Attention citizens! This announcement is in accord with space infrastructure regulation 7 2 3 dash C subparagraph 7.");
	text_to_speech(player_ids[1],
		string.format("The planet you call Zeta Scorpio is scheduled to be demolished in %d of your days.",
				days_left));
	text_to_speech(player_ids[1], "You are advised to evacuate to a safe distance immediately. We assume you have made appropriate plans.");
	text_to_speech(player_ids[1], "The demolition notice has been available for hundreds of your years.");
	text_to_speech(player_ids[1], "The department of space infrastructure thanks you for your cooperation.");
end

function short_warning_message()
	text_to_speech(player_ids[1],
		string.format("Attention Citizens! The planet you call Zeta Scorpio is scheduled to be demolished in %d of your days.",
				days_left));
end

function warning_message(cookie)
	if (days_left % 5) == 0 then
		long_warning_message();
	else
		short_warning_message();
	end
	days_left = days_left - 1;
	register_timer_callback("warning_message", 600, 0);
end

function starbase_listener1(from, channel, message)
	comms_channel_transmit("SB-01", sb01channel1, ": GREETINGS.  WE HAVE GOOD NEWS.");
	comms_channel_transmit("SB-01", sb01channel1, ": WE HAVE BROKEN THE CODE USED BY THE DEMOLISHER");
	comms_channel_transmit("SB-01", sb01channel1, ": AND WE THINK WE HAVE FIGURED OUT A WAY TO GAIN ENTRY.");
	comms_channel_transmit("SB-01", sb01channel1, ": THE LANDING BAY DOOR COMPUTER ON THE DEMOLISHER HAS");
	comms_channel_transmit("SB-01", sb01channel1, ": BEEN COMPROMISED.");
	comms_channel_transmit("SB-01", sb01channel1,
		string.format(": YOU CAN TRANSMIT " .. opencmd .. " ON CHANNEL %d TO OPEN THE DOOR OR ", door_channel))
	comms_channel_transmit("SB-01", sb01channel1, ": OR " .. closecmd .. " TO CLOSE THE DOOR AND THE DOOR")
	comms_channel_transmit("SB-01", sb01channel1, ": WILL RESPOND WITH A CHALLENGE.  WE CAN WITH SOME EFFORT");
	comms_channel_transmit("SB-01", sb01channel1, ": DETERMINE THE CORRECT RESPONSE TO THE CHALLENGE.");
	comms_channel_transmit("SB-01", sb01channel1, ": WHEN YOU RECEIVE THE CHALLENGE, FORWARD IT TO SB-01 ON");
	comms_channel_transmit("SB-01", sb01channel1,
		string.format(": CHANNEL %d AND WE WILL DECODE IT ON THE SUPERCOMPUTERS", sb01channel2));
	comms_channel_transmit("SB-01", sb01channel1, ": AND TRANSMIT THE CORRECT RESPONSE BACK TO YOU.");
	comms_channel_transmit("SB-01", sb01channel1, ": YOU CAN THEN TRANSMIT THIS ANSWER TO THE LANDING BAY DOOR");
	comms_channel_transmit("SB-01", sb01channel1, ": COMPUTER AND GAIN ENTRY.");
	comms_channel_unlisten("SB-01", sb01channel1);
	comms_channel_listen("SB-01", sb01channel2, "starbase_listener2");
end

function starbase_listener2(from, channel, message)
   if from == "SB-01" then
	return;
   end
   if string.sub(message, 1, 26) == "TX/RX INITIATED ON CHANNEL" then
	return;
   end
   if message == challenge then
	comms_channel_transmit("SB-01", channel,
		string.format(": THE ANSWER TO THE CHALLENGE IS: %s", challenge_answer));
   else
	comms_channel_transmit("SB-01", channel, "THE CHALLENGE DOES NOT SEEM TO DECODE CORRECTLY.");
	comms_channel_transmit("SB-01", channel, "DOUBLE CHECK THE ACCURACY OF YOUR TRANSCRIPTION.");
   end
end

function hail_player()
	comms_channel_transmit("SB-01", 0, ": *** HAILING ON CHANNEL " .. sb01channel1 .. " ***");
	comms_channel_listen("SB-01", sb01channel1, "starbase_listener1");
end

register_timer_callback("door_mover", 1, 0);
register_timer_callback("move_demolisher", 10, 0);
register_timer_callback("warning_message", 10, 0);
register_timer_callback("hail_player", 300, 0);
comms_channel_listen("DEMOLISHER", door_channel, "door_comms")

