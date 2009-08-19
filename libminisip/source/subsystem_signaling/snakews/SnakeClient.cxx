/*
 Copyright (C) 2009 the Minisip Team
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Copyright (C) 2009
 *
 * Authors: Erik Eliasson <ere@kth.se>
*/

#include<config.h>

#include"SnakeClient.h"
#include<iostream>
#include<string>

#include"callbackservice_autogen_src/axis2_stub_CallbackServiceUserStubService.h"
#include"servicemanager_autogen_src/axis2_stub_ServicesManagerUserStubService.h"
#include"presenceagent_autogen_src/axis2_stub_PresenceAgentUserStubService.h"
#include<libmsip/SipCommandString.h>

using namespace std;

PresenceService::PresenceService(MRef<CommandReceiver*> c, MRef<SipSoftPhoneConfiguration*> conf, string i, string u){
	callback=c;
	pconf=conf;
	id=i;
	url=u;
}

void PresenceService::registerContacts(){
	list<string> contacts;

	std::list<MRef<PhoneBook *> >::iterator i;
	for (i=pconf->phonebooks.begin(); i!=pconf->phonebooks.end() ; i++){
		list< MRef<PhoneBookPerson *> > persons = (*i)->getPersons();
		list< MRef<PhoneBookPerson *> >::iterator j;
		for (j=persons.begin(); j!=persons.end(); j++){
			list< MRef<ContactEntry *> > entries = (*j)->getEntries();
			list< MRef<ContactEntry *> >::iterator ent;
			for (ent=entries.begin(); ent!=entries.end(); ent++){
				contacts.push_back( (*ent)->getUri() );
			}
		
		}
	}

	bool quit=false;
	axutil_env_t *env = NULL;
	axis2_char_t *client_home = NULL;
	axis2_char_t *endpoint_uri = NULL;
	axis2_stub_t *stub = NULL;

	endpoint_uri = (axis2_char_t*)url.c_str();
	env = axutil_env_create_all("alltest.log", AXIS2_LOG_LEVEL_TRACE);
	client_home = AXIS2_GETENV("AXIS2C_HOME");

	stub = axis2_stub_create_PresenceAgentUserStubService(env, client_home, endpoint_uri);

	adb_setContactList8_t* req_c = adb_setContactList8_create(env);

	adb_setContactList_t* req = adb_setContactList_create(env);
	adb_setContactList_set_sessionId(req, env, id.c_str() );

	list<string>::iterator si;
	for (si=contacts.begin(); si!=contacts.end(); si++){
		adb_setContactList_add_contactAddress(req, env, (*si).c_str() );
	}
	adb_setContactList8_set_setContactList(req_c, env, req);

	adb_setContactListResponse1_t* resp_c = axis2_stub_op_PresenceAgentUserStubService_setContactList(stub,env,req_c);

	adb_setContactListResponse_t* resp = adb_setContactListResponse1_get_setContactListResponse(resp_c, env);






#if 0
	adb_getStatus2_t* req_c = adb_getStatus2_create(env);

	adb_getStatus_t* req = adb_getStatus_create(env);
	adb_getStatus_set_sessionId(req, env, id.c_str() );
	adb_getStatus_set_userAddress(req, env, "erik@hdviper.org");

	adb_getStatus2_set_getStatus(req_c, env, req);

	adb_getStatusResponse3_t* resp_c = axis2_stub_op_PresenceAgentUserStubService_getStatus(stub, env, req_c );
	adb_getStatusResponse_t* resp = adb_getStatusResponse3_get_getStatusResponse( resp_c, env );

	adb_userStatus_t* status = adb_getStatusResponse_get_userStatus(resp, env);

	string descr = adb_userStatus_get_description( status, env);
	string sipaddr = adb_userStatus_get_sipAddress(status, env);
	adb_statusType_t* type = adb_userStatus_get_type( status, env);
	string t = adb_statusType_get_statusType(type,env);


	cerr << "EEEE: PRESENCE: descr="<<descr<<" sipaddr="<<sipaddr<<" type="<<t<<endl;
#endif

}

void PresenceService::statusUpdated(){
	cerr << "EEEE: PresenceService::statusUpdated running"<<endl;
	list<string> contacts;

	std::list<MRef<PhoneBook *> >::iterator i;
	for (i=pconf->phonebooks.begin(); i!=pconf->phonebooks.end() ; i++){
		list< MRef<PhoneBookPerson *> > persons = (*i)->getPersons();
		list< MRef<PhoneBookPerson *> >::iterator j;
		for (j=persons.begin(); j!=persons.end(); j++){
			list< MRef<ContactEntry *> > entries = (*j)->getEntries();
			list< MRef<ContactEntry *> >::iterator ent;
			for (ent=entries.begin(); ent!=entries.end(); ent++){
				contacts.push_back( (*ent)->getUri() );
			}
		
		}
	}

	bool quit=false;
	axutil_env_t *env = NULL;
	axis2_char_t *client_home = NULL;
	axis2_char_t *endpoint_uri = NULL;
	axis2_stub_t *stub = NULL;

	endpoint_uri = (axis2_char_t*)url.c_str();
	env = axutil_env_create_all("alltest.log", AXIS2_LOG_LEVEL_TRACE);
	if (!env){
		cerr << "EEEE: WARNING: env==NULL"<<endl;
		return;
	}
	client_home = AXIS2_GETENV("AXIS2C_HOME");

	stub = axis2_stub_create_PresenceAgentUserStubService(env, client_home, endpoint_uri);

	if (!stub){
		cerr << "EEEE: WARNING: stub==NULL"<<endl;
		return;
	}

	adb_getStatuses10_t* req_c = adb_getStatuses10_create(env);
	adb_getStatuses_t* req = adb_getStatuses_create( env );
	adb_getStatuses_set_sessionId(req, env, id.c_str() );

	list<string>::iterator si;
	for (si=contacts.begin(); si!=contacts.end(); si++){
		adb_getStatuses_add_userAddress(req, env, (*si).c_str() );
	}

	adb_getStatuses10_set_getStatuses(req_c, env, req);

	adb_getStatusesResponse0_t* resp_c = axis2_stub_op_PresenceAgentUserStubService_getStatuses(stub, env, req_c );

	adb_getStatusesResponse_t* resp = adb_getStatusesResponse0_get_getStatusesResponse(resp_c, env);

	int n = adb_getStatusesResponse_sizeof_userStatus(resp, env);
	cerr << "EEEE: PresenceService::statusUpdated checking statuses for n="<<n<<endl;

	for(int i = 0; i < n; i ++ ){
		adb_userStatus_t* status = adb_getStatusesResponse_get_userStatus_at(resp, env, i);
		string uri = adb_userStatus_get_sipAddress(status, env);
		adb_statusType_t* type= adb_userStatus_get_type(status, env);
		string t = adb_statusType_get_statusType(type,env);


		std::list<MRef<PhoneBook *> >::iterator i;
		for (i=pconf->phonebooks.begin(); i!=pconf->phonebooks.end() ; i++){
			list< MRef<PhoneBookPerson *> > persons = (*i)->getPersons();
			list< MRef<PhoneBookPerson *> >::iterator j;
			for (j=persons.begin(); j!=persons.end(); j++){
				list< MRef<ContactEntry *> > entries = (*j)->getEntries();
				list< MRef<ContactEntry *> >::iterator ent;
				for (ent=entries.begin(); ent!=entries.end(); ent++){
					if ((*ent)->getUri()== uri){

						if (t=="ONLINE" && !(*ent)->isOnline() ){
							cerr << "----->STATUS CHANGE: "<< uri<<" is "<<t<<endl;
							CommandString cmd("",SipCommandString::remote_presence_update,uri,"online");
							callback->handleCommand("gui", cmd);
						}
						if (t=="OFFLINE" && (*ent)->isOnline() ){
							cerr << "----->STATUS CHANGE: "<< uri<<" is "<<t<<endl;
							CommandString cmd("",SipCommandString::remote_presence_update,uri,"offline");
							callback->handleCommand("gui", cmd);
						}

					
					}
				}

			}
		}

	}




}


CallbackService::CallbackService(MRef<CommandReceiver*> c, string i, string u){
	callback=c;
	id=i;
	last_eid=0;
	url=u;
}

void CallbackService::start(){
	Thread t(this);
}

void CallbackService::run(){
#ifdef DEBUG_OUTPUT
	setThreadName("CallbackService (Snake)");
#endif
	bool quit=false;
	axutil_env_t *env = NULL;
		axis2_char_t *client_home = NULL;
		axis2_char_t *endpoint_uri = NULL;
		axis2_stub_t *stub = NULL;

		endpoint_uri = (axis2_char_t*)url.c_str();
		env = axutil_env_create_all("alltest.log", AXIS2_LOG_LEVEL_TRACE);
		client_home = AXIS2_GETENV("AXIS2C_HOME");

		stub = axis2_stub_create_ServicesManagerUserStubService(env, client_home, endpoint_uri);

		while (!quit){
			adb_getEvents3_t* req_c0 = adb_getEvents3_create(env);
			adb_getEvents_t* req = adb_getEvents_create(env);
			adb_getEvents_set_sessionId( req, env, id.c_str() );
			adb_getEvents_set_lastEventId( req, env, last_eid );

			adb_getEvents3_set_getEvents(req_c0,env,req);


			cerr << "EEEE: CallbackService::run: calling ws..."<<endl;
			adb_getEventsResponse0_t* resp_c0 = axis2_stub_op_CallbackServiceUserStubService_getEvents(stub,env, req_c0);
			cerr << "EEEE: CallbackService::run: calling ws done"<<endl;


			adb_getEventsResponse_t* resp = adb_getEventsResponse0_get_getEventsResponse( resp_c0, env );

			int nservices = adb_getEventsResponse_sizeof_event(resp,env);


			for(int i = 0; i < nservices; i ++ ){
				adb_snakeEvent_t* e = adb_getEventsResponse_get_event_at(resp, env, i);
				int64_t eid = adb_snakeEvent_get_eventId(e, env);
				axis2_char_t* meth= adb_snakeEvent_get_methodId(e, env);
				axis2_char_t* recv= adb_snakeEvent_get_receiver(e, env);
				axis2_char_t* sname= adb_snakeEvent_get_serviceName(e, env);
				cerr << "EEEE: SNAKE CALLBACK: eid="<< eid<<": method <"<<meth<<"> receiver <"<< recv<<"> service name <"<<sname<<">"<<endl;

				last_eid=eid;

				if (string("USER_STATUS_CHANGED")==meth){
					cerr << "EEEE: Detected USER_STATUS_CHANGED - sending snake_presence_update"<<endl;
					CommandString cmd("","snake_presence_update");
					callback->handleCommand("snake", cmd);
				}

			}
			if (!nservices)
				Thread::msleep(2000);
		}


}

SnakeClient::SnakeClient(MRef<SipSoftPhoneConfiguration*> conf){
	pconf=conf;
}

void SnakeClient::handleCommand(std::string subsystem, const CommandString& command){
	cerr <<"EEEE: SnakeClient::handleCommand: received "<< command.getOp() << endl;

	if (command.getOp()=="snake_presence_update"){
		if (presenceService)
			presenceService->statusUpdated();
	}

	if (command.getOp()=="service_manager"){
		string smurl = command.getParam();
		string id = command.getDestinationId();
		cerr << "EEEE: sm url=<"<<smurl<<">"<<endl;

		axutil_env_t *env = NULL;
		axis2_char_t *client_home = NULL;
		axis2_char_t *endpoint_uri = NULL;
		axis2_stub_t *stub = NULL;

		endpoint_uri = (axis2_char_t*)smurl.c_str();
		env = axutil_env_create_all("alltest.log", AXIS2_LOG_LEVEL_TRACE);
		client_home = AXIS2_GETENV("AXIS2C_HOME");

		cerr<<"EEEE: connecting to service manager web service..."<<endl;
		stub = axis2_stub_create_ServicesManagerUserStubService(env, client_home, endpoint_uri);
		cerr<<"EEEE: connecting to service manager web service done"<<endl;


		adb_getServicesResponse1_t* resp;
		adb_getServices0_t *request = adb_getServices0_create( env );
		adb_getServices_t *req = adb_getServices_create( env );

		adb_getServices_set_sessionId(req, env, id.c_str());

		adb_getServices0_set_getServices( request, env, req);

		resp = axis2_stub_op_ServicesManagerUserStubService_getServices( stub, env, request);

		adb_getServicesResponse_t* r = adb_getServicesResponse1_get_getServicesResponse(resp,env);

//		axutil_array_list_t* services = adb_getServicesResponse_get_services(r,env);
//
		int nservices = adb_getServicesResponse_sizeof_service(r,env);

		cerr << "EEEE: nr services: "<< nservices<<endl;

		for(int i = 0; i < nservices; i ++ ){
			adb_service_t* s = adb_getServicesResponse_get_service_at(r, env, i);
			axis2_char_t* saddr = adb_service_get_address(s, env);
			axis2_char_t* sname = adb_service_get_name(s, env);

			if (string("callback-service")==sname){
				callbackService = new CallbackService(callback, id, saddr);
				callbackService->start();
			}
			if (string("presence-agent")==sname){
				presenceService = new PresenceService(callback, pconf, id, saddr);
				presenceService->registerContacts();
				presenceService->statusUpdated();
			}



		
			cerr << "EEEE: service "<< i+1<<": name <"<<sname<<"> addr <"<< saddr<<">"<<endl;
		
		}


	}





}

CommandString SnakeClient::handleCommandResp(std::string subsystem, const CommandString&){

}


