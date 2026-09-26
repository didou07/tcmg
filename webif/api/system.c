#define MODULE_LOG_PREFIX "webif"
#include "../../src/core/runtime_state.h"
#include "../../src/log/log.h"
#include "../internal/proto.h"

void handle_api_reload(int fd)
{
	g_reload_cfg = 1;
	send_json_ok(fd, "reload scheduled");
}

void handle_api_restart(int fd)
{
	tcmg_log("%s", "restart requested via API");
	g_restart = 1;
	g_running = 0;
	send_json_ok(fd, "restart initiated");
}

void handle_api_resetstats(int fd)
{
	handle_reset_stats();
	send_json_ok(fd, "stats reset");
}
