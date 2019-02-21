
clear_all();

x = 300000;
y = -300000;
z = 300000;
zinc = 100;
first_ship = 0
last_ship = 22;

player_ids = get_player_ship_ids();
if (player_ids[1] ~= nil) then
	move_object(player_ids[1], x, y, z);
end

x = x + 500;
y = y - 500;

for i = first_ship, last_ship  do
	ship = add_ship("SHIP NAME", x, y, z + i * zinc, i, 0, 0);
	ai_push_catatonic(ship);
	set_object_orientation(ship, 1, 0, 0, 0); -- unrotated
end

