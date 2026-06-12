#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

void send_page_power(int fd, const char *qs)
{
	char action[16], confirm[8];
	get_param(qs, "action",  action,  sizeof(action));
	get_param(qs, "confirm", confirm, sizeof(confirm));

	PAGE_INIT(12288)

	pos = emit_header(&buf, &bsz, pos, "Power", "power");

	if (strcmp(confirm, "yes") == 0 && action[0]) {
		int is_restart = (strcmp(action, "restart") == 0);
		tcmg_log("webif: %s requested", is_restart ? "restart" : "shutdown");
		if (is_restart) g_restart = 1;
		g_running = 0;

		pos = buf_printf(&buf, &bsz, pos,
			"<div class='pg-center'>"
			"<div class='done-card'>"
			"<div class='%s' style='margin:0 auto 16px'>"
			"  <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"    %s"
			"  </svg>"
			"</div>"
			"<h2>%s</h2>"
			"<p style='margin-top:8px'>%s</p>"
			"%s"
			"</div></div>",
			is_restart ? "dico info" : "dico danger",
			is_restart
				? "<polyline points='23 4 23 10 17 10'/><path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
				: "<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>",
			is_restart ? "Restarting&hellip;" : "Server Stopped",
			is_restart
				? "Config will be reloaded. Redirecting when back online&hellip;"
				: "TCMG has been shut down. All connections were dropped.",
			is_restart
				? "<div style='display:flex;justify-content:center;margin-top:14px'>"
				  "<div class='spill'><div class='pulse sm'></div>&nbsp;Waiting for server&hellip;</div>"
				  "</div>"
				  "<script>setTimeout(function(){"
				  "  var t=setInterval(function(){"
				  "    fetch('/api/status',{cache:'no-store'})"
				  "      .then(function(){clearInterval(t);location.href='/status';})"
				  "      .catch(function(){});"
				  "  },1500);"
				  "},3500);</script>"
				: "");
	}

	else if (action[0]) {
		int is_restart = (strcmp(action, "restart") == 0);
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='pg-center'>"
			"<div class='dlg'>"
			"<div class='%s' style='margin:0 auto 18px'>"
			"  <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"    %s"
			"  </svg>"
			"</div>"
			"<h2>%s Server?</h2>"
			"<p>%s</p>"
			"<div class='da'>"
			"  <a href='/power?action=%s&confirm=yes' class='%s'>Confirm %s</a>"
			"  <a href='/power' class='btn bg'>Cancel</a>"
			"</div>"
			"</div></div>",
			is_restart ? "dico info" : "dico danger",
			is_restart
				? "<polyline points='23 4 23 10 17 10'/><path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
				: "<path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>",
			is_restart ? "Restart" : "Shutdown",
			is_restart
				? "Active connections will be dropped and configuration reloaded on restart."
				: "All active connections will be dropped immediately. The process will <strong>not</strong> restart.",
			is_restart ? "restart" : "shutdown",
			is_restart ? "btn bp" : "btn bd_",
			is_restart ? "Restart" : "Shutdown");
	}

	else {
		pos = buf_printf(&buf, &bsz, pos,
			"<div style='display:flex;gap:16px;flex-wrap:wrap;justify-content:center;max-width:600px;margin:0 auto'>"

			"<div class='card' style='flex:1;min-width:220px;text-align:center;padding:28px 20px'>"
			"  <div class='dico info' style='margin:0 auto 16px'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"      <polyline points='23 4 23 10 17 10'/>"
			"      <path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"    </svg>"
			"  </div>"
			"  <h3 style='font-size:16px;font-weight:700;margin-bottom:8px'>Restart</h3>"
			"  <p style='font-size:12px;color:var(--t1);margin-bottom:18px;line-height:1.6'>"
			"    Drops connections and reloads configuration."
			"  </p>"
			"  <a href='/power?action=restart' class='btn bp' style='width:100%%;justify-content:center'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8' width='14' height='14'>"
			"      <polyline points='23 4 23 10 17 10'/>"
			"      <path d='M20.49 15a9 9 0 1 1-2.12-9.36L23 10'/>"
			"    </svg>"
			"    Restart Server"
			"  </a>"
			"</div>"

			"<div class='card' style='flex:1;min-width:220px;text-align:center;padding:28px 20px'>"
			"  <div class='dico danger' style='margin:0 auto 16px'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8'>"
			"      <path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"    </svg>"
			"  </div>"
			"  <h3 style='font-size:16px;font-weight:700;margin-bottom:8px'>Shutdown</h3>"
			"  <p style='font-size:12px;color:var(--t1);margin-bottom:18px;line-height:1.6'>"
			"    Stops all connections. Process will not restart."
			"  </p>"
			"  <a href='/power?action=shutdown' class='btn bd_' style='width:100%%;justify-content:center'>"
			"    <svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='1.8' width='14' height='14'>"
			"      <path d='M18.36 6.64a9 9 0 1 1-12.73 0'/><line x1='12' y1='2' x2='12' y2='12'/>"
			"    </svg>"
			"    Shutdown Server"
			"  </a>"
			"</div>"

			"</div>");
	}

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}

void send_page_shutdown(int fd, const char *qs) { (void)qs; send_redirect(fd, "/power?action=shutdown"); }
void send_page_restart (int fd, const char *qs) { (void)qs; send_redirect(fd, "/power?action=restart"); }
