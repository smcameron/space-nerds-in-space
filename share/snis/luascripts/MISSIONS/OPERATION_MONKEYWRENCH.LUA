-- Operation Monkeywrench

secs = 10; -- snis server simulation runs at 10Hz
player_ids = get_player_ship_ids();
reset_player_ship(player_ids[1]);
starbase0 = -1;
mission_stage = 0;
summon_to_base_time = 15 * secs;
player_orders_time = 10 * secs;
bruiser_msg_time = 90 * secs;
encrypted_message_time = 60 * secs;
second_pierce_msg_time = 90 * secs;
players_took_passengers = 0;
commendations_msg_time = 30 * secs;
epilogue_msg_time = 30 * secs;
govt_revenge_msg_time = 60 * secs;
govt_attack_time = 30 * secs;
num_attackers = 5;

function setup_universe()
	clear_all();
	nebula = {};
	nebula[1] = add_nebula("", -148501.890625, 45787.949219, -7898.945312, 3297.000000);
	nebula[2] = add_nebula("", -145407.371965, 43805.332549, -9363.966490, 3684.000000);
	nebula[3] = add_nebula("", -142195.531235, 41794.207748, -11640.571446, 1807.000000);
	nebula[4] = add_nebula("", -140360.826082, 41049.983404, -12524.848109, 995.000000);
	nebula[5] = add_nebula("", -139394.444445, 40763.132013, -13164.737827, 1128.000000);
	nebula[6] = add_nebula("", -137174.583165, 40167.701960, -15163.076630, 2765.000000);
	nebula[7] = add_nebula("", -134624.523062, 38990.504086, -16929.577497, 3052.000000);
	nebula[8] = add_nebula("", -132021.298558, 37367.319484, -18929.994265, 3669.000000);
	nebula[9] = add_nebula("", -129350.882660, 34959.773894, -21471.106181, 1470.000000);
	nebula[10] = add_nebula("", -128572.419937, 33987.710431, -22720.419623, 842.000000);
	nebula[11] = add_nebula("", -37325.261719, -34215.843750, -294072.875000, 1688.000000);
	nebula[12] = add_nebula("", -42310.610148, -39166.388154, -296808.929320, 4165.000000);
	nebula[13] = add_nebula("", -45430.499014, -42732.597709, -298399.038471, 4091.000000);
	nebula[14] = add_nebula("", -37283.391422, -31162.140912, -290229.258196, 2173.000000);
	nebula[15] = add_nebula("", -36891.015000, -29899.152194, -287981.931337, 1054.000000);
	nebula[16] = add_nebula("", -36835.872589, -29419.082679, -286813.080828, 3047.000000);
	nebula[17] = add_nebula("", -37189.399086, -28557.056014, -283277.378614, 2812.000000);
	nebula[18] = add_nebula("", -34433.738035, -35855.761024, -294652.821037, 3657.000000);
	nebula[19] = add_nebula("", -32990.156146, -33570.064032, -294291.689520, 3658.000000);
	nebula[20] = add_nebula("", -33655.292797, -36114.199252, -295554.820673, 644.000000);
	nebula[21] = add_nebula("", -10577.498047, -51824.925781, 172068.609375, 3158.000000);
	nebula[22] = add_nebula("", -13167.324372, -53526.403668, 169887.132548, 441.000000);
	nebula[23] = add_nebula("", -10947.646144, -51904.051857, 172438.450024, 3981.000000);
	nebula[24] = add_nebula("", -21505.960471, -54675.647977, 182129.304618, 1877.000000);
	nebula[25] = add_nebula("", -22995.941278, -55212.788552, 183730.786046, 3298.000000);
	nebula[26] = add_nebula("", -25266.810144, -56559.993375, 186678.800824, 4914.000000);
	nebula[27] = add_nebula("", -28204.598184, -59106.930397, 191112.169718, 599.000000);
	nebula[28] = add_nebula("", -28630.151782, -59421.192763, 191598.807933, 4780.000000);
	nebula[29] = add_nebula("", -10459.650302, -55384.526265, 167572.271670, 961.000000);
	nebula[30] = add_nebula("", -10324.020516, -56033.377092, 166628.625834, 1095.000000);
	nebula[31] = add_nebula("", 141147.828125, -99913.296875, 105326.023438, 875.000000);
	nebula[32] = add_nebula("", 141689.650252, -100797.523930, 105161.489799, 609.000000);
	nebula[33] = add_nebula("", 142129.582964, -101368.030980, 105038.813125, 683.000000);
	nebula[34] = add_nebula("", 142690.339303, -101940.500017, 104866.837128, 4062.000000);
	nebula[35] = add_nebula("", 139121.730015, -105044.569828, 120031.667990, 3044.000000);
	nebula[36] = add_nebula("", 138445.139391, -106081.568914, 123468.207684, 2192.000000);
	nebula[37] = add_nebula("", 138196.481600, -106364.994902, 126071.444846, 2444.000000);
	nebula[38] = add_nebula("", 137887.520377, -107131.078129, 128885.512289, 4286.000000);
	nebula[39] = add_nebula("", 137136.397218, -107912.736366, 133913.168510, 319.000000);
	nebula[40] = add_nebula("", 137075.691503, -107992.091219, 134282.699916, 778.000000);
	asteroid = {};
	asteroid[1] = add_asteroid(206259.809027, -155486.656268, 32153.083062);
	set_asteroid_speed(asteroid[1], 1.000000);
	asteroid[2] = add_asteroid(171841.698204, -120795.434644, 18748.416412);
	set_asteroid_speed(asteroid[2], 1.000000);
	asteroid[3] = add_asteroid(174717.381127, -147992.087209, 24777.486771);
	set_asteroid_speed(asteroid[3], 1.000000);
	asteroid[4] = add_asteroid(180537.114072, -156784.228843, 8050.678301);
	set_asteroid_speed(asteroid[4], 1.000000);
	asteroid[5] = add_asteroid(202556.010534, -143660.781120, 10822.398900);
	set_asteroid_speed(asteroid[5], 1.000000);
	asteroid[6] = add_asteroid(58826.535543, -91174.670652, 237932.522116);
	set_asteroid_speed(asteroid[6], 1.000000);
	asteroid[7] = add_asteroid(65855.574758, -82647.593943, 254007.418741);
	set_asteroid_speed(asteroid[7], 1.000000);
	asteroid[8] = add_asteroid(68041.908804, -97299.724978, 239943.079390);
	set_asteroid_speed(asteroid[8], 1.000000);
	asteroid[9] = add_asteroid(75804.952403, -103815.233363, 247758.847952);
	set_asteroid_speed(asteroid[9], 1.000000);
	asteroid[10] = add_asteroid(49113.282490, -98170.224506, 232859.579389);
	set_asteroid_speed(asteroid[10], 1.000000);
	asteroid[11] = add_asteroid(-205649.537167, -36269.507351, 147155.126385);
	set_asteroid_speed(asteroid[11], 1.000000);
	asteroid[12] = add_asteroid(-193782.682432, -28724.016031, 155905.398360);
	set_asteroid_speed(asteroid[12], 1.000000);
	asteroid[13] = add_asteroid(-216116.296857, -36546.117552, 140948.720280);
	set_asteroid_speed(asteroid[13], 1.000000);
	asteroid[14] = add_asteroid(-206892.392604, -27642.759743, 150289.021552);
	set_asteroid_speed(asteroid[14], 1.000000);
	asteroid[15] = add_asteroid(-209882.569871, -22322.015665, 143371.359926);
	set_asteroid_speed(asteroid[15], 1.000000);
	asteroid[16] = add_asteroid(-264373.040601, 47970.212711, 90307.280915);
	set_asteroid_speed(asteroid[16], 1.000000);
	asteroid[17] = add_asteroid(-277790.387796, 36838.250779, 72929.102987);
	set_asteroid_speed(asteroid[17], 1.000000);
	asteroid[18] = add_asteroid(-271123.598295, 43010.599766, 90091.848652);
	set_asteroid_speed(asteroid[18], 1.000000);
	asteroid[19] = add_asteroid(-249826.737016, 44367.043867, 83583.141703);
	set_asteroid_speed(asteroid[19], 1.000000);
	asteroid[20] = add_asteroid(-268194.592424, 57464.123640, 98024.401141);
	set_asteroid_speed(asteroid[20], 1.000000);
	asteroid[21] = add_asteroid(132139.428319, -64784.368257, -1513.450700);
	set_asteroid_speed(asteroid[21], 1.000000);
	asteroid[22] = add_asteroid(144061.570534, -81591.011513, 14103.057057);
	set_asteroid_speed(asteroid[22], 1.000000);
	asteroid[23] = add_asteroid(131119.550292, -50304.907524, 11746.761809);
	set_asteroid_speed(asteroid[23], 1.000000);
	asteroid[24] = add_asteroid(120464.729325, -74535.622641, 5800.386426);
	set_asteroid_speed(asteroid[24], 1.000000);
	asteroid[25] = add_asteroid(120819.002072, -77534.945986, 10057.495710);
	set_asteroid_speed(asteroid[25], 1.000000);
	asteroid[26] = add_asteroid(-28003.585797, -63118.357166, 125504.788522);
	set_asteroid_speed(asteroid[26], 1.000000);
	asteroid[27] = add_asteroid(-26874.038902, -61897.652348, 120627.813644);
	set_asteroid_speed(asteroid[27], 1.000000);
	asteroid[28] = add_asteroid(-12740.690864, -75644.227826, 136870.108918);
	set_asteroid_speed(asteroid[28], 1.000000);
	asteroid[29] = add_asteroid(-36232.931196, -50251.177332, 136243.445609);
	set_asteroid_speed(asteroid[29], 1.000000);
	asteroid[30] = add_asteroid(-31561.575419, -56251.377272, 131920.786103);
	set_asteroid_speed(asteroid[30], 1.000000);
	asteroid[31] = add_asteroid(121059.097891, -48579.197001, 112799.651002);
	set_asteroid_speed(asteroid[31], 1.000000);
	asteroid[32] = add_asteroid(119121.873374, -50954.765669, 110438.347206);
	set_asteroid_speed(asteroid[32], 1.000000);
	asteroid[33] = add_asteroid(123424.716701, -55390.138613, 101176.282964);
	set_asteroid_speed(asteroid[33], 1.000000);
	asteroid[34] = add_asteroid(131475.965911, -55522.559259, 112401.502008);
	set_asteroid_speed(asteroid[34], 1.000000);
	asteroid[35] = add_asteroid(122213.169371, -49052.291116, 107319.867299);
	set_asteroid_speed(asteroid[35], 1.000000);
	asteroid[36] = add_asteroid(-194789.688679, 20887.203877, 199246.595499);
	set_asteroid_speed(asteroid[36], 1.000000);
	asteroid[37] = add_asteroid(-197345.256986, 43660.510210, 200903.591792);
	set_asteroid_speed(asteroid[37], 1.000000);
	asteroid[38] = add_asteroid(-207694.635889, 39995.570867, 219411.306564);
	set_asteroid_speed(asteroid[38], 1.000000);
	asteroid[39] = add_asteroid(-198989.238952, 42753.733314, 219877.526709);
	set_asteroid_speed(asteroid[39], 1.000000);
	asteroid[40] = add_asteroid(-209502.903113, 31245.949026, 204155.085031);
	set_asteroid_speed(asteroid[40], 1.000000);
	asteroid[41] = add_asteroid(-103207.918993, 37764.583600, 172974.449890);
	set_asteroid_speed(asteroid[41], 1.000000);
	asteroid[42] = add_asteroid(-103575.985219, 35350.767098, 188650.580922);
	set_asteroid_speed(asteroid[42], 1.000000);
	asteroid[43] = add_asteroid(-119233.559144, 54079.109323, 183523.446565);
	set_asteroid_speed(asteroid[43], 1.000000);
	asteroid[44] = add_asteroid(-116342.283640, 31893.029208, 170800.840344);
	set_asteroid_speed(asteroid[44], 1.000000);
	asteroid[45] = add_asteroid(-102981.695217, 35274.942387, 168137.662243);
	set_asteroid_speed(asteroid[45], 1.000000);
	asteroid[46] = add_asteroid(191721.234324, -139770.140182, 210276.150123);
	set_asteroid_speed(asteroid[46], 1.000000);
	asteroid[47] = add_asteroid(192941.064616, -140471.383214, 200468.686767);
	set_asteroid_speed(asteroid[47], 1.000000);
	asteroid[48] = add_asteroid(183724.123022, -158943.375380, 214423.633244);
	set_asteroid_speed(asteroid[48], 1.000000);
	asteroid[49] = add_asteroid(200896.711090, -140793.259669, 190484.256181);
	set_asteroid_speed(asteroid[49], 1.000000);
	asteroid[50] = add_asteroid(194773.110059, -142492.404370, 200827.027786);
	set_asteroid_speed(asteroid[50], 1.000000);
	planet = {};
	planet[1] = add_planet("QUAREAUX", -92757.500000, -137770.453125, -168390.406250, 5441.000000, 2);
	planet[2] = add_planet("CORBU", -264156.062500, -15846.296875, 283464.906250, 3338.000000, 2);
	planet[3] = add_planet("XIKOPH", -143652.953125, -126535.476562, 78987.632812, 8221.000000, 2);
	planet[4] = add_planet("GANZON", 214085.453125, -77067.062500, -209186.859375, 6314.000000, 2);
	planet[5] = add_planet("IKASTER", -33267.671875, -104303.960938, 300836.062500, 8572.000000, 0);
	planet[6] = add_planet("ZANTAR", -88822.585938, 93629.898438, 243444.234375, 8498.000000, 2);
	zantar = planet[6];
	starbase = {};
	starbase[1] = add_starbase(-98490.985840, -140888.393555, -175861.518555, 0);
	set_plot_armor(starbase[1], 1);
	starbase[2] = add_starbase(-266054.608643, -11403.408691, 281833.910767, 1);
	set_plot_armor(starbase[2], 1);
	starbase[3] = add_starbase(-98394.914062, 93632.536533, 236812.050293, 2);
	starbase[4] = add_starbase(217994.796631, -78889.231445, -201625.295410, 3);
	starbase[5] = add_starbase(-269121.312500, 43167.648438, 226146.406250, 4);
	starbase[6] = add_starbase(-235036.890625, -11954.746094, -95495.648438, 5);
	starbase[7] = add_starbase(87335.796875, -95813.515625, -130649.062500, 6);
	starbase[8] = add_starbase(-282241.906250, 16026.218750, 180739.859375, 7);
	starbase[9] = add_starbase(-93545.718750, 85.667969, 195290.031250, 8);
	ship = {};
	ship[1] = add_ship("AURORE", -88822, 93629 + 10000, 243444, 4, 0, 1); -- near Zantar
	aurore = ship[1];
	set_plot_armor(aurore, 1);
	ai_push_catatonic(aurore); -- make the ship just sit there.
	ship[2] = add_ship("THUOSTSOPH AURIGA", 165528.053567, -61953.795642, -169564.787459, 18, 1, 1);
	ship[3] = add_ship("BEAR SKY", -97114.281201, -139103.877506, -173168.551029, 14, 2, 1);
	ship[4] = add_ship("SHOKVEESNOVA PORTA", -83549.777134, 28784.599577, 264957.069125, 5, 3, 1);
	ship[5] = add_ship("VAKIOIPHA SKY", -263405.691191, -12255.723745, 279314.211050, 6, 4, 1);
	ship[6] = add_ship("REDSHIFT TOU", 91319.288351, -91133.061066, -94748.647649, 17, 0, 1);
	ship[7] = add_ship("TUPTAR STELLAR", -150636.431421, 32616.281878, 103734.854892, 17, 1, 1);
	ship[8] = add_ship("FLAME STARMAN", 178486.612211, -75058.076374, -178798.922939, 9, 2, 1);
	ship[9] = add_ship("IRAPIPHA VELOCE", -77993.807925, -42920.018416, 12.896179, 0, 3, 1);
	ship[10] = add_ship("VELOCITA DOVE", -144281.383832, 54748.462543, 230074.580474, 7, 4, 1);
	ship[11] = add_ship("RAPTOR JADE", -94875.463220, -139911.793903, -175100.946766, 4, 0, 1);
	ship[12] = add_ship("TAURUS CYGNUS", -261391.761632, -19046.072269, 284720.845643, 18, 1, 1);
	ship[13] = add_ship("CAPRICORN RANGER", 221809.535891, -159176.761048, 128538.438408, 21, 2, 1);
	ship[14] = add_ship("BUI STARLIGHT", -279302.405725, 49234.519936, 21439.336387, 13, 3, 1);
	ship[15] = add_ship("CELESTIAL NAOS", -84180.257580, -134692.271471, -150228.086972, 19, 4, 1);
	ship[16] = add_ship("GALAXIE VIOVLALNOVA", -219641.155519, -23091.074868, 274509.776639, 4, 0, 1);
	ship[17] = add_ship("STARLINK IKKAIGNOVA", 257209.442707, -142627.366577, -25894.898734, 15, 1, 1);
	ship[18] = add_ship("VOID VEA", 208402.090611, -71478.824649, -206990.030540, 10, 2, 1);
	ship[19] = add_ship("STARWARD LUCE STELL", 94021.143572, -93600.402527, -196400.030406, 4, 3, 1);
	ship[20] = add_ship("ASTRO CARINA", -200438.539417, 12602.715464, -290481.783166, 6, 4, 1);
	ship[21] = add_ship("REVENGE VOIDNESS", -265922.704280, -11207.184221, 280814.746139, 12, 0, 1);
	ship[22] = add_ship("LEOPARD RAPIDO", -93149.989438, -131359.866704, -165298.141094, 14, 1, 1);
	ship[23] = add_ship("BRUISER", -233911.436579, -12260.366119, -95522.516186, 4, 2, 1);
	bruiser = ship[23];
	set_plot_armor(bruiser, 1);
	ship[24] = add_ship("STARGRAZER CASSIOPE", -118361.014949, -7464.510257, -45827.064948, 21, 3, 1);
	ship[25] = add_ship("NIGHT DOVE", 132873.198557, -70886.344041, 301130.180493, 3, 4, 1);
	ship[26] = add_ship("EPI STARBOUND", 125216.239341, -61223.952591, 300281.227042, 13, 0, 1);
	ship[27] = add_ship("TIEMPO BULLET", -71720.087580, -140045.707331, -140689.192935, 6, 1, 1);
	ship[28] = add_ship("CETUS HORIZON", -86246.085043, -133254.454765, -167718.352132, 11, 2, 1);
	ship[29] = add_ship("STARWAY ARROW", 48549.954016, -113349.869261, -73828.247725, 0, 3, 1);
	ship[30] = add_ship("LOAMPHAK WANDERING", -52350.441787, -37813.742777, 227645.513034, 0, 4, 1);
	ship[31] = add_ship("AURIGA BARRACUDA", -280052.042475, 15630.077561, 181314.868396, 1, 0, 1);
	ship[32] = add_ship("ONVUANOVA COSMO", 77608.097244, -60582.850150, 277826.840472, 5, 1, 1);
	ship[33] = add_ship("DISTANT DEMON", -192165.183541, -51134.796766, -202123.771341, 20, 2, 1);
	ship[34] = add_ship("BLOSSOM CENTAURUS", 90837.786899, -97074.023535, -118104.020038, 6, 3, 1);
	ship[35] = add_ship("EUROPA ESTERON", -95916.632776, -140295.953972, -175379.597028, 17, 4, 1);
	ship[36] = add_ship("JIROZ WAYWARD", 211891.690512, -106212.884160, 286371.141909, 0, 0, 1);
	ship[37] = add_ship("WINGER CELESTIAL", 219396.917580, -85072.969049, -178523.086344, 6, 1, 1);
	ship[38] = add_ship("TEMPO STARFIRE", 104393.383071, -64597.775489, 104552.085121, 7, 2, 1);
	ship[39] = add_ship("ZIUPKAP LUMIERE", 164640.693813, -162823.499432, -6630.236554, 18, 3, 1);
	ship[40] = add_ship("BLACKNESS GEM", 241461.410127, -137104.308883, 190495.845003, 8, 4, 1);
	ship[41] = add_ship("ASTERION WING", 72022.990510, -151577.282393, -183774.921205, 9, 0, 1);
	ship[42] = add_ship("NIGHT TRAVELLER", 11684.703478, -57879.767491, 276714.207388, 15, 1, 1);
	ship[43] = add_ship("LUMIERE WUFFER", 194534.796987, -137594.639626, -153296.839509, 0, 2, 1);
	ship[44] = add_ship("GOLDEN CONCEPT", 220725.173968, -83243.092738, -170846.225089, 6, 3, 1);
	ship[45] = add_ship("ZISTO ODIV", -34212.737271, -70227.194837, -20151.821650, 21, 4, 1);
	ship[46] = add_ship("OCTOPUS TIEMPO", -28153.530073, -16778.744141, 20663.638967, 19, 0, 1);
	ship[47] = add_ship("NIGHT PHEETHBOVACH", -96259.537341, -130828.309033, -166806.321256, 20, 1, 1);
	ship[48] = add_ship("ASTRO STELLARON", 63331.025031, -107550.182370, -189506.049298, 21, 2, 1);
	ship[49] = add_ship("DARKSKY BOUNTY", 158003.567281, -100462.298426, -73583.480084, 15, 3, 1);
	ship[50] = add_ship("ALPHA PORTA", -84036.148422, -23451.403543, -60230.826335, 3, 4, 1);
	ship[51] = add_ship("POLICE-15", -89233.769761, -124444.227796, -162186.542557, 20, 0, 1);
	ship[52] = add_ship("POLICE-96", -85912.180081, -113363.280939, -152421.238535, 20, 0, 1);
	ship[53] = add_ship("POLICE-00", -71723.507840, -140700.258526, -134821.000009, 20, 0, 1);
	ship[54] = add_ship("POLICE-72", 90808.130557, -84277.066504, 126409.924611, 20, 0, 1);
	ship[55] = add_ship("POLICE-16", -265539.703100, -21934.766300, 287026.697222, 20, 3, 1);
	ship[56] = add_ship("POLICE-16", -240077.363141, -15663.550648, 273198.961884, 20, 3, 1);
	ship[57] = add_ship("POLICE-01", -260911.692248, -7991.117143, 280090.461705, 20, 3, 1);
	ship[58] = add_ship("POLICE-17", -258358.064243, -14804.559047, 288280.345105, 20, 3, 1);
	ship[59] = add_ship("POLICE-38", -10267.208235, -76534.501894, -17221.215851, 20, 0, 1);
	ship[60] = add_ship("POLICE-57", 202066.095445, -116392.754888, -160257.578553, 20, 0, 1);
	ship[61] = add_ship("POLICE-21", -153548.074774, -111097.965382, 64928.036465, 20, 0, 1);
	ship[62] = add_ship("POLICE-06", -138934.157188, -116997.208156, 47944.430746, 20, 0, 1);
	ship[63] = add_ship("POLICE-30", -138278.796022, -26795.143916, -259722.148790, 20, 4, 1);
	ship[64] = add_ship("POLICE-98", 27560.640509, -49017.347485, 132853.496476, 20, 4, 1);
	ship[65] = add_ship("POLICE-40", 213323.605905, -61760.517725, -207514.297781, 20, 4, 1);
	ship[66] = add_ship("POLICE-84", 200308.562713, -58783.820431, -187277.962837, 20, 4, 1);
	ship[67] = add_ship("POLICE-55", -70457.974587, 74543.039490, 242808.740970, 20, 3, 1);
	ship[68] = add_ship("POLICE-09", -80751.795594, 96165.842911, 232997.103724, 20, 3, 1);
	ship[69] = add_ship("POLICE-10", -92746.517695, 92462.694214, 222952.616560, 20, 3, 1);
	ship[70] = add_ship("POLICE-40", -88410.012431, 117077.448786, 243330.287756, 20, 3, 1);
	ship[71] = add_ship("POLICE-26", 31411.485672, -94789.600038, -114062.668930, 20, 2, 1);
	ship[72] = add_ship("POLICE-75", -156641.923333, 13928.592004, -3493.205612, 20, 2, 1);
	ship[73] = add_ship("POLICE-27", 11794.990370, -113043.778385, -127480.920178, 20, 2, 1);
	ship[74] = add_ship("POLICE-99", 37663.166696, -108562.582027, -145861.943732, 20, 2, 1);
	ship[75] = add_ship("POLICE-93", 21893.237057, -100097.125774, -135496.507365, 20, 0, 1);
	ship[76] = add_ship("POLICE-63", 44708.518939, -102130.166061, -140927.453912, 20, 0, 1);
	ship[77] = add_ship("POLICE-74", 5516.407424, -79831.415920, -119064.254767, 20, 0, 1);
	ship[78] = add_ship("POLICE-16", 56162.257908, -98315.416508, -114993.375929, 20, 0, 1);
	ship[79] = add_ship("POLICE-62", -211980.799802, -33693.861720, -82066.898895, 20, 0, 1);
	ship[80] = add_ship("POLICE-37", -261320.372604, 16181.081994, -132611.653154, 20, 0, 1);
	ship[81] = add_ship("POLICE-90", 36450.462309, -92580.425663, -142721.322370, 20, 0, 1);
	ship[82] = add_ship("POLICE-02", 34578.540194, -108505.447785, -130797.850459, 20, 0, 1);
	ship[83] = add_ship("POLICE-44", 68025.239579, -41066.499477, 110364.542831, 20, 4, 1);
	ship[84] = add_ship("POLICE-32", 48158.930216, -91189.079352, -156295.144870, 20, 4, 1);
	ship[85] = add_ship("POLICE-48", 105405.424756, -127403.059403, -172592.337544, 20, 4, 1);
	ship[86] = add_ship("POLICE-73", -180180.850000, 3372.974149, 44516.986815, 20, 4, 1);
	ship[87] = add_ship("POLICE-56", 132649.998035, -117756.169173, -24637.543385, 20, 1, 1);
	ship[88] = add_ship("POLICE-08", 198557.400401, -272294.049137, -255100.932082, 20, 1, 1);
	ship[89] = add_ship("POLICE-48", 203337.523573, -88964.111997, 281583.673690, 20, 1, 1);
	ship[90] = add_ship("POLICE-38", -192854.183880, -32281.402344, -281934.029587, 20, 1, 1);
	ship[91] = add_ship("POLICE-92", -62492.944808, -45091.640625, 60944.997862, 20, 0, 1);
	ship[92] = add_ship("POLICE-08", -267734.262015, 32348.936538, -87134.167422, 20, 0, 1);
	ship[93] = add_ship("POLICE-54", -65718.990054, -40219.623711, 65805.827113, 20, 0, 1);
	ship[94] = add_ship("POLICE-12", -54865.177625, -46043.088553, 63431.185885, 20, 0, 1);
	spacemonster = {};
	spacemonster[1] = add_spacemonster("M. MYSTERIUM", -208699.104109, -13211.556122, 255647.279999);
	spacemonster[2] = add_spacemonster("M. MYSTERIUM", -218358.005936, 60090.195484, -41677.953003);
	black_hole = {};
	black_hole[1] = add_black_hole("SUSHOZ", -90327.179688, -19429.039062, -68401.835938, 1862.000000);
	black_hole[2] = add_black_hole("TIIVMUTIAN", 53283.218750, 1481.041016, 27660.935547, 835.000000);
end

set_variable("SUPPRESS_STARBASE_COMPLAINTS", 1.0);

setup_universe();

-- set player 1 location
if (player_ids[1]) then
	move_object(player_ids[1], 43173.244381, 8310.584136, 146096.771516);
end

function summon_to_base()
	demon_print("SUMMON TO BASE");
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "Incoming transmission from SB-00");
	end
        comms_transmission(starbase[1], "------------------");
        comms_transmission(starbase[1], "THIS IS ADMIRAL ASTON PIERCE AT STARBASE ZERO");
        comms_transmission(starbase[1], "ORBITING QUAREAUX");
	comms_transmission(starbase[1], "YOUR NEW ORDERS ARE TO REPORT TO SB-00 FOR");
	comms_transmission(starbase[1], "BRIEFING ON A TOP SECRET MISSION OF THE");
	comms_transmission(starbase[1], "UTMOST IMPORTANCE. I LOOK FORWARD TO YOUR");
	comms_transmission(starbase[1], "ARRIVAL AS SOON AS POSSIBLE");
	comms_transmission(starbase[1], "ADMIRAL PIERCE OUT.");
        comms_transmission(starbase[1], "------------------");
end

function encrypted_message_to_pierce()
	demon_print("ENCRYPTED MESSAGE");
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "Intercepted Encrypted transmission");
	end
	local cipher_key = generate_cipher_key();
	-- this is ROT13'd to avoid spoiling it for anyone reading this
		-- "NQZVENY CVREPR, JR HAQREFGNAQ GUR QRYVIREL BS LBHE YNGRFG FUVCZRAG BS FYNIRF " ..
		-- "JVYY ABG BPPHE NF GURL UNIR RFPNCRQ NAQ GNXRA BIRE GUR NHEBER. JR GEHFG LBH " ..
		-- "JVYY FRAQ NA NEZRQ FUVC GB QRFGEBL GUR NHEBER FB GURL PNHFR AB ZBER GEBHOYR.",
	comms_enciphered_transmission(zantar,
		"NQZVENY CVREPR, JR HAQREFGNAQ GUR QRYVIREL BS LBHE YNGRFG FUVCZRAG BS FYNIRF " ..
		"JVYY ABG BPPHE NF GURL UNIR RFPNCRQ GURVE OBAQF NAQ GNXRA BIRE GUR NHEBER. JR " ..
		"GEHFG LBH JVYY FRAQ NA NEZRQ FUVC GB QRFGEBL GUR NHEBER. N ERSHAQ VF BA GUR " ..
		"JNL.", cipher_key);
end

function second_message_from_pierce()
	mission_stage = 5;
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "Secure transmission from SB-00");
	end
        comms_transmission(starbase[1], "------------------");
	comms_transmission(starbase[1], "SECURE TRANSMISSION FROM ADMIRAL PIERCE");
	comms_transmission(starbase[1], "OPERATION MONKEYWRENCH INTELLIGENCE UPDATE");
	comms_transmission(starbase[1], "WE HAVE RECEIVED RELIABLE INTELLIGENCE THAT THE");
	comms_transmission(starbase[1], "TERRORIST CREW OF THE AURORE WILL CLAIM TO BE");
	comms_transmission(starbase[1], "REFUGEES SEEKING ASYLUM. THIS IS NOT TRUE. DO NOT");
	comms_transmission(starbase[1], "COMMUNICATE OR ATTEMPT TO NEGOTIATE WITH THE CREW OF");
	comms_transmission(starbase[1], "THE AURORE. YOUR MISSION IS TO DESTROY THE AURORE");
	comms_transmission(starbase[1], "AND ALL THE DANGEROUS TERRORISTS ABOARD.");
        comms_transmission(starbase[1], "------------------");
end

function message_from_bruiser()
	mission_stage = 4;
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "Secure transmission from starship Bruiser");
	end
	local name = get_object_name(player_ids[1]);
	if name == nil then
		name = ""
	end
	demon_print("MESSAGE FROM BRUISER");
        comms_transmission(bruiser, "------------------");
	comms_transmission(bruiser, "AHOY, " .. name .. " THIS IS CAPTAIN ZELLICK OF THE STARSHIP 'BRUISER'");
	comms_transmission(bruiser, "WE ARE ALSO PART OF OPERATION MONKEYWRENCH, JUST NOW");
	comms_transmission(bruiser, "RETURNING FROM ZANTAR.  JUST WANTED TO GIVE YOU ALL");
	comms_transmission(bruiser, "A LITTLE HEADS UP.  WELL, WE GOT OUR ASSES KICKED AND");
	comms_transmission(bruiser, "ARE HEADING BACK TO PORT FOR REPAIRS. SITUATION'S FUBAR.");
	comms_transmission(bruiser, "OPERATION MONKEYWRENCH... MORE LIKE OPERATION MONKEYSHIT!");
	comms_transmission(bruiser, "GOOD LUCK " .. name .. " AND BE CAREFUL.");
	comms_transmission(bruiser, "");
	comms_transmission(bruiser, "CAPTAIN ZELLICK OUT.");
        comms_transmission(bruiser, "------------------");
	register_timer_callback("encrypted_message_to_pierce", encrypted_message_time, 0)
	register_timer_callback("second_message_from_pierce", second_pierce_msg_time, 0);
	move_object(bruiser, 500000, 500000, 500000); -- get Bruiser out of here
end

function message_from_aurore()
	mission_stage = 5;
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "transmission from vessel Aurore");
	end
	comms_transmission(aurore, "MAYDAY MAYDAY MAYDAY");
	comms_transmission(aurore, "THIS IS THE VESSEL AURORE");
	comms_transmission(aurore, "WE WERE BEING TRANSPORTED FROM QUAREAUX TO THE");
	comms_transmission(aurore, "DEATH CAMPS ON ZANTAR, BUT WE ESCAPED OUR BONDS AND");
	comms_transmission(aurore, "TOOK OVER THE SHIP. CAN ANYONE HEAR US? WE NEED HELP!");
	comms_transmission(aurore, "EIGHT SOULS ABOARD. PLEASE HELP US!");
	comms_transmission(aurore, "WE WERE ATTACKED BY THE STARSHIP BRUISER AND IT IS");
	comms_transmission(aurore, "A MIRACLE WE SURVIVED. THE SHIP HAS NO POWER.");
	comms_transmission(aurore, "PLEASE HELP US.  WE MUST MAKE IT TO THE STARBASE");
	comms_transmission(aurore, "IN ORBIT AROUND CORBU!");
end

function give_player_orders()
	mission_stage = 3;
	demon_print("GIVE PLAYER ORDERS");
        comms_transmission(starbase[1], "------------------");
        comms_transmission(starbase[1], "THIS IS ADMIRAL ASTON PIERCE.");
        comms_transmission(starbase[1], "WELCOME TO STARBASE ZERO.");
	comms_transmission(starbase[1], "YOU ARE HERE TO BE BRIEFED ON OPERATION MONKEYWRENCH");
	comms_transmission(starbase[1], "THIS MISSION IS TOP SECRET, NO ONE MUST KNOW ABOUT IT");
	comms_transmission(starbase[1], "IF YOU ARE DISCOVERED, THE GOVERNMENT WILL DENY ALL");
	comms_transmission(starbase[1], "KNOWLEDGE.  I NOW TURN THINGS OVER TO THE PRESIDENT");
	comms_transmission(starbase[1], "");
	comms_transmission(starbase[1], "HELLO TROOPS, THIS IS YOUR PRESIDENT. YOU ARE BEING");
	comms_transmission(starbase[1], "ENTRUSTED TO CARRY OUT A MISSION OF THE UTMOST IMPORTANCE");
	comms_transmission(starbase[1], "I KNOW THAT YOU CAN DO IT, AND YOU'LL ALL BE HEROES");
	comms_transmission(starbase[1], "FOR SERVING THIS GREAT PLANET UPON ON YOUR RETURN");
	comms_transmission(starbase[1], "YOUR MISSION IS TO TRAVEL TO ZANTAR AND INTERCEPT THE VESSEL");
	comms_transmission(starbase[1], "AURORE. WE HAVE RECEIVED RELIABLE INTELLIGENCE THAT THIS");
	comms_transmission(starbase[1], "VESSEL IS CARRYING VIOLENT TERRORISTS INTENT ON DOING HARM");
	comms_transmission(starbase[1], "TO OUR GOVERNMENT AND TO OUR WAY OF LIFE.  ADDITIONALLY,");
	comms_transmission(starbase[1], "ON BOARD THE AURORE IS A NEW TYPE OF EXPLOSIVE WEAPON OF");
	comms_transmission(starbase[1], "SUCH DESTRUCTIVE POWER LIKE NOBODY'S EVER SEEN BEFORE. YOUR");
	comms_transmission(starbase[1], "MISSION IS TO DESTROY THE AURORE AND ALL ABOARD.");
	comms_transmission(starbase[1], "I KNOW YOU WILL SUCCEED.  GOOD LUCK AND GODSPEED.");
	comms_transmission(starbase[1], "NOW HERE IS ADMIRAL PIERCE AGAIN.");
	comms_transmission(starbase[1], "");
	comms_transmission(starbase[1], "ALRIGHT, I WON'T KEEP YOU ANY LONGER. YOU HAVE RECIEVED");
	comms_transmission(starbase[1], "YOUR ORDERS DIRECTLY FROM THE PRESIDENT.  NOW GO FORTH");
	comms_transmission(starbase[1], "AND MAKE OPERATION MONKEYWRENCH A SUCCESS!");
	comms_transmission(starbase[1], "");
	comms_transmission(starbase[1], "ADMIRAL PIERCE OUT.");
        comms_transmission(starbase[1], "------------------");
	register_timer_callback("message_from_bruiser", bruiser_msg_time, 0);
end

function player_docked_callback(player_id, station_id)
	if station_id == starbase[1] and mission_stage == 0
	then
		mission_stage = 1;
		register_timer_callback("give_player_orders", player_orders_time, 0)
	end
end

function approaching_aurore()
	message_from_aurore();
end

function very_near_aurore()
	mission_stage = 6;
	for i, player_id in ipairs(player_ids) do
		text_to_speech(player_id, "transmission from vessel Aurore");
	end
	local name = get_object_name(player_ids[1]);
	if name == nil then
		name = "APPROACHING UNKNOWN VESSEL"
	end
        comms_transmission(aurore, "------------------");
	comms_transmission(aurore, "THIS IS THE VESSEL AURORE");
	comms_transmission(aurore, "AHOY " .. name);
	comms_transmission(aurore, "WE DESPERATELY NEED TO BE TRANSPORTED ABOARD YOUR SHIP");
	comms_transmission(aurore, "DO YOU HAVE TRANSPORTER CAPABILITY?");
	comms_transmission(aurore, "WE HAVE NO POWER AND LIFE SUPPORT SYSTEMS HAVE FAILED");
	comms_transmission(aurore, "TIME IS OF THE ESSENCE.");
        comms_transmission(aurore, "------------------");
end

function epilogue()
	local name = get_object_name(player_ids[1]);
	show_timed_text(-1, 90,
		"EPILOGUE:\n" ..
		"ALTHOUGH THE CAPTAIN AND CREW OF THE " .. name .. "\n" ..
		"WERE AWARDED COMMENDATIONS AND MEDALS FOR THEIR\n" ..
		"ACTIONS THAT DAY, IN A FEW YEARS TIME, THE REIGN OF\n" ..
		"QUAREAUX CAME TO AN END, AND THEY WERE ALL TRACKED\n" ..
		"DOWN TO THEIR HIDING PLACE ON AN OUT OF THE WAY\n" ..
		"PLANET, BROUGHT BACK TO CORBU AND TRIED FOR THEIR\n" ..
		"CRIME OF DESTROYING THE AURORE AND ALL ABOARD, AND\n" ..
		"SENTENCED TO DEATH.\n");
end

function award_commendations()
	local name = get_object_name(player_ids[1]);
        comms_transmission(starbase[1], "------------------");
        comms_transmission(starbase[1], "THIS IS ADMIRAL PIERCE");
        comms_transmission(starbase[1], "YOU HAVE ALL DONE EXCELLENT WORK!");
        comms_transmission(starbase[1], "COMMENDATIONS TO THE CAPTAIN AND CREW");
	if name ~= nil then
		comms_transmission(starbase[1], "OF THE " .. name);
	else
		name = "YOUR SHIP";
	end
        comms_transmission(starbase[1], "THE CAPTAIN WILL RECEIVE THE HIGHEST HONOR,");
        comms_transmission(starbase[1], "THE BLACK STAR, FOR HIS ACTIONS IN THIS MISSION");
        comms_transmission(starbase[1], "TO DESTROY THE TERRORISTS ABOARD THE AURORE!");
	comms_transmission(starbase[1], "YOU ARE ALL HEROES TODAY!");
        comms_transmission(starbase[1], "GOD BLESS THE CREW OF THE " .. name .. "!");
        comms_transmission(starbase[1], "AND GOD BLESS QUAREAUX!");
        comms_transmission(starbase[1], "ADMIRAL PIERCE OUT!");
	register_timer_callback("epilogue", epilogue_msg_time, 0);
end

function handle_object_death_event(id)
	if id == aurore then
		demon_print("AURORA KILLED!");
		register_timer_callback("award_commendations", commendations_msg_time, 0);
	end
	if id == player_ids[1] then
		demon_print("PLAYER KILLED!");
	end
end

aurore_complains_flag = 0;
function aurore_complains()
	if aurore_complains_flag == 1 then
		return;
	end
	aurore_complains_flag = 1;
	local name = get_object_name(player_ids[1]);
	if name == nil then
		name = "APPROACHING UNKNOWN VESSEL"
	end
        comms_transmission(aurore, "------------------");
	comms_transmission(aurore, "THIS IS THE VESSEL AURORE");
	comms_transmission(aurore, "AHOY " .. name);
	comms_transmission(aurore, "WHY ARE YOU FIRING UPON US???");
        comms_transmission(aurore, "------------------");
end

aurore_complains_two_flag = 0;
function aurore_complains_two()
	if aurore_complains_two_flag == 1 then
		return;
	end
	aurore_complains_two_flag = 1;
	local name = get_object_name(player_ids[1]);
	if name == nil then
		name = "APPROACHING UNKNOWN VESSEL"
	end
        comms_transmission(aurore, "------------------");
	comms_transmission(aurore, "THIS IS THE VESSEL AURORE");
	comms_transmission(aurore, "WE ARE BEING FIRED UPON!  PLEASE HELP US!");
        comms_transmission(aurore, "------------------");
end

function handle_object_hit_event(target_id, attacker_id)
	if target_id == aurore then
		if attacker_id == player_ids[1] then
			demon_print("PLAYER SHOT AURORE!");
			register_timer_callback("aurore_complains", 10, 0);
		else
			register_timer_callback("aurore_complains_two", 10, 0);
		end
	end
end

function make_passenger(n, name)
	id = create_passenger(n, name, starbase[1], starbase[2], 0.0)
	set_passenger_location(n, player_ids[1]);
end

function science_custom_button_pressed()
	local id = get_science_selection(player_ids[1]);

	if (id == aurore and players_took_passengers == 0) then
		players_took_passengers = 1;
		text_to_speech(player_ids[1], "8 Passengers transported aboard from the vessel Aurore");
		make_passenger(0, "DR. E. BROWN");
		make_passenger(1, "MR. S. B. VON BLINN");
		make_passenger(2, "MR. F. A. VON BLINN");
		make_passenger(3, "MR. J. J. CARTER");
		make_passenger(4, "MR. B. LARGO");
		make_passenger(5, "DR. Z. WELLINGTON");
		make_passenger(6, "MR. S. KLONOBAR");
		make_passenger(7, "MRS. Y. NORTHFIRE");
	else
		play_sound(player_ids[1], "sounds/quindar-outro.ogg");
	end
end

function custom_button_press(shipid, button)
	if shipid ~= player_ids[1] then
		register_callback("custom-button-press-event", "custom_button_press");
		return;
	end
	if button == 4 then
		-- science
		science_custom_button_pressed();
	end
	register_callback("custom-button-press-event", "custom_button_press");
end

function setup_custom_buttons()
	set_custom_button_label(player_ids[1], 4, "TRANSPORTER");
	enable_custom_button(player_ids[1], 4);
	register_callback("custom-button-press-event", "custom_button_press");
end

function maintain_attacker_focus()
	for i = 1,num_attackers,1 do
		ai_push_attack(attacker[i], player_ids[1]);
	end
	register_timer_callback("maintain_attacker_focus", 200, 0);
end

attacker = {};
function add_attacker(i)
	local x, y, z = random_point_on_sphere(5000);
	px, py, pz = get_object_location(player_ids[1]);
	x = x + px;
	y = y + py;
	z = z + pz;
	attacker[i] = add_ship(generate_ship_name(), x, y, z, 18, 0, 0);
	ai_push_attack(attacker[i], player_ids[1]);
	register_timer_callback("maintain_attacker_focus", 200, 0);
end

function revenge_of_the_govt2()
	for i = 1,num_attackers,1 do
		add_attacker(i);
	end
end

function revenge_of_the_govt()
	demon_print("REVENGE OF THE GOVT");
	text_to_speech(player_ids[1], "Incoming transmission from SB-00");
	local name = get_object_name(player_ids[1]);
	if name == nil then
		name = ""
	end
        comms_transmission(starbase[1], "------------------");
        comms_transmission(starbase[1], "ATTENTION CREW OF " .. name);
        comms_transmission(starbase[1], "WE KNOW WHAT YOU HAVE DONE");
        comms_transmission(starbase[1], "YOU ARE ALL GUILTY OF DEFYING DIRECT");
        comms_transmission(starbase[1], "ORDERS OF THE PRESIDENT!");
        comms_transmission(starbase[1], "YOU WILL NOT ESCAPE THE JUSTICE YOU HAVE");
        comms_transmission(starbase[1], "EARNED FOR YOUR TRAITOROUS ACTIONS!");
        comms_transmission(starbase[1], "ADMIRAL PIERCE OUT!");
        comms_transmission(starbase[1], "------------------");
	register_timer_callback("revenge_of_the_govt2", govt_attack_time, 0)
end

delivered_passengers = 0;
function handle_passenger_disembarked(passenger_no, destination_id)
	if passenger_no >= 0 and passenger_no <= 7 and destination_id == starbase[2] then
		delivered_passengers = delivered_passengers + 1;
		if (delivered_passengers == 8) then
			register_timer_callback("revenge_of_the_govt", govt_revenge_msg_time, 0)
		end
	end
end

text_to_speech(player_ids[1], "Begin mission: Operation monkey wrench");
setup_custom_buttons();
register_timer_callback("summon_to_base", summon_to_base_time, 0)
register_callback("player-docked-event", "player_docked_callback");
register_proximity_callback("approaching_aurore", aurore, player_ids[1], 10000);
register_proximity_callback("very_near_aurore", aurore, player_ids[1], 5000);
register_callback("object-death-event", "handle_object_death_event", 0);
register_callback("passenger-disembarked", "handle_passenger_disembarked", 0);

