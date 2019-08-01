package.path = package.path .. ";../MISSIONS/lib/?.lua";
local intfic_package = require("interactive_fiction");

intfic.gameloop();

