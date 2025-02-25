#ifndef SNIS_CLIENT_FORKER_H__
#define SNIS_CLIENT_FORKER_H__

struct snis_process_options {
	struct ssgl_server_options {
		int x;
	} ssgl_server;
	struct snis_multiverse_options {
		int autowrangle;
		int allow_remote_networks;
	} snis_multiverse;
	struct snis_server_options {
		int x;
	} snis_server;
	struct snis_client_options {
		int x;
	} snis_client;
};

#define FORKER_START_SSGL '1'
#define FORKER_START_MULTIVERSE '2'
#define FORKER_START_SNIS_SERVER '3'
#define FORKER_QUIT '4'
#define FORKER_KILL_EM_ALL '5' /* terminate all snis processes */
#define FORKER_ADVANCED '6' /* start snis_launcher in a terminal */
#define FORKER_UPDATE_ASSETS '7' /* check for updated art assets */
#define FORKER_KILL_SERVERS '8' /* terminate all snis processes */
#define FORKER_RESTART_SNIS_CLIENT 'R'
#define FORKER_SET_AUTOWRANGLE 'A'
#define FORKER_SET_ALLOW_REMOTE_NETWORKS 'N'

void fork_update_assets(int background_task, int local_only);
void forker_process_start(int *pipe_to_forker_process, char **saved_argv);

#endif
