
function player_warp_travel_event(id, x1, y1, z1, x2, y2, z2, warpfactor)
	print("TESTWARPEVENT: Player ", id, " warped from ",
		x1, y1, z1, " to ", x2, y2, z2, " at warp factor ", warpfactor);
	register_callback("player-warp-travel-event", "player_warp_travel_event");
end

register_callback("player-warp-travel-event", "player_warp_travel_event");

