/*
  Copyright (C) 2005, 2004 Erik Eliasson, Johan Bilien
  
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
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Authors: Erik Eliasson <eliasson@it.kth.se>
 *          Johan Bilien <jobi@via.ecp.fr>
*/


/* Name
 * 	split_in_lines.h
 * Author
 * 	Erik Eliasson, eliasson@it.kth.se, 2003
 * Purpose
 * 	Takes a String as argument and splits it at any new line.
*/

#ifndef SPLIT_IN_LINES
#define SPLIT_IN_LINES

#include<vector>
#include<string>

#include <libmutil/libmutil_config.h>

/**
 * Splits a string into multiple parts.
 *
 * @return	If s is an empty string the function will return an empty
 * 		vector (no matter if includeEmpty is true or false)
 */
LIBMUTIL_API std::vector<std::string> split(std::string s, bool do_trim=true, char delim='\n', bool includeEmpty=false);

/**
 * Splits a string on new line character ('\n').
 *
 * @return 	Empty lines are not included in the output. If do_trim is
 * 		true then lines containing only whitespace will not be
 * 		included in the output.
 * 		An empty input string returns an empty vector.
 */
LIBMUTIL_API std::vector<std::string> split_in_lines(std::string s, bool do_trim = true);


#endif
