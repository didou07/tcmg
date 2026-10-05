#ifndef TCMG_LEGACY_SH4_PREINCLUDE_H_
#define TCMG_LEGACY_SH4_PREINCLUDE_H_

#ifndef _Static_assert
#define TCMG_LEGACY_JOIN2(a, b) a##b
#define TCMG_LEGACY_JOIN(a, b) TCMG_LEGACY_JOIN2(a, b)
#define _Static_assert(cond, msg) \
    typedef char TCMG_LEGACY_JOIN(tcmg_static_assert_, __LINE__)[(cond) ? 1 : -1]
#endif

#endif
