#ifndef CBLOG_CONF_H
#define CBLOG_CONF_H

struct Status {
	bool syslog_open;
	time_t conf_mdate;
};

extern HDF* config;
extern struct Status *cblog_status;

#endif
