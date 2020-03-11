-- collision detection with sphere, block, & capsule
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	x,y,z = get_object_location(player_ids[1]);
	a = add_block(-1, x + 2000, y, z, 400, 400, 400, 0, 0, 0, 0, 0, 1);
	b = add_block(-1, x + 2000, y + 800, z, 400, 400, 400, 0, 0, 0, 0, 0, 0);
	c = add_block(-1, x + 2000, y + 1600, z, 400, 400, 80, 0, 0, 0, 0, 0, 1);
	d = add_block(-1, x + 2000, y + 2400, z, 400, 400, 80, 0, 0, 0, 0, 0, 0);
	e = add_block(-1, x + 2000, y + 3200, z, 400, 100, 100, 0, 0, 0, 0, 0, 2);
	set_object_rotational_velocity(a, 0, 1, 0, 0.1 * 3.1415927 / 180.0);
	set_object_rotational_velocity(b, 0, 1, 0, 0.1 * 3.1415927 / 180.0);
	set_object_rotational_velocity(c, 0, 1, 0, 0.1 * 3.1415927 / 180.0);
	set_object_rotational_velocity(d, 0, 1, 0, 0.1 * 3.1415927 / 180.0);
	set_object_rotational_velocity(e, 0, 1, 0, 0.1 * 3.1415927 / 180.0);
end

