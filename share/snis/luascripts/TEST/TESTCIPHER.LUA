-- TEST COMMS CIPHER STUFF

key = generate_cipher_key();

n = add_ship("QUACKERS", 10000, 10000, 10000, 10, 0, 0);
comms_enciphered_transmission(n, "When any government, or any church for that matter, undertakes to say to its subjects, This you may not read, this you must not see, this you are forbidden to know, the end result is tyranny and oppression no matter how holy the motives.", key);

