-- regenerate the universe

arg = {...};

if #arg < 2 then
    random_seed = -1;
else
    random_seed = tonumber(arg[2]);
end

regenerate_universe(random_seed);

