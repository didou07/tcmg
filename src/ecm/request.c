#define MODULE_LOG_PREFIX "ecm"
#include "request.h"
#include "core/utils.h"
#include <string.h>

void ecm_request_init(S_ECM_REQUEST *request, S_CLIENT *client,
                      uint16_t caid, uint16_t sid, uint32_t provid,
                      const uint8_t *ecm, int32_t ecm_len,
                      uint8_t *cw)
{
    if (!request) return;
    memset(request, 0, sizeof(*request));
    request->client = client;
    request->account = client ? client->auth.account : NULL;
    request->fd = client ? client->session.fd : -1;
    request->thread_id = client ? client->identity.thread_id : 0;
    request->user = client ? client->identity.user : NULL;
    request->ip = client ? client->identity.ip : NULL;
    request->caid = caid;
    request->sid = sid;
    request->provid = provid;
    request->ecm = ecm;
    request->ecm_len = ecm_len;
    request->cw = cw;
}
