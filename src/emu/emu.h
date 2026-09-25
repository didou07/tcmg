#ifndef TCMG_EMU_H_
#define TCMG_EMU_H_

#include "../ecm/request.h"
#include "../core/reader_types.h"
void    emu_init(void);
int32_t emu_process_reader(const S_ECM_REQUEST *request, const S_READER *reader);

#endif
