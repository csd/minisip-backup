/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* Copyright (C) 2004, 2005 
 *
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
*/

#ifndef MEDIA_STREAM_H
#define MEDIA_STREAM_H

#include<libmutil/MemObject.h>
#include"../rtp/CryptoContext.h"
#include"Media.h"
#include"MediaStream.h"
#include"Session.h"
#include"RtpReceiver.h"

class KeyAgreement;
class UDPSocket;
class SdpHeaderM;
class SRtpPacket;
class IpProvider;

class MediaStream : public MObject{
	public:
		virtual void start() = 0;
		virtual void stop() = 0;

#ifdef DEBUG_OUTPUT
		virtual string getDebugString();
#endif
		
		/* SDP information */
		std::string getSdpMediaType();/* audio, video, appli;ation... */
		std::list<std::string> getSdpAttributes();

		virtual std::string getMemObjectType(){return "MediaStream";}
		bool disabled;
		
		virtual void setPort( uint16_t port )=0;
		virtual uint16_t getPort()=0;

		bool matches( MRef<SdpHeaderM *> m, uint32_t nFormat=0 );
		void addToM( MRef<SdpPacket*> packet, MRef<SdpHeaderM *> m );

		void setKeyAgreement( MRef<KeyAgreement *> ka );

		uint32_t getSsrc();

		MRef<CodecState *> getSelectedCodec(){return selectedCodec;};
		
		//cesc
		void setMuted( bool mute ) { muted = mute;}
		bool isMuted() { return muted;}
		bool muteKeepAlive( uint32_t max);

	protected:
		MRef<CryptoContext *> getCryptoContext( uint32_t ssrc );
		MediaStream( MRef<Media *> );
		MRef<Media *> media;
		uint32_t csbId;
		uint32_t ssrc;
		// FIXME used only in sender case
		uint8_t payloadType;
		MRef<CodecState *> selectedCodec;
		
		//Cesc -- does it conflict with bool disabled???
		bool muted;
		uint32_t muteCounter;

	private:
		MRef<CryptoContext *> initCrypto( uint32_t ssrc );
		MRef<KeyAgreement *> ka;
		Mutex kaLock;
		std::list< MRef<CryptoContext *> > cryptoContexts;

};

class MediaStreamReceiver : public MediaStream{ 
	public:
		MediaStreamReceiver( MRef<Media *> media, MRef<RtpReceiver *>, MRef<IpProvider *> ipProvider );

#ifdef DEBUG_OUTPUT
		virtual string getDebugString();
#endif

		virtual std::string getMemObjectType(){return "MediaStreamReceiver";}
	
		virtual void start();
		virtual void stop();
		
		virtual void setPort( uint16_t port );
		virtual uint16_t getPort();

		void handleRtpPacket( SRtpPacket * packet );
		uint32_t getId();

		std::list<MRef<Codec *> > getAvailableCodecs();

	private:
		std::list<MRef<Codec *> > codecList;
		MRef<RtpReceiver *> rtpReceiver;
		uint32_t id;
		MRef<IpProvider *> ipProvider;
		uint16_t externalPort;

		void gotSsrc( uint32_t ssrc );

		std::list<uint32_t> ssrcList;
		Mutex ssrcListLock;

		bool running;
		
};

class MediaStreamSender : public MediaStream{ 
	public:
		MediaStreamSender( MRef<Media *> media, 
				   MRef<UDPSocket *> senderSock=NULL );

#ifdef DEBUG_OUTPUT
		virtual string getDebugString();
#endif
		
		virtual std::string getMemObjectType(){return "MediaStreamSender";}
		
		virtual void start();
		virtual void stop();

		virtual void setPort( uint16_t port );
		virtual uint16_t getPort();
		
		void send( byte_t * data, uint32_t length, uint32_t * ts, bool marker = false, bool dtmf = false );
		void setRemoteAddress( IPAddress * remoteAddress );
		
		
	private:
		MRef<UDPSocket *> senderSock;
		uint16_t remotePort;
		uint16_t seqNo;
		uint32_t lastTs;
		IPAddress * remoteAddress;
		Mutex senderLock;
		
};

#endif
