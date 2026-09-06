
player_ids = get_player_ship_ids();

sb1 = lookup_by_name("SB-01");
sb2 = lookup_by_name("SB-02");

create_passenger(0, "PROF. PLUM", player_ids[1], sb1, 600);
create_passenger(1, "Ms. Scarlett", player_ids[1], sb2, 600);

set_passenger_destination(1, "EARTH", "NEW YORK CITY", 9000);

demon_print("TESTPASSENGERS: created 2 passengers.");


