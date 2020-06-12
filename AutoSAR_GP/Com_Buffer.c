#include "Std_Types.h"
#include "Com.h"
#include "Com_helper.h"
#include "Com_Buffer.h"


static void Com_WriteSignalDataToPduBuffer(const ComSignal_type* signal);

extern const ComIPdu_type   ComIPdus[];
extern const ComSignal_type ComSignals[];

void Com_PackSignalsToPdu(uint16 ComIPuId)
{
    uint8 signalIndex;
    const ComIPdu_type *IPdu;

    IPdu = GET_IPdu(ComIPuId);
    for( signalIndex = 0 ; signalIndex < IPdu->ComIPduNumOfSignals ; signalIndex++ )
    {
        Com_WriteSignalDataToPduBuffer(IPdu->ComIPduSignalRef[signalIndex]);
    }
}

void Com_PduUnpacking(PduIdType ComIPuId)
{
    uint8 signalIndex;
    for ( signalIndex = 0; (ComIPdus[ComIPuId].ComIPduNumOfSignal > signalIndex); signalIndex++)
    {
        if((ComIPdus[ComRxPduId].ComIPduSignalRef)[signalIndex].ComUpdateBitEnabled)
        {
            if (CHECKBIT(ComIPdus[ComRxPduId].ComIPduDataPtr, (ComIPdus[ComRxPduId].ComIPduSignalRef)[signalIndex].ComUpdateBitPosition))
            {
                Com_ReadSignalDataFromPduBuffer(ComRxPduId,&ComIPdus[ComRxPduId].ComIPduSignalRef)[signalIndex]);
                if (ComIPdus[ComRxPduId].ComIPduSignalRef->ComNotification != NULL_PTR)
                {
                    ComIPdus[ComRxPduId].ComIPduSignalRef->ComNotification();
                }
                else
                {
                    /* Following MISRA rules */
                }
            }
        }
        else
        {
            /* Following MISRA rules */
        }
    }
    else
    {
        Com_ReadSignalDataFromPduBuffer(ComRxPduId,&ComIPdus[ComRxPduId].ComIPduSignalRef)[signalIndex]);
        /* If signal processing mode is IMMEDIATE, notify the signal callback. */
        if(ComIPdus[ComIPuId].ComIPduSignalProcessing == IMMEDIATE)
        {
            if (ComIPdus[ComRxPduId].ComIPduSignalRef->ComNotification != NULL_PTR)
            {
                ComIPdus[ComRxPduId].ComIPduSignalRef->ComNotification();
            }
            else
            {
                /* Following MISRA rules */
            }
        }
    }
}
}


void Com_WriteSignalDataToPduBuffer(const ComSignal_type* signal)
{
    uint8*  pdu;
    uint64  mask;
    uint8   position;
    uint8   length;

    pdu=ComIPdus[signal->ComIPduHandleId].ComIPduDataPtr;
    if(signal->ComSignalType==UINT8_N)/*TODO:check UINT8_DYN*/
    {
        memcpy(pdu+(signal->ComBitPosition/8), signal->ComSignalDataPtr,signal->ComSignalLength);
    }
    else
    {
        mask=0xffffffffffffffff;
        position=signal->ComBitPosition;
        length=signal->ComSignalLength;
        mask=mask<<(64 - (position+length));
        mask=mask>>(64 - (length));
        mask=mask<<position;
        (*((uint64*)pdu))&=~mask;
        (*((uint64*)pdu))|=((*((uint64*)signal->ComSignalDataPtr))<<position)&mask;
    }

}



void Com_ReadSignalDataFromPduBuffer(PduIdType ComRxPduId, ComSignal_type* SignalRef)
{
    /*TODO: add the sequence of the TP case (for UINT8_N and UINT8_DYN)
      TODO: handle the case of the size more than 8 Bytes*/

    uint8 signalLength;
    uint8 startBit;
    uint64 Data;

    Data = *((uint64*)(ComIPdus[ComRxPduId].ComIPduDataPtr));
    startBit = SignalRef->ComBitPosition;
    signalLength = SignalRef->ComBitSize;
    Data = Data << (64 - (startBit+signalLength));
    Data = Data >> (64 - (signalLength));

    switch(SignalRef->ComSignalType)
    {
    case BOOLEAN:
        *((uint8)SignalRef->ComSignalDataPtr)=(uint8)Data;
        break;
    case FLOAT32:
        *((float32)SignalRef->ComSignalDataPtr)=(float32)Data;
        break;
    case FLOAT64:
        *((float64)SignalRef->ComSignalDataPtr)=(float64)Data;
        break;
    case UINT8:
        *((uint8)SignalRef->ComSignalDataPtr)=(uint8)Data;
        break;
    case UINT16:
        *((uint16)SignalRef->ComSignalDataPtr)=(uint16)Data;
        break;
    case UINT32:
        *((uint32)SignalRef->ComSignalDataPtr)=(uint32)Data;
        break;
    case UINT8_N:
        *((uint8)SignalRef->ComSignalDataPtr)=(uint8)Data;
        break;
    case UINT8_DYN:
        *((uint8)SignalRef->ComSignalDataPtr)=(uint8)Data;
        break;
    case SINT8:
        *((sint8)SignalRef->ComSignalDataPtr)=(sint8)Data;
        break;
    case SINT16:
        *((sint16)SignalRef->ComSignalDataPtr)=(sint16)Data;
        break;
    case SINT32:
        *((sint32)SignalRef->ComSignalDataPtr)=(sint32)Data;
        break;
    case SINT64:
        *((sint64)SignalRef->ComSignalDataPtr)=(sint64)Data;
        break;
    case UINT64:
        *((uint64)SignalRef->ComSignalDataPtr)=(uint64)Data;
        break;
    }
}



void Com_WriteSignalDataToSignalBuffer (const uint16 signalId, const void * signalData)
{
    const ComSignal_type * Signal;
    uint8 mod;

    Signal =  GET_Signal(signalId);
    if(Signal->ComSignalType==UINT8_N)
    {
        /*TODO:UINT8_DYN*/
        memcpy(Signal->ComSignalDataPtr, signalData, Signal->ComSignalLength);
    }
    else
    {
        if(Signal->ComBitSize%8)
        {
            mod=1;
        }
        else
        {
            mod=0;
        }
        memcpy(Signal->ComSignalDataPtr, signalData, Signal->ComBitSize/8+mod);
    }
}


void Com_ReadSignalDataFromSignalBuffer (const uint16 signalId,  void * signalData)
{
    uint8 Size;

    /*TODO: add UINT8_DYN*/
    if(ComSignals[signalId].ComSignalType==UINT8_N)
    {
        memcpy(signalData, ComSignals[signalId].ComSignalDataPtr,ComSignals[signalId].ComSignalLength);
    }
    else
    {
        Size=ComSignals[signalId].ComBitSize/8;
        if(ComSignals[signalId].ComBitSize%8)
        {
            Size++;
        }
        memcpy(signalData, ComSignals[signalId].ComSignalDataPtr, Size);
    }
}
