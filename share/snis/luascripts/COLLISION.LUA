
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




x1 = 10000
y1 = 0
z1 = 10000

x2 = 13000
y2 = 0
z2 = 10000

x3 = 13000
y3 = 0
z3 = 13000

player_ids = get_player_ship_ids();
if (player_ids[1]) then
	load_skybox(player_ids[1], "orange-haze");
	move_object(player_ids[1], (x1 + x2 + x3) / 3, (y1 + y2 + y3) / 3, (z1 + z2 + z3) / 3);
end

for i = 0, 10 do
	id1 = add_ship("a", x1 + i * 100, y1 + i * 100, z1, DISRUPTOR, vekkazi);
	if (i % 2) == 0 then
		ai_push_patrol(id1, 3, x2, y2, z2, x3, y3, z3, x1, y1, z1, nil);
	else
		ai_push_patrol(id1, 3, x1, y1, z1, x3, y3, z3, x2, y2, z2, nil);
	end
end

sbase_id = add_starbase(72500, 0, 68500, 76);
p1 = add_planet("NOWWHAT", 72000, 100, 50000);
p2 = add_planet("NOWWHAT", 68000, -100, 55000);

sb2 = add_starbase(x1, y1, z1, 22);
sb2 = add_starbase(x2, y2, z2, 23);
sb2 = add_starbase(x3, y3, z3, 24);

