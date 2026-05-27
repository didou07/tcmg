
#ifndef TCMG_BAN_H_
#define TCMG_BAN_H_

/* Returns true when 'ip' is currently serving a ban. */
bool ban_is_banned(const char *ip);

/* Increment fail counter; apply ban when threshold is reached. */
void ban_record_fail(const char *ip);

/* Clear fail counter and any active ban for 'ip'. */
void ban_record_ok(const char *ip);

/* Free all entries (called at shutdown). */
void ban_free_all(void);

#endif /* TCMG_BAN_H_ */
