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

/* Copyright (C) 2004 
 *
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
 *	    Joachim Orrblad <joachim@orrblad.com>
 *          Mikael Magnusson <mikma@users.sourceforge.net>
*/

#include <config.h>

#include"Session.h"

#include<libminisip/media/CallRecorder.h>
#include<libminisip/media/MediaStream.h>
#include<libminisip/media/Media.h>
#include<libminisip/media/AudioMedia.h>
#include<libminisip/media/video/VideoMedia.h>
#include<libminisip/media/video/codec/VideoCodec.h>
#include<libminisip/media/video/codec/AVCoder.h>
#include<libminisip/media/DtmfSender.h>
#include<libminisip/media/codecs/Codec.h>
#include<libminisip/signaling/sdp/SdpPacket.h>
#include<libminisip/signaling/sdp/SdpHeaderV.h>
#include<libminisip/signaling/sdp/SdpHeaderT.h>
#include<libminisip/signaling/sdp/SdpHeaderC.h>
#include<libminisip/signaling/sdp/SdpHeaderA.h>
#include<libminisip/signaling/sdp/SdpHeaderM.h>
#include<libminisip/signaling/sdp/SdpHeaderS.h>
#include<libminisip/signaling/sdp/SdpHeaderO.h>
#include<libmsip/SipDialogConfig.h>
#include<libmikey/KeyAgreement.h>
#include<libmikey/KeyAgreementDH.h>
#include<libmutil/dbg.h>
#include<libmutil/stringutils.h>
#include<libmutil/Timestamp.h>

#ifdef ZRTP_SUPPORT
#include <libminisip/media/zrtp/ZrtpHostBridgeMinisip.h>
#endif

#ifdef _WIN32_WCE
#	include"../include/minisip_wce_extra_includes.h"
#endif

#define SESSION_LINE "s=Minisip Session"

// pn501 Added for multicodec list operations
using namespace std;

class MikeyConfig: public IMikeyConfig{
	public:
		MikeyConfig( MRef<SipIdentity*> aIdentity ):
				identity(aIdentity) {}

		const std::string getUri() const{
			return identity->getSipUri().getProtocolId() + ":" +
				identity->getSipUri().getUserIpString();
		}

		MRef<SipSim*> getSim() const{
			return identity->getSim();
		}
		MRef<CertificateChain*> getPeerCertificate() const{
			return NULL;
		}
		size_t getPskLength() const{
			return identity->getPsk().size();
		}

		const byte_t* getPsk() const{
			return (byte_t*)identity->getPsk().c_str();
		}

		bool isMethodEnabled( int kaType ) const{
			switch( kaType ){
				case KEY_AGREEMENT_TYPE_DH:
				case KEY_AGREEMENT_TYPE_PK:
				case KEY_AGREEMENT_TYPE_RSA_R:
					return identity->dhEnabled;
				case KEY_AGREEMENT_TYPE_PSK:
				case KEY_AGREEMENT_TYPE_DHHMAC:
					return identity->pskEnabled;
				default:
					return false;
			}
		}

		bool isCertCheckEnabled() const{
			return identity->checkCert;
		}
	private:
		MRef<SipIdentity*> identity;
};


SessionRegistry * Session::registry = NULL;
MRef<KeyAgreement *> Session::precomputedKa = NULL;

Session::Session( string localIp, MRef<SipIdentity*> ident, string localIp6 ):
		started(false),
		localIpString(localIp), 
		localIp6String(localIp6)
{
	identity = ident;
	ka_type = ident->ka_type;

	dtmfTOProvider = new TimeoutProvider<DtmfEvent *, MRef<DtmfSender *> >;
	Session::precomputedKa = NULL;

	mutedSenders = true;
	silencedSources = false;
	profile = 1;
	sendingHeight = 720;
	sendingWidth = 1280;
	
	if( registry ){
		registry->registerSession( this );
	}
}

uint32_t Session::getSendingWidth(){
	return this->sendingWidth;
}

uint32_t Session::getSendingHeight(){
	return this->sendingHeight;
}

int Session::getProfile(){
	return this->profile;
}

void Session::unregister(){
	if( registry ){
		registry->unregisterSession( this );
	}

	if( Session::precomputedKa.isNull()
	    && identity && identity->getSim() ){
		KeyAgreementDH* ka = NULL;
		//ka = new KeyAgreementDH( identity->getSim()->getCertificateChain(), 
		//			 identity->getSim()->getCAs() );
		ka = new KeyAgreementDH( identity->getSim() );
		ka->setGroup( DH_GROUP_OAKLEY5 );
		Session::precomputedKa = ka;
	}
}

Session::~Session(){
	dtmfTOProvider->stopThread();
}


static string matchAnat(string group, MRef<SdpPacket *> offer, string localIpString, string localIp6String){
	vector<string> groups;

#ifdef DEBUG_OUTPUT	
	cerr << "Found group:" << group << endl;
#endif
	if( group.substr(0, 5) == "ANAT "){
		size_t start = 5;
		for(;;){
			string id;
			size_t pos = group.find(' ', start);

			if( pos == string::npos )
				id = group.substr( start );
			else
				id = group.substr( start, pos - start );
			groups.push_back( id );
#ifdef DEBUG_OUTPUT	
			cerr << "Found id: " << id << endl;
#endif
			if( pos == string::npos )
				break;
			
			start = pos + 1;
		}
	}

	// Search for an ANAT group id with all valid streams.
	vector<string>::iterator j;
	for( j = groups.begin(); j != groups.end(); j++ ){
		string id = *j;
		bool idOk = false;
		
		unsigned int i;
		for( i = 0; i < offer->getHeaders().size(); i++ ){
			if( offer->getHeaders()[i]->getType() != SDP_HEADER_TYPE_M )
				continue;
			
			MRef<SdpHeaderM *> offerM = (SdpHeaderM*)*offer->getHeaders()[i];
			string curId = offerM->getAttribute("mid", 0);
			if( id != curId ){
#ifdef DEBUG_OUTPUT	
				cout << "Skip id:" << curId << endl;
#endif
				continue;
			}
			
#ifdef DEBUG_OUTPUT	
			cout << "Header " << i << endl;
#endif
			MRef<SdpHeaderC *> c = offerM->getConnection();

			if( !c ){
#ifdef DEBUG_OUTPUT	
				cout << "No connection header:" << endl;
#endif
				// Ignore, may be an unsupported media type 
				continue;
			}

			if( c->getNetType() != "IN" ){
#ifdef DEBUG_OUTPUT	
				cout << "Unsupported net type:" << c->getNetType() << endl;
#endif
				idOk = false;
				break;
			}
			
			if( c->getAddrType() != "IP4" && localIp6String.empty() ){
#ifdef DEBUG_OUTPUT	
				cout << "Unsupported addr type:" << c->getAddrType() << endl;
#endif
				idOk = false;
				break;
			}
			
			if( c->getAddrType() != "IP6" && localIpString.empty() ){
				cout << "Unsupported addr type:" << c->getAddrType() << endl;
				idOk = false;
				break;
			}
			
			if( offerM->getPort() == 0 ){
#ifdef DEBUG_OUTPUT	
				cout << "Disabled port:" << endl;
#endif
				// Ignore, may be an unsupported media type 
				continue;
			}
			
#ifdef DEBUG_OUTPUT	
			cerr << "Found valid group id:" << curId << endl;
#endif
			idOk = true;
		}
		
		if( idOk ){
#ifdef DEBUG_OUTPUT	
			cout << "Return id:" << id << endl;
#endif
			return id;
		}
	}

	return "";
}

MRef<SdpPacket *> Session::emptySdp(){
	MRef<SdpPacket *> result;

	result = new SdpPacket;

	MRef<SdpHeader*> v = new SdpHeaderV(0);
	result->addHeader(v);

	string ipString;
	string addrtype;

	if( !localIpString.empty() ){
		ipString = localIpString;
		addrtype = "IP4";
	}
	else{
		ipString = localIp6String;
		addrtype = "IP6";
	}

	MRef<SdpHeader*> o = new SdpHeaderO("","3344","3344","IN", 
			addrtype, ipString );
	result->addHeader(o);

	MRef<SdpHeader*> s = new SdpHeaderS(SESSION_LINE);
	result->addHeader(s);

	MRef<SdpHeader*> t = new SdpHeaderT(0,0);
	result->addHeader(t);

	return result;
}

bool Session::addRealtimeMediaToOffer(MRef<SdpPacket*> result, const string &peerUri, bool anatSupported, string transport){
	list< MRef<RealtimeMediaStreamReceiver *> >::iterator i;
	std::list<std::string>::iterator iAttribute;
	std::list<std::string> attributes;
	string type;
	uint16_t localPort = 0;
	MRef<SdpHeaderM *> m;
	std::list<MRef<Codec *> > codecs;
	std::list<MRef<Codec *> >::iterator iC;
	uint8_t payloadType;
	string rtpmap;
	bool anat = false;

// 	cerr << "Session::getSdpOffer" << endl;

	if( anatSupported && !localIpString.empty() && !localIp6String.empty() ){
		anat = true;
		result->setSessionLevelAttribute( "group", "ANAT 1 2" );
	}

	for( i = realtimeMediaStreamReceivers.begin(); i != realtimeMediaStreamReceivers.end(); i++ ){
		codecs = (*i)->getAvailableCodecs();

		type = (*i)->getSdpMediaType();
		m = new SdpHeaderM( type, localPort, 1, transport );
		
		for( iC = codecs.begin(); iC != codecs.end(); iC ++ ){
			payloadType = (*iC)->getSdpMediaType();
			rtpmap = (*iC)->getSdpMediaAttributes();
			
			m->addFormat( itoa(payloadType) );
			if( rtpmap != "" ){
				MRef<SdpHeaderA*> a = new SdpHeaderA("a=X");
				a->setAttributes( "rtpmap:" + itoa( payloadType) + " " + rtpmap );
				m->addAttribute( *a );
			}
			if( (*iC)->getCodecName() == "iLBC" ) { //for now, iLBC codec only supports 20ms frames
				MRef<SdpHeaderA*> ilbc_fmtp = new SdpHeaderA("a=X");
				ilbc_fmtp->setAttributes("fmtp:" + itoa( payloadType) + " mode=20" );
				m->addAttribute(*ilbc_fmtp);
			}
			if( (*iC)->getCodecName() == "H.264" ) { 
				MRef<SdpHeaderA*> h264_fmtp = new SdpHeaderA("a=X");
				//h264_fmtp->setAttributes("fmtp:" + itoa( payloadType) + " profile-level-id=42900b" );
				h264_fmtp->setAttributes("fmtp:" + itoa( payloadType) + " profile-level-id=4d4033" );
				m->addAttribute(*h264_fmtp);
			}

		}
		if (type=="audio"){
			//added static DTMF SDP headers in INVITE to audio media
			m->addFormat( "101" );
			MRef<SdpHeaderA*> dtmf = new SdpHeaderA("a=X");
			dtmf->setAttributes("rtpmap:101 telephone-event/8000");
			m->addAttribute(*dtmf);
			MRef<SdpHeaderA*> dtmf_fmtp = new SdpHeaderA("a=X");
			dtmf_fmtp->setAttributes("fmtp:101 0-15");
			m->addAttribute(*dtmf_fmtp);
		}
		
		result->addHeader( *m );

		attributes = (*i)->getSdpAttributes();
		for( iAttribute = attributes.begin(); iAttribute != attributes.end(); iAttribute ++ ){
			MRef<SdpHeaderA*> a = new SdpHeaderA("a=X");
			a->setAttributes( *iAttribute );
			m->addAttribute( *a );
		}

		if( anat ){
			MRef<SdpHeaderM*> m4 = m;
			MRef<SdpHeaderM*> m6 = new SdpHeaderM( **m );

			// IPv4
			m4->setPort( (*i)->getPort("IP4") );

			MRef<SdpHeaderA*> mid2 = new SdpHeaderA( "a=mid:2" );

			m4->addAttribute( mid2 );

			MRef<SdpHeaderC*> conn4 = new SdpHeaderC( "IN", "IP4", localIpString );
			conn4->set_priority( m4->getPriority() );
			m4->setConnection( *conn4 );

			// IPv6
			m6->setPort( (*i)->getPort("IP6") );

			MRef<SdpHeaderA*> mid1 = new SdpHeaderA( "a=mid:1" );
			m6->addAttribute( mid1 );


			MRef<SdpHeaderC*> conn6 = new SdpHeaderC( "IN", "IP6", localIp6String );
			conn6->set_priority( m6->getPriority() );
			m6->setConnection( *conn6 );

			result->addHeader( *m6 );
		}
		else{
			string ipString;
			string addrtype;

			if( !localIpString.empty() ){
				ipString = localIpString;
				addrtype = "IP4";
			}
			else{
				ipString = localIp6String;
				addrtype = "IP6";
			}

			MRef<SdpHeaderC*> c = new SdpHeaderC("IN", addrtype, ipString );
			m->setConnection(c);
			m->setPort( (*i)->getPort( addrtype ) );
		}

	}
#ifdef DEBUG_OUTPUT	
	cerr << "Session::getSdpOffer: " << endl << result->getString() << endl << endl;
#endif

	return true;

}

bool Session::addReliableMediaToOffer(MRef<SdpPacket*> result, const string &peerUri, bool anatSupported){
	list< MRef<ReliableMediaStream*> >::iterator i;
	std::list<std::string>::iterator iAttribute;
	std::list<std::string> attributes;

	string type;
	uint16_t localPort = 0;
	MRef<SdpHeaderM *> m;
	std::list<MRef<Codec *> > codecs;
	std::list<MRef<Codec *> >::iterator iC;
//	uint8_t payloadType;
	string rtpmap;
//	bool anat = false;

	for( i = reliableMediaSessions.begin(); i != reliableMediaSessions.end(); i++ ){
		massert(*i);
		type = (*i)->getSdpMediaType();
		m = new SdpHeaderM( type, localPort, 1, (*i)->getTransport() );

		m->addFormat( (*i)->getMediaFormats() );
		result->addHeader( *m );

		attributes = (*i)->getSdpAttributes();
		for( iAttribute = attributes.begin(); iAttribute != attributes.end(); iAttribute ++ ){
			MRef<SdpHeaderA*> a = new SdpHeaderA("a=X");
			a->setAttributes( *iAttribute );
			m->addAttribute( *a );
		}

#if ADD_ANAT_HEADER_TO_RELIABLE_MEDIA_FIXME
		if( anat ){
			MRef<SdpHeaderM*> m4 = m;
			MRef<SdpHeaderM*> m6 = new SdpHeaderM( **m );
///////////
			// IPv4
			m4->setPort( (*i)->getPort("IP4") );

			MRef<SdpHeaderA*> mid2 = new SdpHeaderA( "a=mid:2" );

			m4->addAttribute( mid2 );

			MRef<SdpHeaderC*> conn4 = new SdpHeaderC( "IN", "IP4", localIpString );
			conn4->set_priority( m4->getPriority() );
			m4->setConnection( *conn4 );

			// IPv6
			m6->setPort( (*i)->getPort("IP6") );

			MRef<SdpHeaderA*> mid1 = new SdpHeaderA( "a=mid:1" );
			m6->addAttribute( mid1 );


			MRef<SdpHeaderC*> conn6 = new SdpHeaderC( "IN", "IP6", localIp6String );
			conn6->set_priority( m6->getPriority() );
			m6->setConnection( *conn6 );

			result->addHeader( *m6 );
		}
		else
#endif
		{
			string ipString;
			string addrtype;

			if( !localIpString.empty() ){
				ipString = localIpString;
				addrtype = "IP4";
			}
			else{
				ipString = localIp6String;
				addrtype = "IP6";
			}

			MRef<SdpHeaderC*> c = new SdpHeaderC("IN", addrtype, ipString );
			m->setConnection(c);

			m->setPort( (*i)->getPort( addrtype ) );
		}

	}

#ifdef DEBUG_OUTPUT	
	cerr << "Session::getSdpOffer: " << endl << result->getString() << endl << endl;
#endif

	return true;
}



MRef<SdpPacket *> Session::getSdpOffer( const string &peerUri, bool anatSupported ){ // used by the initiator when creating the first message
	string keyMgmtMessage;
	const char *transport = NULL;
	MRef<SdpPacket *> result;

	result = emptySdp();

	if( identity->securityEnabled ){
		int type = 0;

		// FIXME
		switch( ka_type ){
			case KEY_MGMT_METHOD_MIKEY_DH:
				type = KEY_AGREEMENT_TYPE_DH;
				break;
			case KEY_MGMT_METHOD_MIKEY_PSK:
				type = KEY_AGREEMENT_TYPE_PSK;
				break;
			case KEY_MGMT_METHOD_MIKEY_PK:
				type = KEY_AGREEMENT_TYPE_PK;
				break;
			case KEY_MGMT_METHOD_MIKEY_DHHMAC:
				type = KEY_AGREEMENT_TYPE_DHHMAC;
				break;
			case KEY_MGMT_METHOD_MIKEY_RSA_R:
				type = KEY_AGREEMENT_TYPE_RSA_R;
				break;
			default:
				mikey = NULL;
				return false;
		}

		MRef<SdpHeaderA *> a;
		MikeyConfig *config = new MikeyConfig( identity );
		// FIXME free config
		mikey = new Mikey( config );

		addStreams();

		keyMgmtMessage = mikey->initiatorCreate( type, peerUri );
		if( mikey->error() ){
			// something went wrong
			return false;
		}
		result->setSessionLevelAttribute( "key-mgmt", keyMgmtMessage );
		transport = "RTP/SAVP";
	}
	else{
		transport = "RTP/AVP";
	}

	if (!addRealtimeMediaToOffer(result, peerUri, anatSupported, transport)){
		return NULL;
	}
	if (!addReliableMediaToOffer(result, peerUri, anatSupported) ){
		return NULL;
	}

	return result;
}

bool Session::setSdpAnswer( MRef<SdpPacket *> answer, string peerUri ){
	unsigned int i;
	int j;
	MRef<RealtimeMediaStreamReceiver *> receiver;
	bool found = false;

	this->peerUri = peerUri;
#ifdef DEBUG_OUTPUT
// 	cerr << "Session::setSdpAnswer" << endl;
#endif
	if( mikey ){
		/* get the keymgt: attribute */
		string keyMgmtMessage = 
			answer->getSessionLevelAttribute( "key-mgmt" );

		if( !peerUri.empty() ){
			mikey->getKeyAgreement()->setPeerUri( peerUri );
		}

		if( !mikey->initiatorAuthenticate( keyMgmtMessage ) ){
			errorString = "Could not authenticate the key management message";
			fprintf( stderr, "Auth failed\n");
			return false;
		}
		this->peerUri = mikey->peerUri();

		string mikeyErrorMsg = mikey->initiatorParse();
		if( mikeyErrorMsg != "" ){
			errorString = "Could not parse the key management message. ";
			errorString += mikeyErrorMsg;
			fprintf( stderr, "Parse failed\n");
			return false;
		}

		/*
		else{
			if( ka && ka->type() == KEY_AGREEMENT_TYPE_DH ){
				((KeyAgreementDH *)*ka)->computeTgk();
			}
		}
		*/
	}

	string group = answer->getSessionLevelAttribute("group");
	string selectedId;

	if( !group.empty() )
		selectedId = matchAnat(group, answer, localIpString, localIp6String);
#ifdef DEBUG_OUTPUT	
	cout << "Selected:" << selectedId << endl;
#endif

	MRef<SdpHeaderC*> sessionConn = answer->getSessionLevelConnection();

	for( i = 0; i < answer->getHeaders().size(); i++ ){
		if( answer->getHeaders()[i]->getType() == SDP_HEADER_TYPE_M ){
			MRef<SdpHeaderM *> m = ((SdpHeaderM*)*(answer->getHeaders()[i]));
#ifdef DEBUG_OUTPUT
			cerr << "Session::setSdpAnswer - trying media line " << m->getString() << endl;
#endif
			
			if( !group.empty() ){
#ifdef DEBUG_OUTPUT
				cout << "Media " << i << endl;
#endif
				const string &id = m->getAttribute("mid", 0);
#ifdef DEBUG_OUTPUT
				cout << "id: " << id << endl;
#endif
				if( id != selectedId ){
#ifdef DEBUG_OUTPUT
					cerr << "Skip unselected id:" << id << endl;
#endif
					continue;
				}
			}

			MRef<IPAddress *> remoteAddress;
			MRef<SdpHeaderC *> c = m->getConnection();
			if( !c )
				c = sessionConn;

			if( !c ){
				cerr << "Session::setSdpAnswer - skip missing connection" << endl;
				continue;
			}

			remoteAddress = c->getIPAdress();

			for( j = 0; j < m->getNrFormats(); j++ ){
				receiver = matchFormat( m, j, remoteAddress );

				if( !receiver )
					continue;
#ifdef DEBUG_OUTPUT
				cerr << "Session::setSdpAnswer - Found receiver at " << remoteAddress->getString() << endl;
				cerr << "Receiver found: " << !!receiver << endl;
#endif

				if( receiver && m->getPort() == 0 ){
					/* This offer was rejected */
					receiver->disabled = true;
					// FIXME close socket
				}
				else{
					/* Be ready to receive */
					receiver->start();
					found = true;
				}
			}
		}
	}
	return found;
}

MRef<RealtimeMediaStreamReceiver *> Session::matchFormat(MRef<SdpHeaderM *> m,
		uint32_t iFormat, MRef<IPAddress *> &remoteAddress) {
	list<MRef<RealtimeMediaStreamSender *> >::iterator iSStream;
	list<MRef<RealtimeMediaStreamReceiver *> >::iterator iRStream;

	/* If we have a sender for this format, activate it */
#ifdef DEBUG_OUTPUT
	mdbg("media") << "Session::matchFormat: Starting senders loop" << endl;
#endif
	uint8_t j = 1;
	realtimeMediaStreamSendersLock.lock();
	for (iSStream = realtimeMediaStreamSenders.begin(); iSStream
			!= realtimeMediaStreamSenders.end(); iSStream++, j++) {
#ifdef DEBUG_OUTPUT
		mdbg("media") << "Trying a sender"<< endl;
#endif
		if ((*iSStream)->matches(m, iFormat)) {
#ifdef DEBUG_OUTPUT
			mdbg("media") << "Found sender for " << (*iSStream)->getSdpMediaType()<< endl;
#endif

#if 0
			if( ka ) {
				ka->addSrtpStream( (*iStream)->getSsrc(),
						0, /*ROC */
						0, /* policy (fix me) */
						2*j/* CSID */
				);
			}
#endif

#ifdef DEBUG_OUTPUT	
			cerr << "Set remote: " << remoteAddress->getString() << "," << m->getPort() << endl;
#endif
			///////////////////////////////////////////////

			cout
					<< "--------------------------------------------------Match Format ------------------------\n";
			cout
					<< "-------- Storing destination's ip and port. Is it correct to do it here ?? ------------\n";
			cout << "destinationIp and port " << remoteAddress->getString()
					<< "," << m->getPort() << "\n";
			cout << "Stream Sender type ::: " << (*iSStream)->getSdpMediaType()
					<< endl;
			cout
					<< "------------------------------------------------------------------------------------\n";
			if ((*iSStream)->getSdpMediaType().compare("video") == 0) {
				this->setDestinationPort((uint16_t) m->getPort());
				this->setDestinationIp(remoteAddress);
				string fmtpParam = m->getFmtpParam(m->getFormat(iFormat));
				size_t pos = fmtpParam.find('=');

				cerr<<"TTA fmtpparam = "<<fmtpParam<<" substring = "<< fmtpParam.substr(pos + 1, 2)<<endl;
				if (fmtpParam.substr(pos + 1, 2).compare("42") == 0)
				{
					cerr<<"TTA: Setting Baseline profile"<<endl;
					this->profile = 0;
					this->sendingWidth = 1024;
					this->sendingHeight = 720;
				}
				else
				{
					this->profile = 1;
					this->sendingWidth = 1280;
					this->sendingHeight = 720;
				}
			}
			(*iSStream)->setPort((uint16_t) m->getPort());
			(*iSStream)->setRemoteAddress(remoteAddress);
		}
	}
	realtimeMediaStreamSendersLock.unlock();
	/* Look for a receiver */
#ifdef DEBUG_OUTPUT
	mdbg("media") << "Starting receivers loop"<< endl;
#endif
	for (iRStream = realtimeMediaStreamReceivers.begin(); iRStream
			!= realtimeMediaStreamReceivers.end(); iRStream++) {
		if ((*iRStream)->matches(m, iFormat)) {
#ifdef DEBUG_OUTPUT
			mdbg("media") << "Found receiver for " << (*iRStream)->getSdpMediaType()<< endl;
#endif
			return (*iRStream);
		}
	}

	return NULL;
}

bool Session::setSdpOffer( MRef<SdpPacket *> offer, string peerUri ){ // used by the responder when receiving the first message
	unsigned int i;
	int j;
	MRef<RealtimeMediaStreamReceiver *> receiver;
	MRef<SdpPacket *> packet;
	string keyMgmtMessage;
	std::list<std::string>::iterator iAttribute;
	std::list<std::string> attributes;
	std::list<MRef<Codec *> > codecs;
	std::list<MRef<Codec *> >::iterator iC;
// 	uint8_t payloadType;
	string rtpmap;
	bool found = false;

	this->peerUri = peerUri;
// 	cerr << "Session::setSdpOffer" << endl;

	keyMgmtMessage = offer->getSessionLevelAttribute( "key-mgmt" );

	if( keyMgmtMessage != "" ){
		MikeyConfig *config = new MikeyConfig( identity );
		// FIXME free config
		mikey = new Mikey( config );

		addStreams(); //TODO: This only adds SRTP streams, no reliable media is handled.

		if( !mikey->responderAuthenticate( keyMgmtMessage, peerUri ) ){
			errorString =  "Incoming key management message could not be authenticated";
// 			if( ka ){
				errorString += mikey->authError();
// 			}
			return false;
		}
		this->peerUri = mikey->peerUri();
		//Here we set the offer in ka
		mikey->setMikeyOffer();
	}
	else{
		/*securityConfig.*/ka_type = KEY_MGMT_METHOD_NULL;
	}
		

	sdpAnswer = emptySdp();

	string group = offer->getSessionLevelAttribute("group");
	string selectedId;

	if( !group.empty() ){
		sdpAnswer->setSessionLevelAttribute("group", group);

		selectedId = matchAnat(group, offer, localIpString, localIp6String);
	}

#ifdef DEBUG_OUTPUT	
	cout << "Get remote addr " << endl;
#endif
	MRef<SdpHeaderC*> sessionConn = offer->getSessionLevelConnection();

#ifdef DEBUG_OUTPUT	
	cout << "Built empty sdp" << endl;
#endif

	for( i = 0; i < offer->getHeaders().size(); i++ ){
#ifdef DEBUG_OUTPUT	
		cout << "Header " << i << endl;
#endif

		if( offer->getHeaders()[i]->getType() == SDP_HEADER_TYPE_M ){
			MRef<SdpHeaderM *> offerM = (SdpHeaderM*)*(offer->getHeaders()[i]);

			MRef<SdpHeaderM *> answerM = new SdpHeaderM(
					offerM->getMedia(), 0, 0,
					offerM->getTransport() );

			sdpAnswer->addHeader( *answerM );

			if( !group.empty() ){
#ifdef DEBUG_OUTPUT	
				cout << "Media " << i << endl;
#endif
				const string &id = offerM->getAttribute("mid", 0);
				if(!id.empty())
					answerM->addAttribute(new SdpHeaderA("a=mid:" + id));

#ifdef DEBUG_OUTPUT	
				cout << "id: " << id << endl;
#endif
				if( id != selectedId )
					continue;
			}

			const string &transport = offerM->getTransport();

			if (transport != "RTP/AVP" &&
			    !identity->securityEnabled &&
			    transport == "RTP/SAVP") {
				errorString += "No supported SRTP key exchange method";
				return false;
			}

			MRef<SdpHeaderC *> c = offerM->getConnection();
			MRef<IPAddress *> remoteAddress;
			string addrString;

			if( !c )
				c = sessionConn;

			if( !c )
				continue;

			if ( c->getNetType() != "IN" )
				continue;

			if( c->getAddrType() == "IP4" ){
				if( localIpString.empty() )
					continue;
				addrString = localIpString;
			}
			else if( c->getAddrType() == "IP6" ){
				if( localIp6String.empty() )
					continue;
				addrString = localIp6String;
			}

			remoteAddress = c->getIPAdress();
			answerM->setConnection( new SdpHeaderC("IN", c->getAddrType(), addrString ));

			for( j = 0; j < offerM->getNrFormats(); j++ ){
				receiver = matchFormat( offerM, j, remoteAddress );

				if( receiver ){
					if( answerM->getPort() == 0 ){
						answerM->setPort( receiver->getPort( c->getAddrType() ) );
					}
					else{
						/* This media has already been treated */
						continue;
					}
					
					/* found a receiver, accept the offer */
					//add the payload type to the offer, as accepted ...
					string payloadTypeAccepted;
					if(receiver->getMedia()->getSdpMediaType() == "video"){
						payloadTypeAccepted = "" + itoa(receiver->getMedia()->getCodecInstance()->getSdpMediaType());
					}
					else
						payloadTypeAccepted = offerM->getFormat( j );

					cerr<<"TTA:Payloadtype accepted = "<<payloadTypeAccepted<<endl<<endl;
					//string payloadStr = itoa( payloadTypeAccepted );
					answerM->addFormat( payloadTypeAccepted );
					MRef<SdpHeaderA*> rtpmap = new SdpHeaderA("a=X");
					MRef<SdpHeaderA*> fmtp = new SdpHeaderA("a=X");

					fmtp->setAttributes( "fmtp:" + /*payloadStr*/ payloadTypeAccepted
								 + " " + offerM->getFmtpParam( offerM->getFormat(j) ) );
					rtpmap->setAttributes(   "rtpmap:" + /*payloadStr*/ payloadTypeAccepted
								+ " " + offerM->getRtpMap( offerM->getFormat(j)) );
					
					answerM->addAttribute( *rtpmap );
					answerM->addAttribute( *fmtp );
					
					/* Additional attributes (framesize, ...) */
					attributes = receiver->getSdpAttributes();
					for( iAttribute = attributes.begin(); iAttribute != attributes.end(); iAttribute ++ ){
						MRef<SdpHeaderA*> a = new SdpHeaderA("a=X");
						a->setAttributes( *iAttribute );
						answerM->addAttribute( *a );
					}

					found = true;
					break;
				}else{
					//cerr << "EEEE: did not find receiver!"<<endl;
				}
			}
		}
	}
	return found;
}

MRef<SdpPacket *> Session::getSdpAnswer(){
// 	cerr << "Session::getSdpAnswer" << endl;
	if( mikey ){
		string keyMgmtAnswer;
		// Generate the key management answer message
		keyMgmtAnswer = mikey->responderParse();
		
		if( mikey->error() ){
			// Something went wrong
			errorString = "Could not parse key management message.";
			fprintf(stderr, "responderParse failed\n" );
			return NULL;
		}
		/*
		else{
			if( ka && ka->type() == KEY_AGREEMENT_TYPE_DH ){
				((KeyAgreementDH *)*ka)->computeTgk();
			}
		}
		*/
		
		sdpAnswer->setSessionLevelAttribute( "key-mgmt", "mikey "+keyMgmtAnswer );
	}
	
	return sdpAnswer;
}

void Session::start(){
	if (started)
		return;
	started=true;
	
	list< MRef<RealtimeMediaStreamSender * > >::iterator iS;
	list< MRef<RealtimeMediaStreamReceiver * > >::iterator iR;

	MRef<KeyAgreement*> ka;

	if( isSecure() ){
		ka = mikey->getKeyAgreement();
	}

	if( ka ){
#ifdef ENABLE_TS
	ts.save( TGK_START );
#endif
	KeyAgreementDHBase *kaDH =
	  dynamic_cast<KeyAgreementDHBase*>(*ka);
	if( kaDH ){
		kaDH->computeTgk();
	}
#ifdef ENABLE_TS
	ts.save( TGK_END );
#endif
	}

	for( iR = realtimeMediaStreamReceivers.begin(); iR != realtimeMediaStreamReceivers.end(); iR++ ){
		if( ! (*iR)->disabled ){
			if( ka ){
				(*iR)->setKeyAgreement( ka );
				(*iR)->flushCryptoContexts();
			}
			(*iR)->start();
		}else{
		}
	}
	
	realtimeMediaStreamSendersLock.lock();
	for( iS = realtimeMediaStreamSenders.begin(); iS != realtimeMediaStreamSenders.end(); iS++ ){
		if( (*iS)->getPort() ){
			if( ka ){
				(*iS)->setKeyAgreement( ka );
			}
			(*iS)->start();
		}
	}
	/*	if( callRecorder ) {
		MRef<CallRecorder *> cr = dynamic_cast<CallRecorder *>(*callRecorder);
		cr->setEnabled( f
	}*/
	realtimeMediaStreamSendersLock.unlock();
}

void Session::stop(){
	cerr <<"ZZZZ: doing Session::stop"<<endl;
	started=false;
	list< MRef<RealtimeMediaStreamSender * > >::iterator iS;
	list< MRef<RealtimeMediaStreamReceiver * > >::iterator iR;

	cerr <<"ZZZZ: Session::stop stopping all receivers"<<endl;
	for( iR = realtimeMediaStreamReceivers.begin(); iR != realtimeMediaStreamReceivers.end(); iR++ ){
		if( ! (*iR)->disabled ){
			(*iR)->stop();
		}
	}
	
	cerr <<"ZZZZ: Session::stop stopping all senders"<<endl;
	realtimeMediaStreamSendersLock.lock();
	for( iS = realtimeMediaStreamSenders.begin(); iS != realtimeMediaStreamSenders.end(); iS++ ){
		if( (*iS)->getPort() ){
			(*iS)->stop();
		}
	}
	cerr <<"ZZZZ: Session::stop done stopping all senders"<<endl;
	MRef<CallRecorder *> cr;
	if (callRecorder)
		cr = dynamic_cast<CallRecorder *>(*callRecorder);
	if( !cr ) {
	#ifdef DEBUG_OUTPUT
		cerr << "Session::stop - no call recorder?" << endl;
	#endif
	} else {
		cr->setAllowStart( false );
		cr->free();
	}
	callRecorder = NULL; //stop the call recorder object

	realtimeMediaStreamSendersLock.unlock();

}


void Session::addRealtimeMediaStreamReceiver( MRef<RealtimeMediaStreamReceiver *> realtimeMediaStream ){
	realtimeMediaStreamReceivers.push_back( *realtimeMediaStream );
	silenceSources( silencedSources );
}

void Session::addRealtimeMediaStreamSender( MRef<RealtimeMediaStreamSender *> realtimeMediaStream ){
	realtimeMediaStreamSendersLock.lock();
	realtimeMediaStream->setMuted( mutedSenders );
	realtimeMediaStreamSenders.push_back( *realtimeMediaStream );
	realtimeMediaStreamSendersLock.unlock();
}


void Session::removeRealtimeMediaStreamSender( MRef<RealtimeMediaStreamSender *> realtimeMediaStream ){
        realtimeMediaStreamSendersLock.lock();
        realtimeMediaStreamSenders.remove( realtimeMediaStream );
        realtimeMediaStreamSendersLock.unlock();
}

string Session::getErrorString(){
	return errorString;
}

uint16_t Session::getErrorCode(){
	return errorCode;
}

bool Session::isSecure(){
	// 	return /*securityConfig.*/secured;
	return mikey && mikey->isSecured();
}

string Session::getCallId(){
	return callId;
}

void Session::setCallId( const string callId ){
	this->callId = callId;
#ifdef ZRTP_SUPPORT
	realtimeMediaStreamSendersLock.lock();
	for ( std::list< MRef<RealtimeMediaStreamSender *> >::iterator it =  realtimeMediaStreamSenders.begin();
                     it !=  realtimeMediaStreamSenders.end(); it++ ) { // TODO - need better support to set call id in ZHB
                        MRef<ZrtpHostBridgeMinisip*> zhb = (*it)->getZrtpHostBridge();
                        if (zhb) {
                            zhb->setCallId(callId);
                        }
        }   
        realtimeMediaStreamSendersLock.unlock();

#endif
}

void Session::sendDtmf( uint8_t symbol ){
	MRef<DtmfSender *> dtmfSender = new DtmfSender( this );
	uint32_t * ts = new uint32_t;
	*ts = 0;
	dtmfTOProvider->requestTimeout( 0, dtmfSender, new DtmfEvent( symbol, 10, 0, false, true, ts ) );
	dtmfTOProvider->requestTimeout( 5, dtmfSender, new DtmfEvent( symbol, 10, 0, false, false, ts ) );
	dtmfTOProvider->requestTimeout( 10, dtmfSender, new DtmfEvent( symbol, 10, 0, false, false, ts ) );

	dtmfTOProvider->requestTimeout( 15, dtmfSender, new DtmfEvent( symbol, 10, 800, true, false, ts ) );
	dtmfTOProvider->requestTimeout( 20, dtmfSender, new DtmfEvent( symbol, 10, 800, true, false, ts ) );
	dtmfTOProvider->requestTimeout( 25, dtmfSender, new DtmfEvent( symbol, 10, 800, true, false, ts, true ) );
	
}

void Session::muteSenders (bool mute) {
	mutedSenders = mute;
	realtimeMediaStreamSendersLock.lock();
	for( std::list< MRef<RealtimeMediaStreamSender *> >::iterator it =  realtimeMediaStreamSenders.begin();
				it !=  realtimeMediaStreamSenders.end(); it++ ) {
		(*it)->setMuted( mute );
	}

	MRef<CallRecorder *> cr;
	if (callRecorder){
		cr = dynamic_cast<CallRecorder *>(*callRecorder);
	}

	if( !cr ) {
	#ifdef DEBUG_OUTPUT
		cerr << "Session::muteSenders - no call recorder?" << endl;
	#endif
	} else {
		cr->setEnabledMic( !mute );
	}

	realtimeMediaStreamSendersLock.unlock();
}

void Session::silenceSources ( bool silence ) {
#ifdef DEBUG_OUTPUT
/*	if( silence )
		cerr << "Session::SilenceSources - true" << endl;
	else 
		cerr << "Session::SilenceSources - false" << endl;
*/
#endif
	silencedSources = silence;
	for( std::list< MRef<RealtimeMediaStreamReceiver *> >::iterator it =  realtimeMediaStreamReceivers.begin();
				it !=  realtimeMediaStreamReceivers.end(); it++ ) {
		list<uint32_t> ssrcList;
		list<uint32_t>::iterator ssrcIt;
		MRef<AudioMedia *> audioMedia;
		MRef<AudioMediaSource *> audioSource;
		
		//obtain the media object used by the media stream, and try to cast it to
		//an audiomedia ... 
		audioMedia = dynamic_cast<AudioMedia *>( *( (*it)->getMedia() ) );
		//if it is not audiomedia, we are not interested
		if( !audioMedia ) {
			continue;
		}
		
		ssrcList = (*it)->getSsrcList();
		
		for( ssrcIt = ssrcList.begin(); ssrcIt != ssrcList.end(); ssrcIt++ ) {
			audioSource = audioMedia->getSource( *ssrcIt );
			if( !audioSource ) {
// 				cerr << "Session::SilenceSources - skipping ssrc ... no source found" << endl;
				continue;
			} else {
				audioSource->setSilenced( silence );
// 				cerr << "Session::SilenceSources - silencing source " << itoa(*ssrcIt) << endl;
			}
		}
	}

	if( callRecorder ) {
		MRef<CallRecorder *> cr = dynamic_cast<CallRecorder *>(*callRecorder);
		if( !cr ) {
		#ifdef DEBUG_OUTPUT
			cerr << "Session::silenceSources - no call recorder? (1)" << endl;
		#endif
		} else {
			cr->setEnabledNetwork( !silence );
		}
	} else {
		#ifdef DEBUG_OUTPUT
			cerr << "Session::silenceSources - no call recorder? (2)" << endl;
		#endif
	}
}

#ifdef DEBUG_OUTPUT
string Session::getDebugString() {
	string ret;
	ret = getMemObjectType() + ": this=" + itoa(reinterpret_cast<int64_t>(this)) +
		"\n         ; callid=" + getCallId() +
		"; peerUri=" + peerUri;
		
	ret += "\n          ";
	if( mutedSenders )
		ret += "; mutedSenders = true";
	else 
		ret += "; mutedSenders = false";	
	
	ret += "\n          ";
	if( silencedSources )
		ret += "; silencedSources = true";
	else 
		ret += "; silencedSources = false";	

	if( callRecorder ) {
		MRef<CallRecorder *> cr = dynamic_cast<CallRecorder *>(*callRecorder);
		ret += "\n          ";
		cr = dynamic_cast<CallRecorder *>( *callRecorder );
		ret += "; " + cr->getDebugString();
	}else
		ret += "\n          (no call recorder)";

	for( std::list< MRef<RealtimeMediaStreamReceiver *> >::iterator it = realtimeMediaStreamReceivers.begin();
				it != realtimeMediaStreamReceivers.end(); it++ ) {
		ret += "\n          " + (*it)->getDebugString();
	}
	for( std::list< MRef<RealtimeMediaStreamSender *> >::iterator it2 =  realtimeMediaStreamSenders.begin();
				it2 !=  realtimeMediaStreamSenders.end(); it2++ ) {
		ret += "\n          " + (*it2)->getDebugString();
	}
	return ret;
}
#endif

void Session::clearRealtimeMediaStreamReceivers() {
	realtimeMediaStreamReceivers.clear();
}

const std::string &Session::getPeerUri() const{
	return peerUri;
}

void Session::addStreams() {
	RealtimeMediaStreamSenders::iterator i;
	RealtimeMediaStreamSenders::iterator last = realtimeMediaStreamSenders.end();

	for( i = realtimeMediaStreamSenders.begin(); i != last; i++ ){
		uint32_t ssrc = (*i)->getSsrc();
		mikey->addSender( ssrc );
	}
}
