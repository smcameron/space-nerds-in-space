
clear_all();

-- find the player
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    player_name = get_object_name(player_ids[1]);
    x, y, z = get_object_location(player_ids[1]);
end

function add_demolisher_ship(x, y, z)

  m = 500;
  l = 20 * m;
  w = 3 * m;
  thick = 0.3 * m;
  degrees = (3.1415927 / 180.0);
  zfight = 10;

  id = add_block(-1, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0);
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
  b = add_block(id, 0.45 * l + zfight, 0, 0.55 * -walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);
  b = add_block(id, 0.45 * l, 0, 0.55 *  walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);
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
  door1 = add_block(id, -0.45 * l + zfight, 0, 0.55 * -walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);
  door2 = add_block(id, -0.45 * l, 0, 0.55 *  walloffset, thick, wallheight, walllength * 1.15, 1, 0, 0, 0, 0);


  move_object(id, x, y, z);

end

add_demolisher_ship(x + 10000, y, z);

