
clear_all();

bhx = 300000;
bhy = 0;
bhy = 0;
spacing = 50000;
r = 3000;

bh1 = add_black_hole("bh1", bhx, bhy, bhz, r);
bh2 = add_black_hole("bh2", bhx, bhy + spacing, bhz, r);
bh3 = add_black_hole("bh3", bhx, bhy - spacing, bhz, r);

player_ids = get_player_ship_ids();
for i, v in pairs(player_ids) do
	print(i, v);
	x, y, z = get_object_location(v);
	if (x) then
		print("object ", v, " location is ", x, ", ", y, ", ", z);
		move_object(v, bhx, bhy - spacing * 2, bhz);
	else
		print("object ", v, " no longer exists.");
	end
end

