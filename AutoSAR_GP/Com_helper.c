#include "Std_Types.h"
#include "Com.h"
#include "Com_helper.h"

boolean validateSignalID (Com_SignalIdType SignalId)
{
	if(SignalId<COM_NUM_OF_SIGNAL)
	{
		return TRUE;
	}
	else
	{
	    return FALSE;
	}
}


uint64 power(uint8 x,uint8 y)
{
	uint64 result = x;

	if (y == 0)
		return 1;

	for (; y>1 ; y--)
	{
		result = result * x;
	}
	return result;
}
