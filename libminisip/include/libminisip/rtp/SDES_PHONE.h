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
*/

#ifndef SDES_PHONE_H
#define SDES_PHONE_H

#include<config.h>

#include<libminisip/rtp/SDESItem.h>
#include<string>


class SDES_PHONE : public SDESItem{
	public:
		SDES_PHONE(void *buildfrom, int max_length);
		virtual ~SDES_PHONE(){};

//		vector<unsigned char> get_bytes();
		int size();

#ifdef DEBUG_OUTPUT
		virtual void debug_print();
#endif

	private:
		std::string phone;
};

#endif
