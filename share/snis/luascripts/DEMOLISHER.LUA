
clear_all();

-- find the player
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    player_name = get_object_name(player_ids[1]);
    x, y, z = get_object_location(player_ids[1]);
end

door1 = -1;
door2 = -1;
door3 = -1;
door4 = -1;

function add_demolisher_ship(x, y, z)

  m = 500;
  l = 20 * m;
  w = 3 * m;
  thick = 0.3 * m;
  degrees = (3.1415927 / 180.0);
  zfight = 10;

  -- This is our main, unparented block of the ship to which all other
  -- blocks are attached:
  id = add_block(-1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0);
  set_object_rotational_velocity(id, 1, 0, 0, 0);  -- Stop default spinning
  set_object_orientation(id, 1, 0, 0, 0);  -- unrotated

  top = add_block(id, 0, 0.3 * w, 0, l * 1.1, thick, w, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(top, 3, 8, 2);
  bottom = add_block(id, 0, -0.3 * w, 0, l, thick, w, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(bottom, 2, 8, 2);

  -- first side
  add_block(id, 0,  0.15 * w, 0.8 * w, l, thick, w, 1, 0, 0,  20 * degrees, 0);
  add_block(id, 0, -0.15 * w, 0.8 * w, l * 1.1, thick, w, 1, 0, 0, -20 * degrees, 0);

  -- second side
  add_block(id, 0,  0.15 * w, -0.8 * w, l, thick, w, 1, 0, 0, -20 * degrees, 0);
  add_block(id, 0, -0.15 * w, -0.8 * w, l * 1.1, thick, w, 1, 0, 0,  20 * degrees, 0);

  wallheight = 0.5 * w;
  walllength = w * 1.0;
  walloffset = w * 1.0;

  -- ribs
  door1 = add_block(id, 0.45 * l + zfight, 0, 0.55 * -walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);
  door2 = add_block(id, 0.45 * l, 0, 0.55 *  walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);
  b = add_block(id, 0.35 * l, 0, 0.75 * -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  b = add_block(id, 0.35 * l, 0, 0.75 *  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);

  b = add_block(id, 0.25 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, 0.25 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, 0.15 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, 0.15 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, 0.05 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, 0.05 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, -0.05 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, -0.05 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, -0.15 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, -0.15 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);
  b = add_block(id, -0.25 * l, 0, -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 5, 1, 2);
  b = add_block(id, -0.25 * l, 0,  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  add_turrets_to_block_face(b, 4, 1, 2);

  b = add_block(id, -0.35 * l, 0, 0.75 * -walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  b = add_block(id, -0.35 * l, 0, 0.75 *  walloffset, thick, wallheight, walllength, 1, 0, 0, 0, 0);
  door3 = add_block(id, -0.45 * l + zfight, 0, 0.55 * -walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);
  door4 = add_block(id, -0.45 * l, 0, 0.55 *  walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);


  move_object(id, x, y, z);

end

add_demolisher_ship(x + 10000, y, z);

door_speed = 0.01;
door_open = 0.0;
door_direction = door_speed;

function set_door_position(door_open)
  set_object_relative_position(door1,  0.45 * l + zfight, 0, 0.55 * -walloffset - door_open * 0.3 * walloffset);
  set_object_relative_position(door2,  0.45 * l, 0, 0.55 *  walloffset + door_open * 0.3 * walloffset);
  set_object_relative_position(door3, -0.45 * l + zfight, 0, 0.55 * -walloffset - door_open * 0.3 * walloffset);
  set_object_relative_position(door4, -0.45 * l, 0, 0.55 *  walloffset + door_open * 0.3 * walloffset);
end

function door_mover(cookie)
  door_open = door_open + door_direction;
  if door_open > 1.0 and door_direction > 0.0 then
    door_direction = -door_speed;
  end
  if door_open < 0.0 and door_direction < 0.0 then
    door_direction = door_speed;
  end
  set_door_position(door_open);
  register_timer_callback("door_mover", 1, 0);
end

register_timer_callback("door_mover", 1, 0);

