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
		char lobbyhost[256];
		char nickname[15];
		char location[20];
		char exempt[20];
		char has_port_range;
		unsigned short minport;
		unsigned short maxport;
	} snis_multiverse;
	struct snis_server_options {
		char allow_remote_networks;
		char enable_enscript;
		char lobbyhost[256];
		char nickname[15];
		char location[20];
		char multiverse_location[20];
		char solarsystem[20];
		char has_port_range;
		unsigned short minport;
		unsigned short maxport;
		char nolobby;
	} snis_server;
};

struct snis_process_options snis_process_options_default(void);

#endif
