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
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
*/

#include<libminisip/codecs/Codec.h>
#include<libminisip/codecs/G711CODEC.h>
#include<libminisip/codecs/ILBCCODEC.h>
#include<libminisip/codecs/SPEEXCODEC.h>
#include<libminisip/codecs/GsmCodec.h>

MRef<CodecState *> AudioCodec::createState( uint8_t payloadType ){
        switch( payloadType ){
                case 0:
                        return new G711CodecState();
#ifdef HAS_GSM
		case 3:
			return new GsmCodecState();
#endif
		case 97:
			return new ILBCCodecState();
#ifdef HAS_SPEEX
		case 114: 
			return new SpeexCodecState();
#endif
                default:
                        return NULL;
        }
}

MRef<AudioCodec *> AudioCodec::create( const std::string & description ){
        if( description == "G.711" ){
                return new G711Codec();
        }
        
        if( description == "iLBC" ){
                return new ILBCCodec();
        }
        
#ifdef HAS_SPEEX
        if( description == "speex" ){
                return new SpeexCodec();
        }
#endif
#ifdef HAS_GSM
        if( description == "GSM" ){
                return new GsmCodec();
        }
#endif

        return NULL;
}
