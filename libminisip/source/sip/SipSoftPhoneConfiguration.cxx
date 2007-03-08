/*
 Copyright (C) 2004-2006 the Minisip Team
 
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

/* Copyright (C) 2004, 2005
 *
 * Name
 * 	SipSoftPhoneConfiguration.cxx
 *
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
 *          Cesc Santasusana < cesc Dot santa at@ gmail dOT com>
 * Purpose
 *          Read and write from the configuration file. 
 *
*/

/****************************************************
 * IF YOU MODIFY THE CONFIG FILE, UPDATE THE VERSION #define
 * (see below)
 *!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

#include<config.h>

#include<libminisip/sip/SipSoftPhoneConfiguration.h>

#include<libminisip/soundcard/SoundIO.h>
#include<libminisip/mediahandler/MediaHandler.h>
#include<libminisip/contactdb/PhoneBook.h>
#include<libminisip/contactdb/MXmlPhoneBookIo.h>
#include<libminisip/contactdb/OnlineMXmlPhoneBookIo.h>
#include<libminisip/configbackend/ConfBackend.h>
#include<libminisip/configbackend/UserConfig.h>
#include<fstream>
#include<libminisip/soundcard/AudioMixer.h>
#include<libmcrypto/SipSimSoft.h>
#ifdef SCSIM_SUPPORT
#include<libmcrypto/SipSimSmartCardGD.h>
#endif
#include<libmcrypto/uuid.h>

#ifdef _WIN32_WCE
#	include<stdlib.h>
#	include"../include/minisip_wce_extra_includes.h"
#endif

#include<libmutil/dbg.h>
#include<libmutil/massert.h>

#include<libminisip/configbackend/OnlineConfBackend.h>

//update both!!!! the str define is to avoid including itoa.h
#define CONFIG_FILE_VERSION_REQUIRED 3
#define CONFIG_FILE_VERSION_REQUIRED_STR "3"

using namespace std;

SipSoftPhoneConfiguration::SipSoftPhoneConfiguration(): 
	//securityConfig(),
	//sip(NULL),
	useSTUN(false),
	stunServerPort(0),
	findStunServerFromSipUri(false),
	findStunServerFromDomain(false),
	stunDomain(""),
	useUserDefinedStunServer(false),
	useAnat(false),
	soundDeviceIn(""),
	soundDeviceOut(""),
	videoDevice(""),
	usePSTNProxy(false),
	tcp_server(false),
	tls_server(false),
	ringtone(""),
	p2tGroupListServerPort(0)
{
	sipStackConfig = new SipStackConfig;
}

SipSoftPhoneConfiguration::~SipSoftPhoneConfiguration(){
	std::list<MRef<PhoneBook *> >::iterator i;
	for (i=phonebooks.begin(); i!=phonebooks.end() ; i++){
		(*i)->free();
	}
}

void SipSoftPhoneConfiguration::save(){
	massert(backend); // This will happen if save() is done without first 
			 // having done a load() (bug).
			 
	//Set the version of the file ... 
	backend->save( "version", CONFIG_FILE_VERSION_REQUIRED );
	
	backend->save( "local_udp_port", sipStackConfig->localUdpPort );
	backend->save( "local_tcp_port", sipStackConfig->localTcpPort );
	backend->save( "local_tls_port", sipStackConfig->localTlsPort );
	backend->saveBool( "auto_answer", sipStackConfig->autoAnswer );
	backend->save( "instance_id", sipStackConfig->instanceId);

	//securityConfig.save( backend );
	
	list< MRef<SipIdentity *> >::iterator iIdent;
	uint32_t ii = 0;

	string accountPath;

	for( iIdent = identities.begin(); iIdent != identities.end(); ii++, iIdent ++){
		//cerr << "Saving identity: " << (*iIdent)->getDebugString() << endl;
		accountPath = string("account[")+itoa(ii)+"]/";
		
		(*iIdent)->lock();

		backend->save( accountPath + "account_name", (*iIdent)->identityIdentifier );
		
		backend->save( accountPath + "sip_uri", (*iIdent)->getSipUri().getUserIpString() );
		

/*From SipDialogSecurity below*/
		backend->saveBool(accountPath + "secured", (*iIdent)->securityEnabled);
		backend->saveBool(accountPath + "use_zrtp", /*use_zrtp*/ (*iIdent)->use_zrtp);
		backend->saveBool(accountPath + "psk_enabled", (*iIdent)->pskEnabled);
		backend->saveBool(accountPath + "dh_enabled", (*iIdent)->dhEnabled);
		backend->saveBool(accountPath + "check_cert", (*iIdent)->checkCert);

		backend->save(accountPath + "psk", (*iIdent)->getPsk() );


		string kaTypeString;
		switch( (*iIdent)->ka_type ){
		case KEY_MGMT_METHOD_MIKEY_DH:
			kaTypeString = "dh";
			break;
		case KEY_MGMT_METHOD_MIKEY_PSK:
			kaTypeString = "psk";
			break;
		case KEY_MGMT_METHOD_MIKEY_PK:
			kaTypeString = "pk";
			break;
		case KEY_MGMT_METHOD_MIKEY_DHHMAC:
			kaTypeString = "dhhmac";
			break;
		case KEY_MGMT_METHOD_MIKEY_RSA_R:
			kaTypeString = "rsa-r";
			break;
		}

		backend->save(accountPath + "ka_type", kaTypeString);


		/***********************************************************
		 * Certificate settings
		 ***********************************************************/

		MRef<certificate_chain*> cert;
		if ((*iIdent)->getSim()){
			cert = (*iIdent)->getSim()->getCertificateChain();
		}else{
			cert = certificate_chain::create(); //create an empty chain if no SIM to simplify code below
		}

		/* Update the certificate part of the configuration file */
		cert->lock();
		cert->init_index();
		MRef<certificate *> certItem = cert->get_next();

		/* The first element is the personal certificate, the next ones
		 * are saved as certificate_chain */
		if( !certItem.isNull() ){
			backend->save(accountPath + "certificate",certItem->get_file());
			backend->save(accountPath + "private_key",certItem->get_pk_file());
			certItem = cert->get_next();
		}

		uint32_t i = 0;

		while( !certItem.isNull() ){
			backend->save(accountPath + "certificate_chain["+itoa(i)+"]",
					certItem->get_file() );
			i++;
			certItem = cert->get_next();
		}

		cert->unlock();

		/* CA database saved in the config file */
		uint32_t iFile = 0;
		uint32_t iDir  = 0;
		MRef<ca_db*> cert_db;
		if ((*iIdent)->getSim())
			cert_db = (*iIdent)->getSim()->getCAs();
		else
			cert_db = ca_db::create();

		cert_db->lock();
		cert_db->init_index();
		MRef<ca_db_item*> caDbItem = cert_db->get_next();


		while( !caDbItem.isNull() ){
			switch( caDbItem->type ){
			case CERT_DB_ITEM_TYPE_FILE:
				backend->save(accountPath + "ca_file["+itoa(iFile)+"]",
						caDbItem->item);
				iFile ++;
				break;
			case CERT_DB_ITEM_TYPE_DIR:
				backend->save(accountPath + "ca_dir["+itoa(iDir)+"]",
						caDbItem->item);
				iDir ++;
				break;
			}

			caDbItem = cert_db->get_next();
		}

		cert_db->unlock();

		// Remove old ca_file entries
		for( i = iFile; ; i++ ){
			string key = accountPath + "ca_file["+itoa(i)+"]";

			if( backend->loadString( key ) == "" ){
				break;
			}

			backend->reset( key );
		}

		// Remove old ca_dir entries
		for( i = iDir; ; i++ ){
			string key = accountPath + "ca_dir["+itoa(i)+"]";

			if( backend->loadString( key ) == "" ){
				break;
			}

			backend->reset( key );
		}



/*From SipDialogSecurity above*/



		backend->saveBool( accountPath + "auto_detect_proxy", false);

		const list<SipUri> &routeSet = (*iIdent)->getRouteSet();

		if( !routeSet.empty() ){
			SipUri proxyUri = *routeSet.begin();

			backend->save( accountPath + "proxy_addr",
				       proxyUri.getIp() );
			backend->save( accountPath + "proxy_port",
				       proxyUri.getPort() );

			string transport = proxyUri.getTransport();

			backend->save( accountPath + "transport", transport );
		}
		else {
			backend->save( accountPath + "proxy_addr", "" );
			backend->save( accountPath + "proxy_port", 0 );
		}

		MRef<SipCredential*> cred = (*iIdent)->getCredential();

		string username;
		string password;

		if( cred ){
			username = cred->getUsername();
			password = cred->getPassword();
		}

		backend->save( accountPath + "proxy_username", username );
		backend->save( accountPath + "proxy_password", password );

		backend->saveBool( accountPath + "pstn_account",
			       (*iIdent) == pstnIdentity );

		backend->saveBool( accountPath + "default_account",
				   (*iIdent) == defaultIdentity );
		
		backend->saveBool( accountPath + "register",
				   (*iIdent)->registerToProxy );

		backend->save( accountPath + "register_expires", (*iIdent)->getSipRegistrar()->getDefaultExpires() );

		(*iIdent)->unlock();

	}
	
	accountPath = "account[" + itoa( ii ) + "]/";
	/* Remove old identities remaining */
	while( backend->loadString( accountPath + "account_name" ) != "" ){
		backend->reset( accountPath + "account_name" );
		backend->reset( accountPath + "sip_uri" );
		backend->reset( accountPath + "proxy_addr" );
		backend->reset( accountPath + "auto_detect_proxy" );
		backend->reset( accountPath + "proxy_username" );
		backend->reset( accountPath + "proxy_password" );
		backend->reset( accountPath + "pstn_account" );
		backend->reset( accountPath + "default_account" );
		backend->reset( accountPath + "register" );
		backend->reset( accountPath + "register_expires" );
		backend->reset( accountPath + "transport" );
		accountPath = "account[" + itoa( ++ii ) + "]/";
	}
	
	// Save soundDeviceIn in sound_device to be backward compatible.
	backend->save( "sound_device", soundDeviceIn );
	backend->save( "sound_device_in", soundDeviceIn );
	backend->save( "sound_device_out", soundDeviceOut );
	
// 	backend->saveBool( "mute_all_but_one", muteAllButOne ); //not used anymore
	
	backend->save( "mixer_type", soundIOmixerType );

	//Save the startup commands
	list<string>::iterator iter; 
	int idx;
	for( idx=0,  iter = startupActions.begin();
			iter != startupActions.end();
			iter++, idx++ ) {
		int pos; 
		string cmdActionsPath = string("startup_cmd[")+itoa(idx)+"]/";
		pos = (*iter).find(' ');
		string cmd = (*iter).substr( 0, pos );
		backend->save( cmdActionsPath + "command", cmd );
		pos ++; //advance to the start of the params ...
		string params = (*iter).substr( pos, (*iter).size() - pos );
		backend->save( cmdActionsPath + "params", params );
	}
		
#ifdef VIDEO_SUPPORT
	backend->save( "video_device", videoDevice );
	backend->save( "frame_width", frameWidth );
	backend->save( "frame_height", frameHeight );
#endif

	list<string>::iterator iCodec;
	uint8_t iC = 0;

	for( iCodec = audioCodecs.begin(); iCodec != audioCodecs.end(); iCodec ++, iC++ ){
		backend->save( "codec[" + itoa( iC ) + "]", *iCodec );
	}

	/************************************************************
	 * PhoneBooks
	 ************************************************************/	
	ii = 0;
	list< MRef<PhoneBook *> >::iterator iPb;
	for( iPb = phonebooks.begin(); iPb != phonebooks.end(); ii++, iPb ++ ){
		backend->save( "phonebook[" + itoa(ii) + "]", 
				     (*iPb)->getPhoneBookId() );
	}

	/************************************************************
	 * STUN settings
	 ************************************************************/
	backend->saveBool("use_stun", useSTUN );
	backend->saveBool("stun_server_autodetect", findStunServerFromSipUri );
	if (findStunServerFromDomain){
		backend->save("stun_server_domain", stunDomain );
	}
	else{
		backend->save("stun_server_domain", "");
	}

	backend->save("stun_manual_server", userDefinedStunServer);
	
	/************************************************************
	 * SIP extensions
	 ************************************************************/
	backend->saveBool( "use_100rel", sipStackConfig->use100Rel );
	backend->saveBool( "use_anat", useAnat );

	/************************************************************
	 * Advanced settings
	 ************************************************************/
	backend->saveBool("tcp_server", tcp_server);
	backend->saveBool("tls_server", tls_server);

	backend->save("ringtone", ringtone);
	
	//add code to load the default network interface
	//<network_interface> into networkInterfaceName
	//We are not saving the interface name of the current localIP ...
	backend->save( "network_interface", networkInterfaceName );

	backend->commit();
}

void SipSoftPhoneConfiguration::addMissingAudioCodecs( MRef<ConfBackend *> be ){
	bool modified = false;

	// Add codecs missing in config.
	MRef<AudioCodecRegistry*> audioCodecReg = AudioCodecRegistry::getInstance();
	AudioCodecRegistry::const_iterator i;
	AudioCodecRegistry::const_iterator last;
	for( i = audioCodecReg->begin(); i != audioCodecReg->end(); i++ ){
		MRef<MPlugin *> plugin = *i;
		MRef<AudioCodec *> codec = dynamic_cast<AudioCodec*>(*plugin);

		if( !codec ){
			cerr << "SipSoftPhoneConfiguration: Not an AudioCodec: " << plugin->getName() << endl;			
			continue;
		}

		string name = codec->getCodecName();

		if( find( audioCodecs.begin(), audioCodecs.end(), name ) == audioCodecs.end() ){
			audioCodecs.push_back( name );
			mdbg << "SipSoftPhoneConfiguration: Add codec " << name << endl;
			modified = true;
		}
	}

	if( modified ){
		int iC = 0;
		list<string>::iterator iCodec;
	
		for( iCodec = audioCodecs.begin(); iCodec != audioCodecs.end(); iCodec ++, iC++ ){
			be->save( "codec[" + itoa( iC ) + "]", *iCodec );
		}
	}
}

string SipSoftPhoneConfiguration::load( MRef<ConfBackend *> be ){
	backend = be;

	//installConfigFile( this->configFileName );

	usePSTNProxy = false;

	string ret = "";

	string account;
	int ii = 0;

	/* Check version first of all! */
	int32_t fileVersion;
	string fileVersion_str;
	fileVersion = backend->loadInt("version", 0);
		//get the string version also ... don't use the itoa.h
//	fileVersion_str = backend->loadString("version", "0");
	if( !checkVersion( fileVersion /*, fileVersion_str*/ ) ) {
		//check version prints a message ... 
		//here, deal with the error
//		ret = "ERROR";
		saveDefault( backend );
//		return ret;
	}
	
	do{


		string accountPath = string("account[")+itoa(ii)+"]/";
		account = backend->loadString(accountPath+"account_name");
		if( account == "" ){
			break;
		}
		MRef<SipIdentity*> ident= new SipIdentity();

		ident->setIdentityName(account);

		if( ii == 0 ){
			//inherited->sipIdentity = ident;
			defaultIdentity=ident;
		}

		ii++;

		string uri = backend->loadString(accountPath + "sip_uri");
		ident->setSipUri(uri);
		
		
/*From SipDialogSecurity below*/

		ident->securityEnabled = backend->loadBool(accountPath + "secured");
		//ident->use_srtp = backend->loadBool(accountPath + "use_srtp");
		//ident->use_srtp = backend->loadBool(accountPath + "use_srtp");
		//if (use_srtp) {
		ident->use_zrtp = backend->loadBool(accountPath + "use_zrtp");
		//}
		ident->dhEnabled   = backend->loadBool(accountPath + "dh_enabled");
		ident->pskEnabled  = backend->loadBool(accountPath + "psk_enabled");
		ident->checkCert   = backend->loadBool(accountPath + "check_cert");


		if( backend->loadString(accountPath + "ka_type", "psk") == "psk" )
			ident->ka_type = KEY_MGMT_METHOD_MIKEY_PSK;

		else if( backend->loadString(accountPath + "ka_type", "psk") == "dh" )
			ident->ka_type = KEY_MGMT_METHOD_MIKEY_DH;
		else if( backend->loadString(accountPath + "ka_type", "psk") == "pk" )
			ident->ka_type = KEY_MGMT_METHOD_MIKEY_PK;
		else if( backend->loadString(accountPath + "ka_type", "psk") == "dhhmac" )
			ident->ka_type = KEY_MGMT_METHOD_MIKEY_DHHMAC;
		else if( backend->loadString(accountPath + "ka_type", "psk") == "rsa-r" )
			ident->ka_type = KEY_MGMT_METHOD_MIKEY_RSA_R;
		else{
			ident->ka_type = KEY_MGMT_METHOD_MIKEY_PSK;
#ifdef DEBUG_OUTPUT
			merr << "Invalid KA type in config file, default to PSK"<<end;
#endif
		}

		string pskString = backend->loadString(accountPath + "psk","Unspecified PSK");
		ident->setPsk(pskString);



		/****************************************************************
		 * Certificate settings
		 ****************************************************************/
#ifdef SCSIM_SUPPORT
		string pin = backend->loadString(accountPath + "hwsim_pin","");
		if (pin.size()>0){
			MRef<SipSimSmartCardGD*> sim = new SipSimSmartCardGD;
			sim ->setPin(pin.c_str());

			assert(sim->verifyPin(0) /*TODO: FIXME: Today we quit if not correct PIN - very temp. solution*/);

			ident->setSim(*sim);

		}
#endif

		string certFile = backend->loadString(accountPath + "certificate","");
		string privateKeyFile = backend->loadString(accountPath + "private_key","");

		MRef<certificate_chain*> certchain = certificate_chain::create();

#ifdef ONLINECONF_SUPPORT
		if(certFile.substr(0,10)=="httpsrp://") {
			OnlineConfBack *conf;
			conf = backend->getConf();
			certificate *cert=NULL;
			cert = conf->getOnlineCert();
			certchain->add_certificate( cert );
		} else
#endif

		if( certFile != "" ){
			certificate * cert=NULL;

			try{
				cert = certificate::load( certFile );
				certchain->add_certificate( cert );
			}
			catch( certificate_exception & ){
				merr << "Could not open the given certificate " << certFile <<end;
			}

			if( privateKeyFile != "" ){

				try{
					cert->set_pk( privateKeyFile );
				}
				catch( certificate_exception_pkey & ){
					merr << "The given private key " << privateKeyFile << " does not match the certificate"<<end;                        }

				catch( certificate_exception &){
					merr << "Could not open the given private key "<< privateKeyFile << end;
				}
			}
		}

		uint32_t iCertFile = 0;
		certFile = backend->loadString(accountPath + "certificate_chain[0]","");



#ifdef ONLINECONF_SUPPORT
		if(certFile.substr(0,10)=="httpsrp://") {
			OnlineConfBack *conf;
			conf = backend->getConf();
			vector<struct contdata*> res;
			string user = conf->getUser();
			conf->downloadReq(user, "certificate_chain",res);/*gets the whole chain*/
			for(int i=0;i<res.size();i++) {
				try {
					certificate *cert = certificate::load((unsigned char *)res.at(i)->data,
							(size_t) res.at(i)->size,
							"httpsrp:///"+user + "/certificate_chain" );
					certchain->add_certificate( cert );
				} catch(certificate_exception &) {
					merr << "Could not open the given certificate" << end;
				}
			}
		}

		else
#endif


		while( certFile != "" ){
			try{
				certificate * cert = certificate::load( certFile );
				certchain->add_certificate( cert );
			}
			catch( certificate_exception &){
				merr << "Could not open the given certificate" << end;
			}
			iCertFile ++;
			certFile = backend->loadString(accountPath + "certificate_chain["+itoa(iCertFile)+"]","");

		}

		MRef<ca_db*> cert_db = ca_db::create();
		iCertFile = 0;
		certFile = backend->loadString(accountPath + "ca_file[0]","");



#ifdef ONLINECONF_SUPPORT
		if(certFile.substr(0,10)=="httpsrp://")
		{
			OnlineConfBack *conf;
			conf = backend->getConf();
			vector<struct contdata*> res;
			string user = conf->getUser();
			conf->downloadReq(user, "certificate_chain",res);
			for(int i=0;i<res.size();i++)
			{
				try{
					certificate *cert = certificate::load((unsigned char *)res.at(i)->data,
							(size_t) res.at(i)->size,
							"httpsrp:///"+user + "/root_cert" );
					cert_db->add_certificate( cert );
				}
				catch( certificate_exception &){
					merr << "Could not open the CA certificate" << end;
				}
			}
		}

		else
#endif


		while( certFile != ""){
			try{
				cert_db->add_file( certFile );
			}
			catch( certificate_exception &e){
				merr << "Could not open the CA certificate " << e.what() << end;
			}
			iCertFile ++;
			certFile = backend->loadString(accountPath + "ca_file["+itoa(iCertFile)+"]","");

		}
		iCertFile = 0;

		certFile = backend->loadString(accountPath + "ca_dir[0]","");

		while( certFile != ""){
			try{
				cert_db->add_directory( certFile );
			}
			catch( certificate_exception &){
				merr << "Could not open the CA certificate directory " << certFile << end;
			}
			iCertFile ++;
			certFile = backend->loadString(accountPath + "ca_dir["+itoa(iCertFile)+"]","");
		}

		if (!ident->getSim()){
			ident->setSim(new SipSimSoft(certchain, cert_db));
		}else{
			ident->getSim()->setCertificateChain(certchain);	//TODO: certchain and cert_db should not be attributes in SipSoftPhoneConfig any more?!
			ident->getSim()->setCAs(cert_db);
		}

/*From SipDialogSecurity above*/

		// 
		// Outbound proxy
		// 
		bool autodetect = backend->loadBool(accountPath + "auto_detect_proxy");
		
		//these two values we collect them, but if autodetect is true, they are not used
		string proxy = backend->loadString(accountPath + "proxy_addr","");
		uint16_t proxyPort = (uint16_t)backend->loadInt(accountPath +"proxy_port", 5060);

		string preferredTransport = backend->loadString(accountPath +"transport", "UDP");

		ident->setSipProxy( autodetect, uri, preferredTransport, proxy, proxyPort );

		string proxyUser = backend->loadString(accountPath +"proxy_username", "");

		string proxyPass = backend->loadString(accountPath +"proxy_password", "");
		MRef<SipCredential*> cred;

		if( proxyUser != "" ){
			cred = new SipCredential( proxyUser, proxyPass );
		}

		ident->setCredential( cred );

		SipUri registrarUri;

		registrarUri.setProtocolId(ident->getSipUri().getProtocolId());
		registrarUri.setIp( ident->getSipUri().getIp() );
		registrarUri.makeValid( true );
		MRef<SipRegistrar*> registrar = new SipRegistrar( registrarUri );

		ident->setSipRegistrar( registrar );

		ident->setDoRegister(backend->loadBool(accountPath + "register"));
		string registerExpires = backend->loadString(accountPath +"register_expires", "");
		if (registerExpires != ""){
			ident->getSipRegistrar()->setRegisterExpires( registerExpires );
			//set the default value ... do not change this value anymore
			ident->getSipRegistrar()->setDefaultExpires( registerExpires ); 
		} 
#ifdef DEBUG_OUTPUT
		else {
			//cerr << "CESC: SipSoftPhoneConf::load : NO ident expires" << endl;
		}
		//cerr << "CESC: SipSoftPhoneConf::load : ident expires every (seconds) " << ident->getSipRegistrar()->getRegisterExpires() << endl;
		//cerr << "CESC: SipSoftPhoneConf::load : ident expires every (seconds) [default] " << ident->getSipRegistrar()->getDefaultExpires() << endl;
#endif

		if (backend->loadBool(accountPath + "pstn_account")){
			pstnIdentity = ident;
			usePSTNProxy = true;
// 			ident->securityEnabled= false;
		}

		if (backend->loadBool(accountPath + "default_account")){
			//sipStackConfig->sipIdentity = ident;
			defaultIdentity=ident;
		}

		identities.push_back(ident);


	}while( true );

	tcp_server = backend->loadBool("tcp_server", true);
	tls_server = backend->loadBool("tls_server");

	string soundDevice = backend->loadString("sound_device","");
	soundDeviceIn = backend->loadString("sound_device_in",soundDevice);
	soundDeviceOut = backend->loadString("sound_device_out",soundDeviceIn);
	
	soundIOmixerType = backend->loadString("mixer_type", "spatial");
// 	cerr << "sipconfigfile : soundiomixertype = " << soundIOmixerType << endl << endl;

	//Load the startup commands ... there may be more than one
	//cmd entry may not contain white spaces (anything after the space is considered
	// 	as a param
	//params list is a white space separated list of parameters:
	//Ex.
	//	call user@domain
	//	im user@domain message (the message is the last param, and may contain spaces)
	ii = 0;
	do{
		string cmdActionsPath = string("startup_cmd[")+itoa(ii)+"]/";
		string cmd = backend->loadString(cmdActionsPath + "command");
		if( cmd == "" ) {
			break;
		}
		string params = backend->loadString(cmdActionsPath + "params");
		startupActions.push_back( cmd + " " + params );
// 		cerr << "CONFIG: startup command: " << cmd << " " << params << endl;
		ii++;
	}while( true );
	
#ifdef VIDEO_SUPPORT
	videoDevice = backend->loadString( "video_device", "" );
	cerr << "Loaded video_device" << videoDevice << endl;
	frameWidth = backend->loadInt( "frame_width", 176 );
	frameHeight = backend->loadInt( "frame_height", 144 );
#endif

	sipStackConfig->use100Rel = backend->loadBool("use_100rel");
	useAnat = backend->loadBool("use_anat");

	useSTUN = backend->loadBool("use_stun");
	findStunServerFromSipUri = backend->loadBool("stun_server_autodetect");

	findStunServerFromDomain = backend->loadString("stun_server_domain","")!="";
	stunDomain = backend->loadString("stun_server_domain","");
	useUserDefinedStunServer = backend->loadString("stun_manual_server","")!="";
	userDefinedStunServer = backend->loadString("stun_manual_server","");
	phonebooks.clear();


	int i=0;
	string s;
	do{
		s = backend->loadString("phonebook["+itoa(i)+"]","");

		if (s!=""){
			MRef<PhoneBook *> pb;
		   if (s.substr(0,7)=="file://"){
		      pb = PhoneBook::create(new MXmlPhoneBookIo(s.substr(7)));
		   }
#ifdef ONLINECONF_SUPPORT
		   if (s.substr(0,10)=="httpsrp://")
		     {
			OnlineConfBack *conf;
			conf = backend->getConf(); 
			pb = PhoneBook::create(new OnlineMXmlPhoneBookIo(conf));
			
		     }
#endif
		   
			// FIXME http and other cases should go here
			if( !pb.isNull() ){
				phonebooks.push_back(pb);
			} else{
				merr << "Could not open the phonebook " << end;
			}
		}
		i++;
	}while(s!="");

	ringtone = backend->loadString("ringtone","");

	sipStackConfig->localUdpPort = backend->loadInt("local_udp_port",5060);
	sipStackConfig->externalContactUdpPort = sipStackConfig->localUdpPort; //?
	sipStackConfig->localTcpPort = backend->loadInt("local_tcp_port",5060);
	sipStackConfig->localTlsPort = backend->loadInt("local_tls_port",5061);
	sipStackConfig->autoAnswer = backend->loadBool("auto_answer");
	sipStackConfig->instanceId = backend->loadString("instance_id");

	if( sipStackConfig->instanceId.empty() ){
		MRef<Uuid*> uuid = Uuid::create();

		sipStackConfig->instanceId = "\"<urn:uuid:" + uuid->toString() + ">\"";
	}

	//securityConfig.load( backend ); //TODO: EEEE Load security per identity

	// FIXME: per identity security
/*	if( inherited->sipIdentity){
		inherited->sipIdentity->securitySupport = securityConfig.secured;
	}
*/

//	if ( defaultIdentity){
//		defaultIdentity->securitySupport = securityConfig.secured;
//	}

	audioCodecs.clear();
	int iCodec = 0;
	string codec = backend->loadString("codec["+ itoa( iCodec ) + "]","");

	while( codec != "" && iCodec < 256 ){
		audioCodecs.push_back( codec );
		codec = backend->loadString("codec["+ itoa( ++iCodec ) +"]","");
	}

	addMissingAudioCodecs( backend );

	//add code to load the default network interface
	//<network_interface> into networkInterfaceName
	networkInterfaceName = backend->loadString("network_interface", "");
	
	//cerr << "EEEE: SIM: sim is "<< (sipStackConfig?"not NULL":"NULL")<< endl;
	return ret;

}

void SipSoftPhoneConfiguration::saveDefault( MRef<ConfBackend *> be ){
	//be->save( "version", CONFIG_FILE_VERSION_REQUIRED_STR );
	be->save( "version", CONFIG_FILE_VERSION_REQUIRED );
	
#ifdef WIN32
	be->save( "network_interface", "{12345678-1234-1234-12345678}" );
#else
	be->save( "network_interface", "eth0" );
#endif
	
	be->save( "account[0]/account_name", "My account" );
	be->save( "account[0]/sip_uri", "username@domain.example" );
	be->save( "account[0]/proxy_addr", "sip.domain.example" );
	be->saveBool( "account[0]/register", true );
	be->save( "account[0]/proxy_port", 5060 );
	be->save( "account[0]/proxy_username", "user" );
	be->save( "account[0]/proxy_password", "password" );
	be->saveBool( "account[0]/pstn_account", false );
	be->saveBool( "account[0]/default_account", true );

	be->saveBool( "account[0]/secured", false );
	be->save( "account[0]/ka_type", "psk" );
	be->save( "account[0]/psk", "Unspecified PSK" );
	be->save( "account[0]/certificate", "" );
	be->save( "account[0]/private_key", "" );
	be->save( "account[0]/ca_file", "" );
	be->saveBool( "account[0]/dh_enabled", false );
	be->saveBool( "account[0]/psk_enabled", false );
	be->saveBool( "account[0]/check_cert", true );
	
	be->saveBool( "tcp_server", true );
	be->saveBool( "tls_server", false );
	be->save( "local_udp_port", 5060 );
	be->save( "local_tcp_port", 5060 );
	be->save( "local_tls_port", 5061 );

#ifdef WIN32
	be->save( "sound_device", "dsound:0" );
#else
	be->save( "sound_device", "/dev/dsp" );
#endif
	
	be->save( "mixer_type", "spatial" );

#if defined HAS_SPEEX && defined HAS_GSM
	be->save( "codec[0]", "speex" );
	be->save( "codec[1]", "G.711" );
	be->save( "codec[2]", "GSM" );
#elif defined HAS_SPEEX
	be->save( "codec[0]", "speex" );
	be->save( "codec[1]", "G.711" );
#elif defined HAS_GSM
	be->save( "codec[0]", "G.711" );
	be->save( "codec[1]", "GSM" );
#else
	be->save( "codec[0]", "G.711" );
#endif

	be->save( "phonebook[0]", "file://" + getDefaultPhoneBookFilename() );

//we can save startup commands ... but do nothing by default ...
//<startup_cmd><command>call</command><params>uri</params></startup_cmd>
	
	be->commit();
	
}


string SipSoftPhoneConfiguration::getDefaultPhoneBookFilename() {
	return UserConfig::getFileName( "minisip.addr" );
}

bool SipSoftPhoneConfiguration::checkVersion( uint32_t fileVersion/* , string fileVersion_str */) {
	string str="";
	bool ret = false;
	if( fileVersion != CONFIG_FILE_VERSION_REQUIRED ) {
		cerr << "ERROR? Your config file is an old version (some things may not work)" << endl
			<< "    If you delete it (or rename it), next time you open minisip" << endl
			<< "    a valid one will be created (you will have to reenter your settings" << endl;
		ret = false;
	} else {
#ifdef DEBUG_OUTPUT
		str += "Config file version checked ok!\n";
#endif
		cerr << str;
		ret = true;
	}
	return ret;
}

MRef<SipIdentity *> SipSoftPhoneConfiguration::getIdentity( string id ) {
	list< MRef<SipIdentity*> >::iterator it;

	for( it = identities.begin(); it!=identities.end(); it++ ) {
		if( (*it)->getId() == id ) {
			return (*it);
		}
	}

	return NULL;
}

MRef<SipIdentity *> SipSoftPhoneConfiguration::getIdentity( const SipUri &uri ) {
	list< MRef<SipIdentity*> >::iterator it;
	SipUri tmpUri = uri;

	// Search registered contact addresses
	for( it = identities.begin(); it!=identities.end(); it++ ) {
		MRef<SipIdentity*> &identity =  *it;

		identity->lock();
		const list<SipUri> &contacts =
			identity->getRegisteredContacts();

		if( find( contacts.begin(), contacts.end(), tmpUri ) !=
		    contacts.end() ){
#ifdef DEBUG_OUTPUT
			cerr << "Found registered identity " << identity->identityIdentifier << endl;
#endif
			identity->unlock();
			return identity;
		}

		identity->unlock();
	}

	// Search identity AOR user names
	for( it = identities.begin(); it!=identities.end(); it++ ) {
		MRef<SipIdentity*> &identity =  *it;

		identity->lock();

		SipUri identityUri = 
			identity->getSipUri();

		if( identityUri.getUserName() == tmpUri.getUserName() ){
#ifdef DEBUG_OUTPUT
			cerr << "Found AOR identity " << identity->identityIdentifier << endl;
#endif
			identity->unlock();
			return identity;
		}
		identity->unlock();
	}

	return NULL;
}

