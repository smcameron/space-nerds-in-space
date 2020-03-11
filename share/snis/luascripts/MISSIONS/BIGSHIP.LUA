-- create a big ship near the player
door_open = 0.0;
door_vel = 0.05;
e = -1;
door1 = -1;
door2 = -1;
scale = 10.0;

function big_ship(x, y, z, s)
	t = 0;
	block = 0;
	sphere = 1;
	capsule = 2;

	-- Center tiny block */
	e = add_block(-1, x, y, z, 1, 1, 1, 1, 0, 0, 0, t, block);
	set_object_rotational_velocity(e, 1, 0, 0, 0);  -- Stop default spinning

	-- Bottom
	add_block(e, s * 0, s * 0, s * -10, s * 100, s * 40, s * 5, 1, 0, 0, 0, t, block);

	-- Top doors
	door1 = add_block(e, s * (8 + 15 * door_open), s * 0, s * 9.7, s * 17, s * 40, s * 5, 1, 0, 0, 0, t, block);
	door2 = add_block(e, s * (-8 - 15 * door_open), s * 0, s * 9.7, s * 17, s * 40, s * 5, 1, 0, 0, 0, t, block);

	-- top walls
	add_block(e, s * 33, s * 0, s * 10, s * 33, s * 40, s * 5, t, block);
	add_block(e, s * -33, s * 0, s * 10, s * 33, s * 40, s * 5, t, block);

	add_block(e, s * 0, s * -30, s * -6, s * 100, s * 40, s * 5, 1, 0, 0, -17.0 * math.pi / 180.0, t, block);
	add_block(e, s * 0, s * -30, s * 6, s * 100, s * 40, s * 5, 1, 0, 0, 17.0 * math.pi / 180.0, t, block);

	add_block(e, s * 0, s * 30, s * -6, s * 100, s * 40, s * 5, 1, 0, 0, 17 * math.pi / 180.0, t, block);
	add_block(e, s * 0, s * 30, s * 6, s * 100, s * 40, s * 5, 1, 0, 0, -17 * math.pi / 180.0, t, block);

	-- Four corner capsules
	add_block(e, s * 50, s * 20, s * 0, s * 30, 0.5 * s * 15, 0.5 * s * 15, 1, 0, 0, 0, t, capsule);
	add_block(e, s * -50, s * 20, s * 0, s * 30, 0.5 * s * 15, 0.5 * s * 15, 1, 0, 0, 0, t, capsule);
	add_block(e, s * 50, s * -20, s * 0, s * 30, 0.5 * s * 15, 0.5 * s * 15, 1, 0, 0, 0, t, capsule);
	add_block(e, s * -50, s * -20, s * 0, s * 30, 0.5 * s * 15, 0.5 * s * 15, 1, 0, 0, 0, t, capsule);

	-- back wall
	add_block(e, s * -45, s * 0, s * 0, s * 20, s * 35, s * 15, 0, 1, 0, math.pi, t, block);
	-- front wall
	add_block(e, s * 45, s * 0, s * 0, s * 20, s * 35, s * 15, 0, 1, 0, math.pi, t, block);

	-- Three front capsules
	add_block(e, s * 35, s * 30, s * 0, s * 180, 0.5 * s * 20, 0.5 * s * 20, 1, 0, 0, 0.25 * math.pi, t, capsule);
	add_block(e, s * 35, s * -30, s * 0, s * 180, 0.5 * s * 20, 0.5 * s * 20, 1, 0, 0, 0.25 * math.pi, t, capsule);
	add_block(e, s * 0, s * -50, s * 0, s * 120, 0.5 * s * 10, 0.5 * s * 10, 1, 0, 0, 0.25 * math.pi, t, capsule);

	add_block(e, s * 0, s * 50, s * 0, s * 120, 0.5 * s * 10, 0.5 * s * 10, 1, 0, 0, 0.25 * math.pi, t, capsule);
	add_block(e, s * 110, s * 0, s * 0, s * 20, s * 70, s * 5, 1, 0, 0, 0, t, block);

	add_block(e, s * 120, s * 0, s * 0, s * 80, 0.5 * s * 30, 0.5 * s * 30, 1, 0, 0, 0, t, capsule);

	add_block(e, s * -35, s * -30, s * 10, s * 40, s * 3, s * 20, 1, 0, 0, 20.0 * math.pi / 180.0, t, block);
	add_block(e, s * -45, s * -30, s * 10, s * 20, s * 2, s * 40, 1, 0, 0, 20.0 * math.pi / 180.0, t, block);
	add_block(e, s * -35, s * 30, s * 10, s * 40, s * 3, s * 20, 1, 0, 0, -20.0 * math.pi / 180.0, t, block);
	add_block(e, s * -45, s * 30, s * 10, s * 20, s * 2, s * 40, 1, 0, 0, -20.0 * math.pi / 180.0, t, block);
end

-- clear_all(nil);
player_ids = get_player_ship_ids();
if (player_ids[1]) then
	x,y,z = get_object_location(player_ids[1]);
	big_ship(x + 2000, y, z, scale);
end

function move_door()
	door_open = door_open + door_vel;
	if door_open > 1.0 then
		door_vel = -0.01;
	end
	if door_open < 0.0 then
		door_vel = 0.01;
	end
	set_object_relative_position(door1, scale * (8 + 15 * door_open), scale * 0, scale * 9.7);
	set_object_relative_position(door2, scale * (-8 - 15 * door_open), scale * 0, scale * 9.8);
	register_timer_callback("move_door", 1, 0);
end

move_door();

