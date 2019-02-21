-- pay the specified player the specified amount of money

arg = {...};
player_ids = get_player_ship_ids();

if #arg < 3 then
    demon_print("USAGE: PAY_PLAYER SHIP-ID MONEY");
    return;
end

id = tonumber(arg[2]);
money = tonumber(arg[3]);

if id == nil then
    demon_print("PAY_PLAYER: BAD PLAYER ID");
end

if money == nil then
    demon_print("PAY_PLAYER: BAD MONEY VALUE");
end

found = 0 ;
for j, v in pairs(player_ids) do
    if (v == id) then
        demon_print("UPDATED PLAYER WALLET");
        update_player_wallet(id, money);
        found = 1;
	break;
    end
end

if (found == 0) then
   demon_print("FAILED TO FIND PLAYER ID ");
end

