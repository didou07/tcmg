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
    int32_t rc = emu_process_reader(req->request, req->reader);
    if (req->failure) *req->failure = rc == EMU_KEY_NOT_FOUND ? READER_FAILURE_NOT_FOUND :
                                      rc == EMU_OK ? READER_FAILURE_NONE : READER_FAILURE_READER_ERROR;
    return rc;
}

int32_t reader_pcsc_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->reader || !req->request) return -1;
    r = req->request;
    if (!r->cw || !r->ecm) return -1;
    int32_t rc = pcsc_do_ecm_reader(req->reader->device, r->caid,
                                     r->ecm, (size_t)r->ecm_len, r->cw,
                                     req->reader->ecm_whitelist);
    if (req->failure) *req->failure = rc == 0 ? READER_FAILURE_NONE :
                                      rc == -13 ? READER_FAILURE_REJECTED :
                                      (rc == -3 || rc == -4 || rc == -5 || rc == -7 || rc == -8 || rc == -10) ? READER_FAILURE_TRANSPORT_ERROR :
                                      READER_FAILURE_CARD_ERROR;
    return rc;
}

int32_t reader_cccam_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->request) return -1;
    r = req->request;
    int32_t rc = cccam_reader_do_ecm(req->index, req->reader, r->caid, r->sid,
                                     r->provid, r->ecm, r->ecm_len, r->cw);
    if (req->failure) *req->failure = rc == 0 ? READER_FAILURE_NONE :
                                      rc == -9 ? READER_FAILURE_NOT_FOUND :
                                      (rc == -3 || rc == -5 || rc == -6 || rc == -7 || rc == -8) ? READER_FAILURE_TRANSPORT_ERROR :
                                      READER_FAILURE_READER_ERROR;
    return rc;
}

int32_t reader_newcamd_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->request) return -1;
    r = req->request;
    int32_t rc = newcamd_reader_do_ecm(req->index, req->reader, r->caid, r->sid,
                                       r->provid, r->ecm, r->ecm_len, r->cw);
    if (req->failure) *req->failure = rc == 0 ? READER_FAILURE_NONE :
                                      rc == -4 ? READER_FAILURE_NOT_FOUND :
                                      (rc == -3 || rc == -5) ? READER_FAILURE_TRANSPORT_ERROR :
                                      READER_FAILURE_READER_ERROR;
    return rc;
}

int32_t reader_cs378x_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->request) return -1;
    r = req->request;
    int32_t rc = cs378x_reader_do_ecm(req->index, req->reader, r->caid, r->sid,
                                      r->provid, r->ecm, r->ecm_len, r->cw);
    if (req->failure) *req->failure = rc == 0 ? READER_FAILURE_NONE :
                                      (rc == -3 || rc == -4 || rc == -5 || rc == -6 || rc == -7 || rc == -8) ? READER_FAILURE_TRANSPORT_ERROR :
                                      READER_FAILURE_READER_ERROR;
    return rc;
}

int32_t reader_internal_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->reader || !req->request) return -1;
    r = req->request;
    if (!r->cw || !r->ecm) return -1;
    int32_t rc = internal_do_ecm_reader(req->index, r->caid, r->ecm, (size_t)r->ecm_len,
                                         r->cw, req->reader->ecm_whitelist, req->failure);
    return rc;
}

int32_t reader_serial_do_ecm(const S_READER_ECM_REQUEST *req)
{
    const S_ECM_REQUEST *r;
    if (!req || !req->reader || !req->request) return -1;
    r = req->request;
    if (!r->cw || !r->ecm) return -1;
    int32_t rc = serial_do_ecm_reader(req->index, r->caid, r->ecm, (size_t)r->ecm_len,
                                       r->cw, req->reader->ecm_whitelist, req->failure);
    return rc;
}
