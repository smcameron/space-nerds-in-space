
-- some constants...

neutral =  0;
wallunni = 1;
schaazbaut = 2;
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
specific_ship_id = add_ship("REVENGE", 0, 0, 0, DRAGONHAWK, vekkazi, 0);
print("specific ship id = ", specific_ship_id);

-- add a starbases:
starbase_id = add_starbase(10000, 0, 10000, 76);
print("starbase id", starbase_id);

-- add a planet:
planet_id = add_planet("NOWWHAT", 50000, 0, 500000, 5000);
print("planet id = ", planet_id);

-- add a nebula:
nebula_id = add_nebula("IMPENETRABLE", 100000, 0, 70000, 10000);
nebula_id = add_nebula("IMPENETRABLE", 70000, 0, 40000, 13000);
print("nebula id = ", nebula_id);

-- add a spacemonster:
spacemonster_id = add_spacemonster("GEORGE", 6000, 0, 40000);
print("spacemonster id = ", spacemonster_id);

-- add a derelict:
derelict_id = add_derelict("MARIE CELESTE", 9000, 0, 90000, DISRUPTOR, wallunni);
print("derelict id = ", derelict_id);

-- add a wormhole pair
wh1, wh2 = add_wormhole_pair(50000, 0, 0, 50000, 0, 100000);
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

px = 70000;
py = 0;
pz = 70000;

function add_asteroids()
	for i = 1, 200 do
		dx = px + math.random(20000) - 15000;
		dy = py + math.random(20000) - 10000;
		dz = pz + math.random(20000) - 10000;
		add_asteroid(dx, dy, dz);
	end
end

add_asteroids();

if (player_ids[1]) then
	-- load_skybox(player_ids[1], "orange-haze");
	move_object(player_ids[1], px + 12000, py, pz + 12000);
end
if (player_ids[2]) then
	move_object(player_ids[2], 25000, 0, 25000);
end
ai_push_attack(specific_ship_id, player_ids[1]);

for i = -3, 3 do
	for j = -3, 3 do
		id1 = add_ship("a", px + 10000 + j * 100,
				py - 500 + j * 100 + math.random() * 25.0,
				pz - 8000 - i * 100, 13 * math.random(), vekkazi, 0);
		id2 = add_ship("a", px - 10000 + j * 100,
				py + 500 + j * 100 + math.random() * 25.0,
				pz + 8000 - i * 100, 13 * math.random(), zarkon, 0);
		ai_push_attack(id1, id2);
		ai_push_attack(id2, id1);
	end
end

sbase_id = add_starbase(px + 2500, py, pz - 2500, 76);
--p1 = add_planet("NOWWHAT", px - 19000, 300, pz - 50000, 4000);
p2 = add_planet("NOWWHAT", px - 6000, -700, pz - 50000, 8000);

show_timed_text(-1, 10, "SILLY SPACE BATTLE\n" .. "THIS IS A SILLY SPACE BATTLE\n" .. "ENJOY THE SILLY SPACE BATTLE\n");

