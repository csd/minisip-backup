/*
 Copyright (C) 2006 Werner Dittmann
 
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

/*
 * Authors: Werner Dittmann <Werner.Dittmann@t-online.de>
 */

#ifndef _ZRTPTEXTDATA_H_
#define _ZRTPTEXTDATA_H_

/**
 *
 */
extern char *clientId;
extern char *zrtpVersion;

/**
 *
 */
extern char *HelloMsg;
extern char *HelloAckMsg;
extern char *CommitMsg;
extern char *DHPart1Msg;
extern char *DHPart2Msg;
extern char *Confirm1Msg;
extern char *Confirm2Msg;
extern char *Conf2AckMsg;
extern char *ErrorMsg;

/**
 *
 */
extern char *responder;
extern char *initiator;
extern char *iniMasterKey;
extern char *iniMasterSalt;
extern char *respMasterKey;
extern char *respMasterSalt;

extern char *hmacKey;
extern char *retainedSec;
extern char *knownPlain;

extern char *sasString;
/**
 *
 */

// Keep the Hash identifers in supportedHashes in the same order than the
// following enum, starting with zero.
typedef enum  SupportedHashes {
    Sha256,
    NumSupportedHashes
};
extern char *supportedHashes[];

// Keep the Cipher identifers in supportedCipher in the same order than the
// following enum, starting with zero.
enum SupportedSymCiphers {
    Aes256,
    Aes128,
    NumSupportedSymCiphers
};
extern char *supportedCipher[];

// Keep the PubKey identifers in supportedPubKey in the same order than the
// following enum, starting with zero.
enum SupportedPubKeys {
    Dh4096,
    Dh3072,
    NumSupportedPubKeys
};
extern char *supportedPubKey[];

// Keep the SAS identifers in supportedSASType in the same order than the
// following enum, starting with zero.
enum SupportedSASTypes {
    Libase32,
    NumSupportedSASTypes
};
extern char *supportedSASType[];

#endif     // _ZRTPTEXTDATA_H_

