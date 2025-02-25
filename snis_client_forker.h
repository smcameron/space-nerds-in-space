#ifndef SNIS_CLIENT_FORKER_H__
#define SNIS_CLIENT_FORKER_H__

#define FORKER_START_SSGL '1'
#define FORKER_START_MULTIVERSE '2'
#define FORKER_START_SNIS_SERVER '3'
#define FORKER_QUIT '4'
#define FORKER_KILL_EM_ALL '5' /* terminate all snis processes */
#define FORKER_ADVANCED '6' /* start snis_launcher in a terminal */
#define FORKER_UPDATE_ASSETS '7' /* check for updated art assets */
#define FORKER_KILL_SERVERS '8' /* terminate all snis processes */
#define FORKER_RESTART_SNIS_CLIENT 'R'
#define FORKER_UPDATE_PROCESS_OPTIONS 'U'

void fork_update_assets(int background_task, int local_only);
void forker_process_start(int *pipe_to_forker_process, char **saved_argv);

#endif
