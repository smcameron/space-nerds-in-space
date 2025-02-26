#ifndef SNIS_PROCESS_OPTIONS_H__
#define SNIS_PROCESS_OPTIONS_H__

struct snis_process_options {
	struct ssgl_server_options {
		unsigned short port;
		int primary_ip_probe_addr;
	} ssgl_server;
	struct snis_multiverse_options {
		char autowrangle;
		char allow_remote_networks;
		char nickname[15];
		char location[20];
		char exempt[20];
		char has_fixed_port_number;
		char port_number[15];
		char ss_port_range[15];
	} snis_multiverse;
	struct snis_server_options {
		char allow_remote_networks;
		char enable_enscript;
		char nickname[15];
		char location[20];
		char multiverse_location[20];
		char solarsystem[20];
		char has_port_range;
		char port_range[15];
	} snis_server;
	char lobbyhost[256];
	char no_lobby;
};

struct snis_process_options snis_process_options_default(void);

#endif
