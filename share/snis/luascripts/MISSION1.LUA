
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


