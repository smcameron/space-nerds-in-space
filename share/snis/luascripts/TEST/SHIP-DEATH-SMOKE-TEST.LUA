-- SHIP-DEATH-SMOKE-TEST: watch ships die up close.
--
-- Turns the first player ship to face +x, then spawns five ships 250 units ahead of it, ten
-- seconds apart, each of a different type, holds them still, and destroys each two seconds after
-- it appears -- leaving a derelict, as a kill does.  Watch from the main screen.

player_ids = get_player_ship_ids();
smoke_test_n = 0;

function smoke_test_kill(id)
	destroy_ship(id);
end

function smoke_test_spawn(cookie)
	local x, y, z = get_object_location(player_ids[1]);
	local id = add_ship("VICTIM" .. smoke_test_n, x + 250, y,
				z + (smoke_test_n % 3 - 1) * 200, smoke_test_n % 20, 0, 0);

	ai_push_catatonic(id);
	register_timer_callback("smoke_test_kill", 20, id);
	smoke_test_n = smoke_test_n + 1;
	if smoke_test_n < 5 then
		register_timer_callback("smoke_test_spawn", 100, 0);
	end
end

set_object_orientation(player_ids[1], 0, 1, 0, 0);
smoke_test_spawn(0);
