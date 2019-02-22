
arg = { ... }

shipid = tonumber(arg[2]);
status = tonumber(arg[3]);

print("shipid = ", shipid, " status = ", status);

set_red_alert_status(shipid, status);

