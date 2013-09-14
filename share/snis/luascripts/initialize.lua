
print("initialize.lua ran");

function player_death_callback(object_id)
  print("player_death_callback, object_id = ", object_id);
end

function player_respawn_callback(object_id)
  print("player_respawn_callback, object_id = ", object_id);
end

register_callback("player-death-event", "player_death_callback");
register_callback("player-respawn-event", "player_respawn_callback");

