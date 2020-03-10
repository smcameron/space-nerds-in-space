-- navigate a timed circuit
-- This is a simple training mission script which requires the crew to 
-- navigate a timed circuit around a rectangle of 4 starbases, docking
-- at each one in turn.
--

adjx = -300000;
adjz = -300000;

-- First, clear the board
clear_all();

start_time = 0;

-- make 4 starbases in a big rectangle
sb1 = add_starbase(adjx + 1000.0, 10000, adjz + 1000.0, 1);
sb2 = add_starbase(adjx + 150000.0, -2000, adjz + 100.0, 2);
sb3 = add_starbase(adjx + 190000.0, 10000, adjz + 70000.0, 3);
n1 = add_nebula("insanity", adjx + 190000.0, 14000, adjz + 70500, 3000);
n2 = add_nebula("insanity", adjx + 191500.0, 15000, adjz + 73500, 5000);
n3 = add_nebula("insanity", adjx + 193000.0, 18000, adjz + 77500, 9000);
sb4 = add_starbase(adjx + 100.0, -2500, adjz + 90000.0, 4);
p1 = add_planet("PHARII", adjx + 191000, 10000, adjz + 85000, 12000);
wh1, wh2 = add_wormhole_pair(adjx + 150000.0, -2000, adjz + 1100.0, adjx + 190000.0, 10000, adjz + 68000.0);

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
reset_player_ship(player_ids[1]);
player_name = "Unknown Player"
if (player_ids[1]) then
    move_object(player_ids[1], adjx + 25000, 0, adjz + 25000);
    player_name = get_object_name(player_ids[1]);
end

-- Explain to the player via comms what they are expected to do.
function greetings(cookie)

	start_time = os.time();
	print("initial start_time = ", start_time);

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
	comms_transmission(sb1, ": -- IMPORTANT-- TO DOCK: ");
	comms_transmission(sb1, ": 1. LOWER SHIELDS");
	comms_transmission(sb1, ": 2. ENGAGE DOCKING MAGNETS");
	comms_transmission(sb1, ": 3. HAIL STARBASE AND REQUEST PERMISSION TO DOCk");
	comms_transmission(sb1, ": 4. RAM A DOCKING PORT ON THE STATION");
	comms_transmission(sb1, ": ");
	comms_transmission(sb1, ": GOOD LUCK...");
	show_timed_text(player_ids[1], 10,
		"TRAINING MISSION 1\n" ..
		"MAKE A CIRCUIT OF 4 STARBASES\n" ..
		"AS QUICKLY AS POSSIBLE.\n" ..
		"1. SB-01\n" ..
		"2. SB-02\n" ..
		"3. SB-03\n" ..
		"4. SB-04\n" ..
		"FINALLY, BACK TO SB-01\n" ..
		"PROCEED IMMEDIATELY TO " .. sb1_name .. "\n... GO! ...\n");
	text_to_speech(player_ids[1], "Make a circuit of 4 star bases as quickly as possible.");
	text_to_speech(player_ids[1], "Get ready. Get set. Go.");
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
	comms_transmission(base, string.format("%s %5.2f %s",
				": ELAPSED TIME: ", os.time() - start_time, " SECONDS"));
	comms_transmission(base, ": NOW PROCEED TO " .. next_location);
	print("os.time() = ", os.time);
	print("start_time = ", start_time);
	show_timed_text(player_ids[1], 10, "NEXT LEG\nPROCEED TO " .. next_location .. "\n\nGO!!!\n");
	text_to_speech(player_ids[1], "Proceed immediately to the next star base.");
end

function we_have_a_winner(base)
	comms_transmission(base, "");
	comms_transmission(base, ": CONGRATULATIONS, " .. player_name);
	comms_transmission(base, ": YOU HAVE COMPLETED THE RACE COURSE,");
	comms_transmission(base, ": AND THIS TRAINING MISSION.");
	comms_transmission(base, string.format("%s %5.2f %s",
				": TOTAL ELAPSED TIME: ", os.time() - start_time, " SECONDS"));
	show_timed_text(player_ids[1], 10, "RACE COMPLETE\n");
	text_to_speech(player_ids[1], "Congratulations to the captain and crew.");
	text_to_speech(player_ids[1], "You have completed the course.");
	text_to_speech(player_ids[1], string.format("%s %5.1f %s",
				"Total elapsed time ", os.time() - start_time, " seconds"));
end

-- called whenever the player docks at a starbase
function player_docked_callback(player_id, station_id)

	-- only the first player counts
	if player_id ~= player_ids[1] then
		return;
	end

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

