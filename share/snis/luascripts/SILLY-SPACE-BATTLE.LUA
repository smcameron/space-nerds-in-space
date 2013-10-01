
-- some constants...

neutral =  0;
wallunni = 1;
gordouni = 2;
zarkon = 3;
vekkazi = 4;

CRUISER = 0;
DESTROYER = 1;
FREIGHTER = 2;
TANKER = 3;
TRANSPORT = 4;
BATTLESTAR = 5;
STARSHIP = 6;
ASTEROIDMINER = 7;
SCIENCE = 8;
SCOUT = 9;
DRAGONHAWK = 10;
SKORPIO = 11;
DISRUPTOR = 12;
RESEARCH_VESSEL = 13;

-- clear everything out of the universe except player ships
clear_all();

-- add a random ship to the universe in a random location
random_ship_id = add_random_ship();
print("random ship id = ");
print(random_ship_id);

-- add a specific ship to the universe in a specific location
specific_ship_id = add_ship("REVENGE", 0, 0, 0, DRAGONHAWK, vekkazi);
print("specific ship id = ", specific_ship_id);

-- add a starbases:
starbase_id = add_starbase(10000, 10000, 0, 76);
print("starbase id", starbase_id);

-- add a planet:
planet_id = add_planet("NOWWHAT", 50000, 500000, 0);
print("planet id = ", planet_id);

-- add a nebula:
nebula_id = add_nebula("IMPENETRABLE", 100000, 70000, 0, 10000);
print("nebula id = ", nebula_id);

-- add a spacemonster:
spacemonster_id = add_spacemonster("GEORGE", 6000, 40000, 0);
print("spacemonster id = ", spacemonster_id);

-- add a derelict:
derelict_id = add_derelict("MARIE CELESTE", 9000, 90000, 0, DISRUPTOR, wallunni);
print("derelict id = ", derelict_id);

-- add a wormhole pair
wh1, wh2 = add_wormhole_pair(50000, 0, 0, 50000, 100000, 0);
print("wh1 = ", wh1, " wh2 = ", wh2);

player_ids = get_player_ship_ids();
for i, v in pairs(player_ids) do
	print(i, v);
	x, y, z = get_object_location(v);
	if (x) then
		print("object ", v, " location is ", x, ", ", y, ", ", z);
	else
		print("object ", v, " no longer exists.");
	end
end

if (player_ids[1]) then
	load_skybox(player_ids[1], "orange-haze");
	move_object(player_ids[1], 70000, 70000, 0);
end
if (player_ids[2]) then
	move_object(player_ids[2], 25000, 25000, 0);
end
attack_ship(specific_ship_id, player_ids[1]);

for i = -3, 3 do
	for j = -3, 3 do
		id1 = add_ship("a", 80000 + j * 100, 68000 - i * 100,
				j * 100 + math.random() * 25.0, 13 * math.random(), vekkazi);
		id2 = add_ship("a", 60000 + j * 100, 68000 - i * 100,
				j * 100 + math.random() * 25.0, 13 * math.random(), zarkon);
		attack_ship(id1, id2);
		attack_ship(id2, id1);
	end
end

sbase_id = add_starbase(72500, 68500, 0, 76);
p1 = add_planet("NOWWHAT", 72000, 50000, 100);
p2 = add_planet("NOWWHAT", 68000, 55000, -100);

