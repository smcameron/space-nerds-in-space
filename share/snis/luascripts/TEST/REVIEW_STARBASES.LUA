-- allow starbase models to be inspected in game

clear_all();

x = 300000;
y = -300000;
z = 300000;
xinc = 1000;
zinc = 1000;
num_starbase_types = 6;
num_rows = 5;

player_ids = get_player_ship_ids();
if (player_ids[1] ~= nil) then
	move_object(player_ids[1], x, y, z);
end

x = x + 1000;
y = y - 500;

counts = {};
for m = 0, num_starbase_types - 1 do
	counts[m] = 0;
end

done = 0;
n = 1;
attempts = 0;
while (done < num_starbase_types and attempts < 500) do
	attempts = attempts + 1;
	sb = add_starbase(x, y, z, n);
	if (sb == nil) then
		break;
	end
	m = sb % num_starbase_types;
	if (counts[m] < num_rows) then
		row = counts[m];
		move_object(sb, x + row * xinc, y, z + m * zinc);
		set_object_orientation(sb, 1, 0, 0, 0); -- unrotated
		counts[m] = counts[m] + 1;
		if (counts[m] == num_rows) then
			done = done + 1;
		end
		n = (n % 99) + 1;
	else
		delete_object(sb);
	end
end
