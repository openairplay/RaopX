/*
 * zero_conf.c
 * RaopX
 *
 * zero_conf.c is put together from the examples in the book:
 * Zero Configuration Networking: The Definitive Guide, by Stuart Cheshire and
 * Daniel H. Steinberg, (c) 2006, O'Reilly Media, Inc.
 *
 * 
 *************************************************************************/

#include <dns_sd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "zero_conf.h"


static volatile int stopNow = 0;
static volatile int timeOut = 1;


extern host_port_t hostPortArray[];
extern int numResolved;

void ZCHandleEvents(DNSServiceRef serviceRef)
{
	int dns_sd_fd = DNSServiceRefSockFD(serviceRef);
	int nfds = dns_sd_fd + 1;
	fd_set readfds;
	struct timeval tv;
	int result;
	
	while (!stopNow)
	{
		FD_ZERO(&readfds);
		FD_SET(dns_sd_fd, &readfds);
		tv.tv_sec = timeOut;
		tv.tv_usec = 0;
		
		result = select(nfds, &readfds, (fd_set*)NULL, (fd_set*)NULL, &tv);
		if (result > 0)
		{
			DNSServiceErrorType err = kDNSServiceErr_NoError;
			if (FD_ISSET(dns_sd_fd, &readfds))
				err = DNSServiceProcessResult(serviceRef);
			if (err) stopNow = 1;
		}
		else
		{
			
			if (errno != EINTR) {
				stopNow = 1;
			} else {
				printf("select() returned %d errno %d %s\n", result, errno, strerror(errno));
			}
		}
	}
	
}


void ZCResolveCallBack(DNSServiceRef serviceRef, 
					   DNSServiceFlags flags, 
					   uint32_t interface, 
					   DNSServiceErrorType errorCode, 
					   const char *fullname,
					   const char *hosttarget, 
					   uint16_t port, 
					   uint16_t txtLen, 
					   unsigned const char *txtRecord, 
					   void *context) 
{ 
#pragma unused(flags) 
#pragma unused(fullname) 
	//printf("\nWas in resolve callback\n\n");
    if (errorCode != kDNSServiceErr_NoError) 
        fprintf(stderr, "ZCResolveCallBack returned %d\n", errorCode); 
    else {
		strcpy(hostPortArray[numResolved].raop_host,hosttarget);
		hostPortArray[numResolved].raop_port=ntohs(port);
		numResolved++;
	}
    if (!(flags & kDNSServiceFlagsMoreComing)) fflush(stdout); 
} 

DNSServiceErrorType ZCDNSServiceResolve(uint32_t interfaceIndex,
										const char * name,
										const char * type,
										const char * domain) 
{ 
    DNSServiceErrorType error; 
    DNSServiceRef  serviceRef; 
    error = DNSServiceResolve(&serviceRef, 0, interfaceIndex, name, type, domain, ZCResolveCallBack, NULL);
	if (error == kDNSServiceErr_NoError) 
	{ 
		DNSServiceProcessResult(serviceRef);
		//HandleEvents(serviceRef);  // Add service to runloop to get callbacks 
		DNSServiceRefDeallocate(serviceRef); 
	}
	
	return error;
} 




void ZCBrowseCallBack(DNSServiceRef service,
					  DNSServiceFlags flags,
					  uint32_t interfaceIndex,
					  DNSServiceErrorType errorCode,
					  const char * name,
					  const char * type,
					  const char * domain,
					  void * context)
{
#pragma unused(context)
	
	if (errorCode != kDNSServiceErr_NoError)
		fprintf(stderr, "ZCBrowseCallBack returned %d\n", errorCode);
	else
	{
		//printf("InterfaceIndex: %i\nName: %s\nType: %s\nDomain: %s\n", interfaceIndex, name, type, domain);
		DNSServiceErrorType error = ZCDNSServiceResolve(interfaceIndex, name, type, domain);
		if (error) fprintf(stderr, "DNSServiceDiscovery returned %d\n", error);
	}
	if(!(flags & kDNSServiceFlagsMoreComing)) fflush(stdout);
}



DNSServiceErrorType ZCDNSServiceBrowse()
{
	DNSServiceErrorType error;
	DNSServiceRef serviceRef;
	
	error = DNSServiceBrowse(&serviceRef, 0, 0, "_raop._tcp", NULL, ZCBrowseCallBack, NULL);
	
	if (error == kDNSServiceErr_NoError)
	{
		ZCHandleEvents(serviceRef);
		DNSServiceRefDeallocate(serviceRef);
	}
	
	return error;
}



