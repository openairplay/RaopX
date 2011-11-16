/*
 * zero_conf.h
 * RaopX
 *
 *************************************************************************/

#ifndef __ZERO_CONF_H_
#define __ZERO_CONF_H


typedef struct host_port_t {
	char raop_host[40];
	int raop_port;
}host_port_t;


void ZCHandleEvents(DNSServiceRef serviceRef);
void ZCResolveCallBack(DNSServiceRef serviceRef, 
					   DNSServiceFlags flags, 
					   uint32_t interface, 
					   DNSServiceErrorType errorCode, 
					   const char *fullname,
					   const char *hosttarget, 
					   uint16_t port, 
					   uint16_t txtLen, 
					   unsigned const char *txtRecord, 
					   void *context);
DNSServiceErrorType ZCDNSServiceResolve(uint32_t interfaceIndex,
										const char * name,
										const char * type,
										const char * domain);
void ZCBrowseCallBack(DNSServiceRef service,
					  DNSServiceFlags flags,
					  uint32_t interfaceIndex,
					  DNSServiceErrorType errorCode,
					  const char * name,
					  const char * type,
					  const char * domain,
					  void * context);
DNSServiceErrorType ZCDNSServiceBrowse();

#endif