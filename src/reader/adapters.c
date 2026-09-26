#define MODULE_LOG_PREFIX "reader"
#include "proto-adapters.h"
#include "emu/emu.h"
#include "pcsc/pcsc.h"
#include "internal/internal.h"
#include "protocols/cccam.h"
#include "protocols/cs378x.h"
#include "protocols/newcamd.h"
#include "../serial/serial.h"

int32_t reader_emu_do_ecm(const S_READER_ECM_REQUEST *req)
{
    if (!req || !req->reader || !req->request || !req->request->cw ||
        !req->request->ecm || !req->request->account) return -1;
    return emu_process_reader(req->request, req->reader);
}

int32_t reader_pcsc_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->reader || !req->request) return -1;
    r = req->request;
    if (!r->cw || !r->ecm) return -1;
    if (!req->reader->do_ecm) return -2;
    return pcsc_do_ecm_reader(req->reader->device, r->caid,
                              r->ecm, (size_t)r->ecm_len, r->cw,
                              req->reader->ecm_whitelist);
}

int32_t reader_cccam_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->request) return -1;
    r = req->request;
    return cccam_reader_do_ecm(req->index, req->reader, r->caid, r->sid,
                                r->provid, r->ecm, r->ecm_len, r->cw);
}

int32_t reader_newcamd_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->request) return -1;
    r = req->request;
    return newcamd_reader_do_ecm(req->index, req->reader, r->caid, r->sid,
                                  r->provid, r->ecm, r->ecm_len, r->cw);
}

int32_t reader_cs378x_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->request) return -1;
    r = req->request;
    return cs378x_reader_do_ecm(req->index, req->reader, r->caid, r->sid,
                                 r->provid, r->ecm, r->ecm_len, r->cw);
}

int32_t reader_internal_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->reader || !req->request) return -1;
    r = req->request;
    if (!r->cw || !r->ecm) return -1;
    if (!req->reader->do_ecm) return -2;
    return internal_do_ecm_reader(req->index, r->caid, r->ecm, (size_t)r->ecm_len,
                                  r->cw, req->reader->ecm_whitelist);
}

int32_t reader_serial_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->reader || !req->request) return -1;
    r = req->request;
    if (!r->cw || !r->ecm) return -1;
    if (!req->reader->do_ecm) return -2;
    return serial_do_ecm_reader(req->index, r->caid, r->ecm, (size_t)r->ecm_len,
                                r->cw, req->reader->ecm_whitelist);
}
