#define MODULE_LOG_PREFIX "webif"
#include "../../globals.h"
#include "../internal/proto.h"
#include "../assets/webif_assets.h"

#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
#pragma GCC diagnostic ignored "-Woverlength-strings"

void send_login_page(int fd, int failed)
{
	int   bsz = 8192, pos = 0;
	char *buf = (char *)malloc(bsz);
	if (!buf) return;

	pos = buf_printf(&buf, &bsz, pos,
		"<!DOCTYPE html><html lang='en'><head>"
		"<meta charset='UTF-8'>"
		"<meta name='viewport' content='width=device-width,initial-scale=1'>"
		"<title>TCMG &mdash; Login</title>"
		"<style>%s</style>"
		"</head><body>",
		TCMG_CSS);

	pos = buf_printf(&buf, &bsz, pos,
		"<div class='lb'><div class='lcard'>"
		"<div class='ll'>"
		"  <div class='lli'>"
		"    <svg width='26' height='26' viewBox='0 0 24 24' fill='none'>"
		"      <path d='M12 2L2 7l10 5 10-5-10-5z' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>"
		"      <path d='M2 17l10 5 10-5' stroke='var(--p)' stroke-width='1.8' stroke-linejoin='round'/>"
		"      <path d='M2 12l10 5 10-5' stroke='var(--cy)' stroke-width='1.8' stroke-linejoin='round'/>"
		"    </svg>"
		"  </div>"
		"  <div>"
		"    <div class='llt'>TCMG</div>"
		"    <div class='llv'>" TCMG_VERSION " &bull; Web Interface</div>"
		"  </div>"
		"</div>");

	if (failed)
		pos = buf_printf(&buf, &bsz, pos,
			"<div class='le'>"
			"<svg width='14' height='14' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
			"<circle cx='12' cy='12' r='10'/>"
			"<line x1='12' y1='8' x2='12' y2='12'/>"
			"<line x1='12' y1='16' x2='12.01' y2='16'/>"
			"</svg>"
			"Invalid credentials &mdash; please try again."
			"</div>");

	pos = buf_printf(&buf, &bsz, pos,
		"<form method='POST' action='/login'>"
		"<div class='fg'>"
		"  <label class='fld'>USERNAME</label>"
		"  <input class='fi' type='text' name='u' placeholder='Enter username' autofocus autocomplete='username'>"
		"</div>"
		"<div class='fg'>"
		"  <label class='fld'>PASSWORD</label>"
		"  <input class='fi' type='password' name='p' placeholder='Enter password' autocomplete='current-password'>"
		"</div>"
		"<button type='submit' class='btn bp' style='width:100%%;justify-content:center;padding:11px;margin-top:4px'>"
		"<svg width='15' height='15' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'>"
		"<path d='M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4'/>"
		"<polyline points='10 17 15 12 10 7'/><line x1='15' y1='12' x2='3' y2='12'/>"
		"</svg>"
		"Sign In</button>"
		"</form>"
		"</div></div>"
		"</body></html>");

	send_response(fd, 200, "OK", "text/html", buf, pos);
	free(buf);
}

