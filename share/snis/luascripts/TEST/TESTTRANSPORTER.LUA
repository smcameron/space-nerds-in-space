-- Test script for Transporter functionality.
-- Places the player ship in open space with 8 stationary cargo containers
-- arranged around it within transporter range (2500 units).

clear_all();

local player_ids = get_player_ship_ids();
if not player_ids[1] then
	demon_print("TESTTRANSPORTER: no player ship found.");
	return;
end

local px, py, pz = 100000, 0, 100000;
local num_containers = 8;
local radius = 800; -- comfortably within TRANSPORTER_RANGE (2500)

move_object(player_ids[1], px, py, pz);
set_object_velocity(player_ids[1], 0, 0, 0);
set_object_orientation(player_ids[1], 0, 1, 0, 0);

demon_print(string.format("TESTTRANSPORTER: placed player ship at (%.0f, %.0f, %.0f)", px, py, pz));

for i = 1, num_containers do
	local angle = (i - 1) * (2 * math.pi / num_containers);
	local cx = px + radius * math.cos(angle);
	local cy = py;
	local cz = pz + radius * math.sin(angle);
	local id = add_cargo_container(cx, cy, cz, 0, 0, 0);
	if id then
		mark_cargo_container_not_for_npc_ships(id, 1);
		local tag = get_cargo_container_transporter_tag(id) or "?????";
		demon_print(string.format("  container %d: id=%d tag=%s pos=(%.0f, %.0f, %.0f)",
			i, id, tag, cx, cy, cz));
	else
		demon_print(string.format("  container %d: failed to create", i));
	end
end
