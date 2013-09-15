
-- This is a simple training mission script which requires the crew to 
-- navigate a timed circuit around a rectangle of 4 starbases, docking
-- at each one in turn.
--

-- First, clear the board
clear_all();

-- make 4 starbases in a big rectangle
sb1 = add_starbase(100.0, 100.0, 0, 1);
sb2 = add_starbase(100000.0, 100.0, 0, 2);
sb3 = add_starbase(100000.0, 70000.0, 0, 3);
sb4 = add_starbase(0.0, 70000.0, 0, 4);

-- get the names of the starbases.
sb1_name = get_object_name(sb1);
sb2_name = get_object_name(sb2);
sb3_name = get_object_name(sb3);
sb4_name = get_object_name(sb4);

-- which base are they supposed to go to next?
next_base = sb1
next_location = get_object_name(next_base);
all_bases_visited = 0;

-- find the player and move him where we want him to start.
player_ids = get_player_ship_ids();
player_name = "Unknown Player"
if (player_ids[1]) then
    move_object(player_ids[1], 101000, 71000, 0);
    player_name = get_object_name(player_ids[1]);
end

-- Explain to the player via comms what they are expected to do.
function greetings(cookie)
	comms_transmission(sb1, ": WELCOME TO YOUR TRAINING MISSION, " .. player_name);
	comms_transmission(sb1, ": YOUR FIRST TASK WILL TO BE TO COMPLETE");
	comms_transmission(sb1, ": A NAVIGATION COURSE.  YOU WILL BE TIMED.");
	comms_transmission(sb1, ": THE PROCEDURE IS AS FOLLOWS:");
	comms_transmission(sb1, ": DOCK WITH EACH OF THE FOLLOWING STARBASES");
	comms_transmission(sb1, ": IN ORDER");
	comms_transmission(sb1, ": 1. " .. sb1_name);
	comms_transmission(sb1, ": 2. " .. sb2_name);
	comms_transmission(sb1, ": 3. " .. sb3_name);
	comms_transmission(sb1, ": 4. " .. sb4_name);
	comms_transmission(sb1, ": FINALLY, RETURN TO " .. sb1_name .. " AND DOCK");
	comms_transmission(sb1, ": ");
	comms_transmission(sb1, ": GOOD LUCK...");
end

function why_are_you_back_here(base, base_name)
	comms_transmission(base, ": " .. player_name ..
			", WHY ARE YOU BACK HERE AT " .. base_name .. "?");
	comms_transmission(base, ": PROCEED AT ONCE TO " .. next_location);
end

function wrong_base(base, base_name, next_location)
	comms_transmission(base, ": " .. player_name .. " YOU ARE AT THE WRONG BASE.");
	comms_transmission(base, ": " .. player_name .. " PROCEED AT ONCE TO " .. next_location);
end

function congrats_on_this_leg(base, base_name, next_location)
	comms_transmission(base, "");
	comms_transmission(base, ": CONGRATULATIONS " .. player_name .. ", ");
	comms_transmission(base, ": FOR COMPLETING THIS LEG OF THE COURSE.");
	comms_transmission(base, ": NOW PROCEED TO " .. next_location);
end

function we_have_a_winner(base)
	comms_transmission(base, "");
	comms_transmission(base, ": CONGRATULATIONS, " .. player_name);
	comms_transmission(base, ": YOU HAVE COMPLETED THE RACE COURSE,");
	comms_transmission(base, ": AND THIS TRAINING MISSION.");
end

-- called whenever the player docks at a starbase
function player_docked_callback(player_id, station_id)
	if station_id == sb1 then
		if next_base == sb1 then
			if all_bases_visited == 1 then
				we_have_a_winner(sb1);
			else
				next_base = sb2;
				next_location = get_object_name(next_base);
				congrats_on_this_leg(sb1, sb1_name, next_location);
			end
		else
			why_are_you_back_here(sb1, sb1_name);
		end
	end
	if station_id == sb2 then
		if next_base == sb2 then
			next_base = sb3;
			next_location = get_object_name(next_base);
			congrats_on_this_leg(sb2, sb2_name, next_location);
		else
			if next_base == sb1 then
				wrong_base(sb2, sb2_name, next_location);
			else
				why_are_you_back_here(sb2, sb2_name);
			end
		end
	end
	if station_id == sb3 then
		if next_base == sb3 then
			next_base = sb4;
			next_location = get_object_name(next_base);
			congrats_on_this_leg(sb3, sb3_name, next_location);
		else
			if next_base == sb1 or next_base == sb2 then
				wrong_base(sb3, sb3_name, next_location);
			else
				why_are_you_back_here(sb3, sb3_name);
			end
		end
	end
	if station_id == sb4 then
		if next_base == sb4 then
			all_bases_visited = 1;
			next_base = sb1;
			next_location = get_object_name(next_base);
			congrats_on_this_leg(sb4, sb4_name, next_location);
		else
			if next_base == sb1 or next_base == sb2 or next_base == sb3 then
				wrong_base(sb4, sb4_name, next_location);
			else
				why_are_you_back_here(sb4, sb4_name);
			end
		end
	end
end

register_callback("player-docked-event", "player_docked_callback");

-- after 10 seconds, explain the training mission to the player.
register_timer_callback("greetings", 100, 0);

