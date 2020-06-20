#include "Std_Types.h"
#include "Com.h"
#include "Com_helper.h"

boolean validateSignalID (Com_SignalIdType SignalId)
{
	if(SignalId<(Com_SignalIdType)COM_NUM_OF_SIGNAL)
	{
		return (boolean)TRUE;
	}
	else
	{
	    return (boolean)FALSE;
	}
}


uint64 power(uint8 x,uint8 y)
{
	uint64 result = x;

	if (y == (uint8)0)
		return (uint64)1;

	for (; y>(uint8)1 ; y--)
	{
		result = result * x;
	}
	return result;
}
