#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"

void send_page_failban(int fd, const char *qs)
{
	char action[32], clearip[MAXIPLEN];
	get_param(qs, "action", action,  sizeof(action));
	get_param(qs, "ip",     clearip, sizeof(clearip));

	if (strcmp(action, "clear") == 0 && clearip[0]) {
		pthread_mutex_lock(&g_cfg.ban_lock);
		for (S_BAN_ENTRY *b = g_cfg.ban_table[ban_hash_pub(clearip)]; b; b = b->next)
			if (strcmp(b->ip, clearip) == 0) b->until = 0;
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("webif: ban cleared for ip=%s", clearip);
	} else if (strcmp(action, "clearall") == 0) {
		pthread_mutex_lock(&g_cfg.ban_lock);
		for (int _bi = 0; _bi < BAN_BUCKETS; _bi++)
			for (S_BAN_ENTRY *b = g_cfg.ban_table[_bi]; b; b = b->next) b->until = 0;
		pthread_mutex_unlock(&g_cfg.ban_lock);
		tcmg_log("%s", "webif: all bans cleared");
	}

	PAGE_INIT(16384)

	pos = emit_header(&buf, &bsz, pos, "Fail-Ban", "failban");

	int    total_bans = 0, total_fails = 0;
	time_t now        = time(NULL);
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (int _bi = 0; _bi < BAN_BUCKETS; _bi++)
		for (S_BAN_ENTRY *b = g_cfg.ban_table[_bi]; b; b = b->next)
			if (b->until > now) { total_bans++; total_fails += b->fails; }
	pthread_mutex_unlock(&g_cfg.ban_lock);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='ph'>"
		"  <div class='ha'>");
	if (total_bans > 0)
		pos = buf_printf(&buf, &bsz, pos,
			"    <a href='/failban?action=clearall' class='btn bd_ sm'>"
			ICO_TRASH "&nbsp;Clear All</a>");
	pos = buf_printf(&buf, &bsz, pos,
		"    <button class='btn bg sm' onclick='location.reload()'>"
		ICO_RELOAD "&nbsp;Refresh</button>"
		"  </div>"
		"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='sbar'>"
		"<div class='sbar-item'><div class='sbl'>Active Bans</div>"
		"  <div class='sbv%s'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Total Fails</div>"
		"  <div class='sbv%s sm'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Max Fails</div>"
		"  <div class='sbv sm'>%d</div></div>"
		"<div class='sbar-item'><div class='sbl'>Ban Duration</div>"
		"  <div class='sbv sm'>%ds</div></div>"
		"</div>",
		total_bans > 0 ? " tr" : " tg", total_bans,
		total_fails > 0 ? " to" : "", total_fails,
		BAN_MAX_FAILS,
		BAN_SECS);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='tw'><table>"
		"<thead><tr>"
		"<th>IP Address</th><th>Fail Count</th>"
		"<th>Expires At</th><th>Remaining</th><th>Action</th>"
		"</tr></thead><tbody>");

	int shown = 0;
	pthread_mutex_lock(&g_cfg.ban_lock);
	for (int _bi = 0; _bi < BAN_BUCKETS; _bi++)
	for (S_BAN_ENTRY *b = g_cfg.ban_table[_bi]; b; b = b->next) {
		if (b->until <= now) continue;
		char exp[32];
		struct tm tm_s;
		localtime_r(&b->until, &tm_s);
		strftime(exp, sizeof(exp), "%H:%M:%S", &tm_s);
		long secs_left = (long)(b->until - now);
		pos = buf_printf(&buf, &bsz, pos,
			"<tr>"
			"<td class='mono bold'>%s</td>"
			"<td><span class='badge bban'>%d fails</span></td>"
			"<td class='mono tm'>%s</td>"
			"<td class='mono to' id='cd_%s'>%lds</td>"
			"<td><a href='/failban?action=clear&ip=%s' class='btn bg sm'>"
			ICO_UNBAN "&nbsp;Unban</a></td>"
			"</tr>",
			b->ip, b->fails, exp, b->ip, secs_left, b->ip);
		shown++;
	}
	pthread_mutex_unlock(&g_cfg.ban_lock);

	if (!shown)
		pos = buf_printf(&buf, &bsz, pos,
			"<tr class='erow'><td colspan='5'>"
			"<svg width='16' height='16' viewBox='0 0 24 24' fill='none' stroke='var(--gr)' stroke-width='1.8'"
			" style='vertical-align:-3px;margin-right:6px'>"
			"<path d='M22 11.08V12a10 10 0 1 1-5.93-9.14'/>"
			"<polyline points='22 4 12 14.01 9 11.01'/>"
			"</svg><span class='tg'>All clear</span> &mdash; no active bans"
			"</td></tr>");

	pos = buf_printf(&buf, &bsz, pos,
		"</tbody></table></div>"
		"<script>(function(){"
		"  var els=document.querySelectorAll('[id^=\"cd_\"]');"
		"  setInterval(function(){"
		"    els.forEach(function(el){"
		"      var v=parseInt(el.textContent);"
		"      el.textContent=(v>0?v-1:0)+'s';"
		"    });"
		"  },1000);"
		"})();</script>");

	pos = emit_footer(&buf, &bsz, pos);
	PAGE_SEND_AND_FREE(fd);
}

