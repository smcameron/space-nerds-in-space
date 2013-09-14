
print("initialize.lua ran");

function player_death_callback(object_id)
  print("player_death_callback, object_id = ", object_id);
end

function player_respawn_callback(object_id)
  print("player_respawn_callback, object_id = ", object_id);
end

function player_docked_callback(player_id, station_id)
  print("player-docked-callback: player = ", player_id, ", station_id = ", station_id);
end


--function once_every_ten_seconds(cookie)
  -- print("once_every_ten_seconds called, cookie = ", cookie);
  -- register_timer_callback("once_every_ten_seconds", 100, cookie + 1);
-- end 

register_callback("player-death-event", "player_death_callback");
register_callback("player-respawn-event", "player_respawn_callback");
register_callback("player-docked-event", "player_docked_callback");
-- register_timer_callback("once_every_ten_seconds", 100, 0);

