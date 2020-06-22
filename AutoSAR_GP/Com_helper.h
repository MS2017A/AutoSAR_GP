/***************************************************
 * File Name: Com_helper.h
 * Author: AUTOSAR COM Team
 * Date Created: Jun 2020
 * Version  : 4.0
 ****************************************************/
#ifndef COM_HELPER_H_
#define COM_HELPER_H_

// set bit in specific bit
#define SETBIT(dest,bit)	( *( (uint8 *)(dest)    + ((bit) / (uint32)8) ) |= (uint8)(1u << ((bit) % (uint32)8)) )

// clear bit in specific bit
#define CLEARBIT(dest,bit)	( *( (uint8 *)(dest)    + ((bit) / (uint32)8) ) &= (uint8)~(uint8)(1u << ((bit) % (uint32)8)) )

// get bit value
#define CHECKBIT(source,bit)	( *( (uint8 *)(source)  + ((bit) / (uint32)8) ) &  (uint8)(1u << ((bit) % (uint32)8)) )


#define GET_Signal(SignalId) \
	(&ComSignals[(SignalId)])


#define GET_IPdu(IPduId) \
	(&ComIPdus[(IPduId)])

boolean validateSignalID (Com_SignalIdType SignalId);

uint64 power(uint8 x,uint8 y);
//boolean compare_float(uint64 f1, uint64 f2);

#endif
