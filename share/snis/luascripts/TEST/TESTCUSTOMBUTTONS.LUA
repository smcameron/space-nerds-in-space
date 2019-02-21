
player_ids = get_player_ship_ids();

set_custom_button_label(player_ids[1], 0, "NAV CUSTOM");
set_custom_button_label(player_ids[1], 1, "WEAP CUSTOM");
set_custom_button_label(player_ids[1], 2, "ENG CUSTOM");
set_custom_button_label(player_ids[1], 3, "DAMCON CUSTOM");
set_custom_button_label(player_ids[1], 4, "SCI CUSTOM");
set_custom_button_label(player_ids[1], 5, "COMMS CUSTOM");

enable_custom_button(player_ids[1], 0);
enable_custom_button(player_ids[1], 1);
enable_custom_button(player_ids[1], 2);
enable_custom_button(player_ids[1], 3);
enable_custom_button(player_ids[1], 4);
enable_custom_button(player_ids[1], 5);

function custom_button_press_event(player_id, button)
  print("Custom button ", button, " pressed.");
  register_callback("custom-button-press-event", "custom_button_press_event")
  text_to_speech(player_id, "Custom button " .. button .. " pressed");
  if button == 5 then
    text_to_speech(player_id, "disabling custom buttons");
    disable_custom_button(player_ids[1], 0);
    disable_custom_button(player_ids[1], 1);
    disable_custom_button(player_ids[1], 2);
    disable_custom_button(player_ids[1], 3);
    disable_custom_button(player_ids[1], 4);
    disable_custom_button(player_ids[1], 5);
  end
end

register_callback("custom-button-press-event", "custom_button_press_event")
