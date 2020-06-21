#include "Std_Types.h"
#include "Com.h"
#include "Com_helper.h"

boolean validateSignalID (Com_SignalIdType SignalId)
{
    boolean result=TRUE;
	if(SignalId<(Com_SignalIdType)COM_NUM_OF_SIGNAL)
	{
		result = (boolean)TRUE;
	}
	else
	{
	    result = (boolean)FALSE;
	}
	return result;
}


uint64 power(uint8 x,uint8 y)
{
	uint64 result = x;

	if (y == (uint8)0)
	{
	    result = (uint64)1;
	}
	else
	{
	for (; y>(uint8)1 ; y--)
	{
		result = result * x;
	}
	}
	return result;
}
