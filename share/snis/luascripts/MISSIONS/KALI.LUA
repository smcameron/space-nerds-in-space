-- kali threatens the crew
clear_all();

first_attack_time = 1 * 600;
first_message_time = 2 * 600;
first_reminder_time = 5 * 600;
second_reminder_time = 10 * 600;
congratulations_time = 5 * 10;
package_delivered = 0;

adjx = -300000;
adjz = -300000;


planet = {};
planet[1] = add_planet("XIU", adjx + 84830.516244, -46920.410938, adjz + 510136.495396, 3288.000000, 0);
planet[2] = add_planet("GECHOE", adjx + 504203.896087, 0.000000, adjz + 399869.082309, 5162.000000, 0);
planet[3] = add_planet("ASTU", adjx + 349077.315046, 0.000000, adjz + 83232.242653, 2910.000000, 0);
planet[4] = add_planet("ENA", adjx + 100747.109997, 0.000000, adjz + 151538.980078, 6222.000000, 0);
starbase = {};
starbase[1] = add_starbase(adjx + 87495.178025, -36022.003384, adjz + 507192.906228, 0);
nebula = {};
nebula[1] = add_nebula("", 504681.600865, 0.000000, 494884.181998, 3821.000000);
nebula[2] = add_nebula("", 487869.774032, 0.000000, 496314.976223, 3315.000000);
nebula[3] = add_nebula("", 490373.663088, 0.000000, 499891.960109, 2816.000000);
nebula[4] = add_nebula("", 493235.251537, 0.000000, 507403.627274, 629.000000);
nebula[5] = add_nebula("", 499673.822755, 0.000000, 509549.818053, 2554.000000);
nebula[6] = add_nebula("", 493950.648091, 0.000000, 499176.563555, 3409.000000);
nebula[7] = add_nebula("", 485365.884977, 0.000000, 502753.548558, 1723.000000);
nebula[8] = add_nebula("", 487869.774032, 0.000000, 507403.627274, 3900.000000);
nebula[9] = add_nebula("", 495023.742921, 0.000000, 509549.818053, 3589.000000);
nebula[10] = add_nebula("", 491089.060759, 0.000000, 517776.881772, 871.000000);
nebula[11] = add_nebula("", 489658.266534, 0.000000, 519207.675997, 1545.000000);
nebula[12] = add_nebula("", 491446.759036, 0.000000, 515272.993834, 4748.000000);
nebula[13] = add_nebula("", 493592.949814, 0.000000, 512411.405385, 3617.000000);

function add_adjusted_asteroid(x, y, z)
  return add_asteroid(adjx + x, y, adjz + z);
end

asteroid = {};
asteroid[1] = add_adjusted_asteroid(447664.041520, 0.000000, 527007.684957);
set_asteroid_speed(asteroid[1], 0.000000);
asteroid[2] = add_adjusted_asteroid(450449.884346, 0.000000, 528238.714427);
set_asteroid_speed(asteroid[2], 0.000000);
asteroid[3] = add_adjusted_asteroid(454385.627742, 0.000000, 527529.416877);
set_asteroid_speed(asteroid[3], 0.000000);
asteroid[4] = add_adjusted_asteroid(458082.012757, 0.000000, 528363.406961);
set_asteroid_speed(asteroid[4], 0.000000);
asteroid[5] = add_adjusted_asteroid(459103.551538, 0.000000, 529288.805308);
set_asteroid_speed(asteroid[5], 0.000000);
asteroid[6] = add_adjusted_asteroid(457688.818609, 0.000000, 531335.823747);
set_asteroid_speed(asteroid[6], 0.000000);
asteroid[7] = add_adjusted_asteroid(444815.012217, 0.000000, 533675.650478);
set_asteroid_speed(asteroid[7], 0.000000);
asteroid[8] = add_adjusted_asteroid(444142.235541, 0.000000, 534366.008543);
set_asteroid_speed(asteroid[8], 0.000000);
asteroid[9] = add_adjusted_asteroid(442325.256792, 0.000000, 535501.833891);
set_asteroid_speed(asteroid[9], 0.000000);
asteroid[10] = add_adjusted_asteroid(443353.546836, 0.000000, 538826.692512);
set_asteroid_speed(asteroid[10], 0.000000);
asteroid[11] = add_adjusted_asteroid(443342.811287, 0.000000, 539881.646401);
set_asteroid_speed(asteroid[11], 0.000000);
asteroid[12] = add_adjusted_asteroid(441223.321058, 0.000000, 541253.003496);
set_asteroid_speed(asteroid[12], 0.000000);
asteroid[13] = add_adjusted_asteroid(438736.599273, 0.000000, 540759.215404);
set_asteroid_speed(asteroid[13], 0.000000);
asteroid[14] = add_adjusted_asteroid(445393.315768, 0.000000, 542002.577711);
set_asteroid_speed(asteroid[14], 0.000000);
asteroid[15] = add_adjusted_asteroid(449986.710425, 0.000000, 542772.525832);
set_asteroid_speed(asteroid[15], 0.000000);
asteroid[16] = add_adjusted_asteroid(450716.005772, 0.000000, 543680.801248);
set_asteroid_speed(asteroid[16], 0.000000);
asteroid[17] = add_adjusted_asteroid(445934.885269, 0.000000, 546353.549897);
set_asteroid_speed(asteroid[17], 0.000000);
asteroid[18] = add_adjusted_asteroid(442916.359480, 0.000000, 547538.538249);
set_asteroid_speed(asteroid[18], 0.000000);
asteroid[19] = add_adjusted_asteroid(442752.948530, 0.000000, 549617.145984);
set_asteroid_speed(asteroid[19], 0.000000);
asteroid[20] = add_adjusted_asteroid(446531.857714, 0.000000, 550399.893987);
set_asteroid_speed(asteroid[20], 0.000000);
asteroid[21] = add_adjusted_asteroid(450893.551816, 0.000000, 549240.592689);
set_asteroid_speed(asteroid[21], 0.000000);
asteroid[22] = add_adjusted_asteroid(452270.909880, 0.000000, 547872.325631);
set_asteroid_speed(asteroid[22], 0.000000);
asteroid[23] = add_adjusted_asteroid(454504.380935, 0.000000, 546793.812552);
set_asteroid_speed(asteroid[23], 0.000000);
asteroid[24] = add_adjusted_asteroid(454969.508721, 0.000000, 542836.849715);
set_asteroid_speed(asteroid[24], 0.000000);
asteroid[25] = add_adjusted_asteroid(454360.510714, 0.000000, 542401.603045);
set_asteroid_speed(asteroid[25], 0.000000);
asteroid[26] = add_adjusted_asteroid(451779.897246, 0.000000, 537782.793938);
set_asteroid_speed(asteroid[26], 0.000000);
asteroid[27] = add_adjusted_asteroid(455287.732556, 0.000000, 535634.684192);
set_asteroid_speed(asteroid[27], 0.000000);
asteroid[28] = add_adjusted_asteroid(458989.298807, 0.000000, 538000.928251);
set_asteroid_speed(asteroid[28], 0.000000);
asteroid[29] = add_adjusted_asteroid(459373.798091, 0.000000, 539604.948128);
set_asteroid_speed(asteroid[29], 0.000000);
asteroid[30] = add_adjusted_asteroid(464305.552260, 0.000000, 538767.111108);
set_asteroid_speed(asteroid[30], 0.000000);
asteroid[31] = add_adjusted_asteroid(464776.158548, 0.000000, 538257.006967);
set_asteroid_speed(asteroid[31], 0.000000);
asteroid[32] = add_adjusted_asteroid(461386.090516, 0.000000, 534222.116205);
set_asteroid_speed(asteroid[32], 0.000000);
asteroid[33] = add_adjusted_asteroid(456990.874518, 0.000000, 534367.054473);
set_asteroid_speed(asteroid[33], 0.000000);
asteroid[34] = add_adjusted_asteroid(455270.835429, 0.000000, 537381.497837);
set_asteroid_speed(asteroid[34], 0.000000);
asteroid[35] = add_adjusted_asteroid(455917.524787, 0.000000, 540819.838042);
set_asteroid_speed(asteroid[35], 0.000000);
asteroid[36] = add_adjusted_asteroid(453127.291048, 0.000000, 539461.957858);
set_asteroid_speed(asteroid[36], 0.000000);
asteroid[37] = add_adjusted_asteroid(440262.701532, 0.000000, 532016.364045);
set_asteroid_speed(asteroid[37], 0.000000);
asteroid[38] = add_adjusted_asteroid(436726.482567, 0.000000, 531775.616137);
set_asteroid_speed(asteroid[38], 0.000000);
asteroid[39] = add_adjusted_asteroid(435957.141834, 0.000000, 535975.230214);
set_asteroid_speed(asteroid[39], 0.000000);
asteroid[40] = add_adjusted_asteroid(437399.760781, 0.000000, 538654.461812);
set_asteroid_speed(asteroid[40], 0.000000);
asteroid[41] = add_adjusted_asteroid(436638.256162, 0.000000, 543530.348721);
set_asteroid_speed(asteroid[41], 0.000000);
asteroid[42] = add_adjusted_asteroid(434555.193970, 0.000000, 546734.545000);
set_asteroid_speed(asteroid[42], 0.000000);
asteroid[43] = add_adjusted_asteroid(433976.031468, 0.000000, 551052.755126);
set_asteroid_speed(asteroid[43], 0.000000);
asteroid[44] = add_adjusted_asteroid(443484.989099, 0.000000, 554639.836465);
set_asteroid_speed(asteroid[44], 0.000000);
asteroid[45] = add_adjusted_asteroid(454477.829496, 0.000000, 557294.558634);
set_asteroid_speed(asteroid[45], 0.000000);
asteroid[46] = add_adjusted_asteroid(463932.349742, 0.000000, 555046.547602);
set_asteroid_speed(asteroid[46], 0.000000);
asteroid[47] = add_adjusted_asteroid(469885.342530, 0.000000, 551101.694065);
set_asteroid_speed(asteroid[47], 0.000000);
asteroid[48] = add_adjusted_asteroid(474837.099876, 0.000000, 540678.873144);
set_asteroid_speed(asteroid[48], 0.000000);
asteroid[49] = add_adjusted_asteroid(474054.215655, 0.000000, 536013.502621);
set_asteroid_speed(asteroid[49], 0.000000);
asteroid[50] = add_adjusted_asteroid(470009.721038, 0.000000, 528120.225570);
set_asteroid_speed(asteroid[50], 0.000000);
asteroid[51] = add_adjusted_asteroid(452339.896297, 0.000000, 522751.996278);
set_asteroid_speed(asteroid[51], 0.000000);
asteroid[52] = add_adjusted_asteroid(452213.353801, 0.000000, 522746.104645);
set_asteroid_speed(asteroid[52], 0.000000);
asteroid[53] = add_adjusted_asteroid(459983.626740, 0.000000, 524713.128249);
set_asteroid_speed(asteroid[53], 0.000000);
asteroid[54] = add_adjusted_asteroid(471005.751738, 0.000000, 531739.904846);
set_asteroid_speed(asteroid[54], 0.000000);
asteroid[55] = add_adjusted_asteroid(468692.960814, 0.000000, 538072.066374);
set_asteroid_speed(asteroid[55], 0.000000);
asteroid[56] = add_adjusted_asteroid(467865.079674, 0.000000, 541910.717411);
set_asteroid_speed(asteroid[56], 0.000000);
asteroid[57] = add_adjusted_asteroid(465379.084792, 0.000000, 546112.663845);
set_asteroid_speed(asteroid[57], 0.000000);
asteroid[58] = add_adjusted_asteroid(455772.976665, 0.000000, 550423.153793);
set_asteroid_speed(asteroid[58], 0.000000);
asteroid[59] = add_adjusted_asteroid(453174.766071, 0.000000, 551937.448232);
set_asteroid_speed(asteroid[59], 0.000000);
asteroid[60] = add_adjusted_asteroid(450471.132724, 0.000000, 552225.494855);
set_asteroid_speed(asteroid[60], 0.000000);
asteroid[61] = add_adjusted_asteroid(462350.861827, 0.000000, 501096.995540);
set_asteroid_speed(asteroid[61], 0.000000);
asteroid[62] = add_adjusted_asteroid(478213.541951, 0.000000, 502870.593547);
set_asteroid_speed(asteroid[62], 0.000000);
asteroid[63] = add_adjusted_asteroid(481888.506983, 0.000000, 501621.022295);
set_asteroid_speed(asteroid[63], 0.000000);
asteroid[64] = add_adjusted_asteroid(485520.654931, 0.000000, 514687.918226);
set_asteroid_speed(asteroid[64], 0.000000);
asteroid[65] = add_adjusted_asteroid(483047.519752, 0.000000, 517384.753343);
set_asteroid_speed(asteroid[65], 0.000000);
asteroid[66] = add_adjusted_asteroid(464416.813336, 0.000000, 513983.729960);
set_asteroid_speed(asteroid[66], 0.000000);
asteroid[67] = add_adjusted_asteroid(458671.838998, 0.000000, 512035.059478);
set_asteroid_speed(asteroid[67], 0.000000);
asteroid[68] = add_adjusted_asteroid(461650.641623, 0.000000, 509777.159589);
set_asteroid_speed(asteroid[68], 0.000000);
asteroid[69] = add_adjusted_asteroid(478026.731378, 0.000000, 510787.471470);
set_asteroid_speed(asteroid[69], 0.000000);
asteroid[70] = add_adjusted_asteroid(477825.608858, 0.000000, 513772.128385);
set_asteroid_speed(asteroid[70], 0.000000);
asteroid[71] = add_adjusted_asteroid(475761.426565, 0.000000, 518681.934898);
set_asteroid_speed(asteroid[71], 0.000000);
asteroid[72] = add_adjusted_asteroid(464673.423808, 0.000000, 511241.784233);
set_asteroid_speed(asteroid[72], 0.000000);
asteroid[73] = add_adjusted_asteroid(467402.675307, 0.000000, 509445.772789);
set_asteroid_speed(asteroid[73], 0.000000);
asteroid[74] = add_adjusted_asteroid(483864.257335, 0.000000, 518623.272729);
set_asteroid_speed(asteroid[74], 0.000000);
asteroid[75] = add_adjusted_asteroid(484839.700924, 0.000000, 526026.910275);
set_asteroid_speed(asteroid[75], 0.000000);
asteroid[76] = add_adjusted_asteroid(476552.147279, 0.000000, 529378.082020);
set_asteroid_speed(asteroid[76], 0.000000);
asteroid[77] = add_adjusted_asteroid(469106.414177, 0.000000, 522830.235732);
set_asteroid_speed(asteroid[77], 0.000000);
asteroid[78] = add_adjusted_asteroid(457826.148889, 0.000000, 521177.803586);
set_asteroid_speed(asteroid[78], 0.000000);
asteroid[79] = add_adjusted_asteroid(458069.387544, 0.000000, 519976.831933);
set_asteroid_speed(asteroid[79], 0.000000);
asteroid[80] = add_adjusted_asteroid(462671.981074, 0.000000, 518025.551463);
set_asteroid_speed(asteroid[80], 0.000000);
asteroid[81] = add_adjusted_asteroid(471279.814847, 0.000000, 519880.081770);
set_asteroid_speed(asteroid[81], 0.000000);
asteroid[82] = add_adjusted_asteroid(480973.154055, 0.000000, 525846.485390);
set_asteroid_speed(asteroid[82], 0.000000);
asteroid[83] = add_adjusted_asteroid(480518.731304, 0.000000, 534981.797752);
set_asteroid_speed(asteroid[83], 0.000000);
asteroid[84] = add_adjusted_asteroid(478574.198398, 0.000000, 536644.207072);
set_asteroid_speed(asteroid[84], 0.000000);
asteroid[85] = add_adjusted_asteroid(487173.599300, 0.000000, 510329.543876);
set_asteroid_speed(asteroid[85], 0.000000);
asteroid[86] = add_adjusted_asteroid(486321.417902, 0.000000, 504630.192974);
set_asteroid_speed(asteroid[86], 0.000000);
asteroid[87] = add_adjusted_asteroid(492840.216430, 0.000000, 492661.361208);
set_asteroid_speed(asteroid[87], 0.000000);
asteroid[88] = add_adjusted_asteroid(476044.271139, 0.000000, 490508.730231);
set_asteroid_speed(asteroid[88], 0.000000);
asteroid[89] = add_adjusted_asteroid(473553.577908, 0.000000, 496880.854247);
set_asteroid_speed(asteroid[89], 0.000000);
asteroid[90] = add_adjusted_asteroid(493384.923578, 0.000000, 487977.936838);
set_asteroid_speed(asteroid[90], 0.000000);
asteroid[91] = add_adjusted_asteroid(496182.669117, 0.000000, 483882.665223);
set_asteroid_speed(asteroid[91], 0.000000);
asteroid[92] = add_adjusted_asteroid(504772.853122, 0.000000, 474254.160684);
set_asteroid_speed(asteroid[92], 0.000000);
asteroid[93] = add_adjusted_asteroid(500799.047625, 0.000000, 469554.208237);
set_asteroid_speed(asteroid[93], 0.000000);
asteroid[94] = add_adjusted_asteroid(484653.478971, 0.000000, 485977.411737);
set_asteroid_speed(asteroid[94], 0.000000);
asteroid[95] = add_adjusted_asteroid(481083.668755, 0.000000, 492244.420098);
set_asteroid_speed(asteroid[95], 0.000000);
asteroid[96] = add_adjusted_asteroid(506995.698537, 0.000000, 461763.634630);
set_asteroid_speed(asteroid[96], 0.000000);
asteroid[97] = add_adjusted_asteroid(507651.209823, 0.000000, 460928.736977);
set_asteroid_speed(asteroid[97], 0.000000);
asteroid[98] = add_adjusted_asteroid(507723.711145, 0.000000, 460651.388196);
set_asteroid_speed(asteroid[98], 0.000000);
asteroid[99] = add_adjusted_asteroid(500336.343428, 0.000000, 468344.320405);
set_asteroid_speed(asteroid[99], 0.000000);
asteroid[100] = add_adjusted_asteroid(495919.984291, 0.000000, 474083.866563);
set_asteroid_speed(asteroid[100], 0.000000);
asteroid[101] = add_adjusted_asteroid(487868.565703, 0.000000, 483157.980787);
set_asteroid_speed(asteroid[101], 0.000000);
asteroid[102] = add_adjusted_asteroid(489256.364000, 0.000000, 477832.013568);
set_asteroid_speed(asteroid[102], 0.000000);
asteroid[103] = add_adjusted_asteroid(489116.863218, 0.000000, 455345.284206);
set_asteroid_speed(asteroid[103], 0.000000);
asteroid[104] = add_adjusted_asteroid(482075.444514, 0.000000, 465794.613574);
set_asteroid_speed(asteroid[104], 0.000000);
asteroid[105] = add_asteroid(484149.571591, 0.000000, 474058.669025);
set_asteroid_speed(asteroid[105], 0.000000);
asteroid[106] = add_adjusted_asteroid(481373.865481, 0.000000, 480120.557064);
set_asteroid_speed(asteroid[106], 0.000000);
asteroid[107] = add_adjusted_asteroid(501644.879781, 0.000000, 481020.924790);
set_asteroid_speed(asteroid[107], 0.000000);
asteroid[108] = add_adjusted_asteroid(505718.628920, 0.000000, 492995.988838);
set_asteroid_speed(asteroid[108], 0.000000);
asteroid[109] = add_adjusted_asteroid(517255.926997, 0.000000, 504306.280047);
set_asteroid_speed(asteroid[109], 0.000000);
asteroid[110] = add_adjusted_asteroid(523233.265661, 0.000000, 497063.721474);
set_asteroid_speed(asteroid[110], 0.000000);
asteroid[111] = add_adjusted_asteroid(529629.758349, 0.000000, 482703.130600);
set_asteroid_speed(asteroid[111], 0.000000);
asteroid[112] = add_adjusted_asteroid(512997.506013, 0.000000, 490348.607019);
set_asteroid_speed(asteroid[112], 0.000000);
asteroid[113] = add_adjusted_asteroid(491076.847245, 0.000000, 510079.508084);
set_asteroid_speed(asteroid[113], 0.000000);
asteroid[114] = add_adjusted_asteroid(501674.897127, 0.000000, 515896.983399);
set_asteroid_speed(asteroid[114], 0.000000);
asteroid[115] = add_adjusted_asteroid(520397.137991, 0.000000, 508560.283619);
set_asteroid_speed(asteroid[115], 0.000000);
asteroid[116] = add_adjusted_asteroid(517372.970190, 0.000000, 493533.367289);
set_asteroid_speed(asteroid[116], 0.000000);
asteroid[117] = add_adjusted_asteroid(516718.440785, 0.000000, 471002.782865);
set_asteroid_speed(asteroid[117], 0.000000);
asteroid[118] = add_adjusted_asteroid(510832.819919, 0.000000, 475297.691556);
set_asteroid_speed(asteroid[118], 0.000000);
asteroid[119] = add_adjusted_asteroid(452657.717477, 0.000000, 472719.624661);
set_asteroid_speed(asteroid[119], 0.000000);
asteroid[120] = add_adjusted_asteroid(471343.141052, 0.000000, 485024.753000);
set_asteroid_speed(asteroid[120], 0.000000);
asteroid[121] = add_adjusted_asteroid(477870.692997, 0.000000, 475679.413847);
set_asteroid_speed(asteroid[121], 0.000000);
asteroid[122] = add_adjusted_asteroid(480566.387052, 0.000000, 466120.201660);
set_asteroid_speed(asteroid[122], 0.000000);
asteroid[123] = add_adjusted_asteroid(487726.476042, 0.000000, 458321.420821);
set_asteroid_speed(asteroid[123], 0.000000);
asteroid[124] = add_adjusted_asteroid(500672.960133, 0.000000, 449343.174427);
set_asteroid_speed(asteroid[124], 0.000000);
asteroid[125] = add_adjusted_asteroid(510709.109747, 0.000000, 448864.315221);
set_asteroid_speed(asteroid[125], 0.000000);
asteroid[126] = add_adjusted_asteroid(529399.567894, 0.000000, 471406.164709);
set_asteroid_speed(asteroid[126], 0.000000);
spacemonster = {};
spacemonster[1] = add_spacemonster("M. MYSTERIUM", 544953.766762, 4779.238533, 443185.267005);
starbase[2] = add_starbase(adjx + 95152.453377, 0.000000, adjz + 152940.501344, 1);
starbase[3] = add_starbase(adjx + 494110.780439, 15915.886320, adjz + 397065.624035, 2);
starbase[4] = add_starbase(adjx + 352703.155369, -2643.565090, adjz + 89265.429270, 3);
astu_starbase = starbase[4];
spacemonster[2] = add_spacemonster("M. MYSTERIUM", adjx + 543800.333857, 6945.276312, adjz + 442531.363186);
spacemonster[3] = add_spacemonster("M. MYSTERIUM", adjx + 499673, 20, adjz + 509549);
spacemonster[4] = add_spacemonster("M. MYSTERIUM", adjx + 499673, 40, adjz + 509549);
spacemonster[5] = add_spacemonster("M. MYSTERIUM", adjx + 543800.333857, 6945.276312, adjz + 442531.363186);
spacemonster[6] = add_spacemonster("M. MYSTERIUM", adjx + 499673, 20, adjz + 509549);
spacemonster[7] = add_spacemonster("M. MYSTERIUM", adjx + 499673, 40, adjz + 509549);
spacemonster[8] = add_spacemonster("M. MYSTERIUM", adjx + 543800.333857, 6945.276312, adjz + 442531.363186);
spacemonster[9] = add_spacemonster("M. MYSTERIUM", adjx + 499673, 20, adjz + 509549);
spacemonster[10] = add_spacemonster("M. MYSTERIUM", adjx + 499673, 40, adjz + 509549);

function add_adjusted_ship(name, x, y, z, a, b, c)
	return add_ship(name, adjx + x, y, adjz + z, a, b, c);
end

ship = {};
ship[1] = add_adjusted_ship("ATHE CETUS", 493900.001664, 16056.207614, 397346.851430, 0, 1, 1);
ship[2] = add_adjusted_ship("SCIMITAR ALTA", 348423.824458, -2734.237066, 85844.499950, 0, 4, 1);
ship[3] = add_adjusted_ship("LONTANA JAGUAR", 326631.714525, -7247.959405, 122301.558355, 0, 1, 1);
ship[4] = add_adjusted_ship("VALKYRIE SILENCIO", 41402.101898, -329.017318, 36000.971116, 0, 2, 1);
ship[5] = add_adjusted_ship("EARLY VOLCAN", 493499.048677, 9168.428938, 401921.021561, 0, 3, 1);
ship[6] = add_adjusted_ship("MOONGRAZER STARRY", 97328.043113, 4472.681873, 157359.406679, 0, 4, 1);
ship[7] = add_adjusted_ship("VOIDSKY CHARMANT", 351632.896082, -1511.276634, 89060.698671, 0, 4, 1);
ship[8] = add_adjusted_ship("EKITH STARLIGHT", 347578.872766, 933.497703, 86533.115505, 0, 1, 1);
ship[9] = add_adjusted_ship("GOLDEN VALIANT", 500564.792527, -2652.311668, 404864.344184, 0, 1, 1);
ship[10] = add_adjusted_ship("VOLARE TARANTULA", 352284.867481, -1873.216279, 89009.938401, 0, 1, 1);
ship[11] = add_adjusted_ship("WOLF FARTHER", 84026.415666, -42683.215038, 510490.640857, 0, 1, 1);
ship[12] = add_adjusted_ship("PANZER GALAXIE", 352458.312716, -2601.115895, 89251.531682, 0, 1, 1);
ship[13] = add_adjusted_ship("STARLIGHT UDI", 353504.401276, -2510.871417, 89421.806518, 0, 1, 1);
ship[14] = add_adjusted_ship("CETUS WALRUS", 97575.692315, -5255.451327, 154526.856703, 0, 2, 1);
ship[15] = add_adjusted_ship("CARINA ARROW", 89480.063836, -46116.896404, 510552.053472, 0, 1, 1);
ship[16] = add_adjusted_ship("STARMAN ESHA", 494325.450916, 3218.481950, 388399.806626, 0, 1, 1);
ship[17] = add_adjusted_ship("VOLARE MONTAGNE", 493936.119452, 15888.617153, 396814.877853, 0, 1, 1);
ship[18] = add_adjusted_ship("ZIZOU SHINING", 452159.624659, -89.632015, 265230.705154, 0, 0, 1);
ship[19] = add_adjusted_ship("MOONGRAZER SILENCIO", 117074.624453, 331.365723, 152440.053153, 0, 0, 1);
ship[20] = add_adjusted_ship("HIGH PHOTON", 351669.391922, -73.167909, 86108.323492, 0, 3, 1);
ship[22] = add_adjusted_ship("BONZAAR KARNACH", 352531.392261, -2633.297044, 89513.144837, 0, 3, 1);
ship[23] = add_adjusted_ship("PLANET STARGAZER", 351431.961135, -1916.420355, 89547.890154, 0, 4, 1);
ship[24] = add_adjusted_ship("ANDROMEDA STUK", 84053.913054, -50270.492810, 513317.501757, 0, 2, 1);
ship[25] = add_adjusted_ship("ALTO CERBERUS", 493526.817183, -5346.606139, 367739.505930, 0, 0, 1);
ship[26] = add_adjusted_ship("STARLINK FESTU", 353236.899500, -2700.641421, 91513.186709, 0, 2, 1);
attacker = add_adjusted_ship("SILENT SCORPION", 1000, 1000, 1000, 18, 3, 0);

macguffin = add_commodity("ELECTRONICS", "SEX ROBOTS", "UNITS", "SEX ROBOTS", 10000.0, 0.1, 0.2, 0);
container = add_cargo_container(adjx + 499673, 0, adjz + 509549);
set_object_velocity(container, 0, 0, 0);
set_commodity_contents(container, macguffin, 50);

-- set player 1 location
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	reset_player_ship(player_ids[1]);
	move_object(player_ids[1], adjx + 87395, -36122, adjz + 507192);
	dock_player_to_starbase(player_ids[1], starbase[1]);
end

first_channel = 1000 + math.random(8000)
cargo_acquired = 0;

function first_listener(from, channel, message)
  comms_channel_unlisten("ALTO CERBERUS", channel);
  comms_channel_transmit("ALTO CERBERUS", channel, ": AHOY!");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THIS IS CAPTAIN NORBU OF THE ALTO CERBERUS.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": WE HAVE A MUTUAL ACQUAINTANCE.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": AS I AM SURE YOU ARE ALL TOO AWARE, YOU OWE");
  comms_channel_transmit("ALTO CERBERUS", channel, ": A CONSIDERABLE DEBT TO MY EMPLOYER, VARKON KALI.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": IT HAS BECOME CLEAR THAT YOU ARE UNABLE TO PAY THIS");
  comms_channel_transmit("ALTO CERBERUS", channel, ": DEBT. ORDINARILY THIS WOULD BE SUFFICIENT CAUSE TO");
  comms_channel_transmit("ALTO CERBERUS", channel, ": ORDER THE DESTRUCTION OF YOU AND YOUR SHIP.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": HOWEVER, THERE IS POSSIBLY ANOTHER WAY.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THERE IS A CERTAIN, UH, CARGO IN A CERTAIN LOCATION");
  comms_channel_transmit("ALTO CERBERUS", channel, ": WHICH MY EMPLOYER WOULD VERY MUCH LIKE TO RETRIEVE");
  comms_channel_transmit("ALTO CERBERUS", channel, ": BUT IS UNABLE TO DO SO FOR . . . REASONS.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": YOU ARE TO RETRIEVE THIS ITEM, WHICH IS IN THE");
  comms_channel_transmit("ALTO CERBERUS", channel, ": GONNRA ASTEROID FIELD AND RETURN IT TO THE STATION");
  comms_channel_transmit("ALTO CERBERUS", channel, ": IN ORBIT AROUND THE PLANET ASTU.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": RETRIEVE THE ITEM, AND THE DEBT IS FORGIVEN.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": FAIL TO RETRIEVE THE ITEM WITH SUFFICIENT");
  comms_channel_transmit("ALTO CERBERUS", channel, ": ALACRITY OR DISCRETION AND YOU WILL BE DESTROYED.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": GOOD LUCK.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": CAPTAIN NORBU OUT.");
end

function reminder1_listener(from, channel, message)
  comms_channel_unlisten("ALTO CERBERUS", channel);
  comms_channel_transmit("ALTO CERBERUS", channel, ": AHOY!");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THIS IS CAPTAIN NORBU OF THE ALTO CERBERUS.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": JUST REMINDING YOU TO DELIVER ON TIME . . . OR ELSE.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": YOU DO NOT WANT TO BE REMINDED A SECOND TIME.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": NORBU OUT.");
end

function reminder2_listener(from, channel, message)
  comms_channel_unlisten("ALTO CERBERUS", channel);
  comms_channel_transmit("ALTO CERBERUS", channel, ": AHOY!");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THIS IS CAPTAIN NORBU OF THE ALTO CERBERUS.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": YOU HAVE FAILED TO DELIVER THE PACKAGE IN A TIMELY");
  comms_channel_transmit("ALTO CERBERUS", channel, ": MANNER. GOODBYE . . . FOREVER.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": NORBU OUT.");
  x,y,z = get_object_location(player_ids[1]);
  name = {};
  name[1] = "DARK CLOUD";
  name[2] = "RAVENOUS";
  name[3] = "FIRE AND FLAME";
  name[4] = "MUERTO";
  name[5] = "DAISY";
  name[6] = "SCIMITAR";
  name[7] = "RAZOR";
  name[8] = "SWITCHBLADE";
  name[9] = "RAPTOR";
  name[10] = "FIREHAWK";
  for i = 1, 10 do
    attacker = add_ship(name[i], x + 800 + i * 150, y + 700 + i * 150, z + 50 + i * 50, 18, 3, 0);
    ai_push_attack(attacker, player_ids[1]);
  end
end


function cargo_acquired_listener(from, channel, message)
  comms_channel_unlisten("ALTO CERBERUS", channel);
  comms_channel_transmit("ALTO CERBERUS", channel, ": AHOY!");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THIS IS CAPTAIN NORBU OF THE ALTO CERBERUS.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": WE HAVE BEEN OBSERVING YOU FROM AFAR AND WE SEE");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THAT YOU HAVE ACQUIRED THE CARGO. EXCELLENT! NOW");
  comms_channel_transmit("ALTO CERBERUS", channel, ": DELIVER IT TO THE STATION IN ORBIT AROUND ASTU, AND.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": DO IT QUICKLY!");
end

function package_delivered_listener(from, channel, message)
  comms_channel_unlisten("ALTO CERBERUS", channel);
  comms_channel_transmit("ALTO CERBERUS", channel, ": AHOY!");
  comms_channel_transmit("ALTO CERBERUS", channel, ": THIS IS CAPTAIN NORBU OF THE ALTO CERBERUS.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": CONGRATULATIONS, YOU HAVE DELIVERED THE PACKAGE AS");
  comms_channel_transmit("ALTO CERBERUS", channel, ": REQUIRED. YOUR CONSIDERABLE DEBT IS FORGIVEN.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": NOW GET THE HELL OUT OF HERE.");
  comms_channel_transmit("ALTO CERBERUS", channel, ": NORBU OUT.");
end

function first_attack(cookie)
  print("first attack called\n");
  x,y,z = get_object_location(player_ids[1]);
  x = x + 1000;
  y = y + 1000;
  z = z + 1000;
  move_object(attacker, x, y, z);
  ai_push_attack(attacker, player_ids[1]);
  print("Attacker added at ", x, y, z, ".\n");
end

function hail_player(response_function)
  comms_channel_transmit("ALTO CERBERUS", 0, ": *** HAILING ON CHANNEL " .. first_channel .. " ***");
  comms_channel_listen("ALTO CERBERUS", first_channel, response_function);
end

function first_message(cookie)
  hail_player("first_listener");
end

function first_reminder(cookie)
  if cargo_acquired == 0 then
    hail_player("reminder1_listener");
  end
end

function second_reminder(cookie)
  if cargo_acquired == 0 then
    hail_player("reminder2_listener");
  end
end

function cargo_acquired_message(cookie)
  hail_player("cargo_acquired_listener");
end

function cargo_container_acquired(shipid, commodity_index, commodity_quantity)
  if commodity_index == macguffin then
    cargo_acquired = 1
    register_timer_callback("cargo_acquired_message", 100, 0);
  end
end

function package_delivered_message(cookie)
  hail_player("package_delivered_listener");
end

function player_docked(player, station)
  print("Player docked lua, cargo_acquired = ", cargo_acquired);
  if cargo_acquired == 1 then
    if station == astu_starbase then
      for i = 0,7 do
        attr = string.format("cargo[%01d].contents.item", i);
        item = get_ship_attribute(player_ids[1], attr);
	print("i, attr, item, macguffin = ", i, attr, item, macguffin);
        if item == macguffin then
          print("Packaged delivered!");
          package_delivered = 1;
          set_commodity_contents(player_ids[1], -1, 0, i); -- Empty the cargo bay
          register_timer_callback("package_delivered_message", congratulations_time , 0);
        end
      end
    end
  end
end

register_timer_callback("first_attack", first_attack_time, 0);
register_timer_callback("first_message", first_message_time, 0);
register_timer_callback("first_reminder", first_reminder_time, 0);
register_timer_callback("second_reminder", second_reminder_time, 0);
register_callback("cargo-container-acquired-event", "cargo_container_acquired");
register_callback("player-docked-event", "player_docked");

show_timed_text(player_ids[1], 60, "KALI\n" ..
	". . . HAVING RECENTLY GAMBLED AWAY A TERRIFYING\n" ..
	"SUM OF MONEY, OUR HEROES FIND THEMSELVES UNABLE\n" ..
	"TO PAY THEIR STAGGERING DEBT AND ARE FLEEING\n" ..
	"FROM KNOWN PSYCHO, VARKON KALI AND HIS GANG OF\n" ..
	"THUGS . . .\n");

