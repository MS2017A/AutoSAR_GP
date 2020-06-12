
#include "Std_Types.h"
#include "Com_Asu_Types.h"
#include "Com_Types.h"
#include "Com_helper.h"
#include "Com_Cfg.h"
#include "Com_Buffer.h"


extern const ComConfig_type * ComConfig;

/* Com_Asu_Config declaration*/
extern Com_Asu_Config_type ComAsuConfiguration;
static Com_Asu_Config_type * Com_Asu_Config = &ComAsuConfiguration;


void Com_PackSignalsToPdu(PduIdType ComIPuId)
{
    uint8 signalID = 0;
    const ComIPdu_type *IPdu = GET_IPdu(ComIPuId);
    for ( signalID = 0; (IPdu->ComIPduSignalRef[signalID] != NULL_PTR); signalID++)
    {

        Com_WriteSignalDataToPduBuffer(IPdu->ComIPduSignalRef[signalID]->ComHandleId, IPdu->ComIPduSignalRef[signalID]->ComSignalDataPtr);
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
                else
                {
                    //TODO: must be edited
                    Asu_Signal->ComSignalUpdated=TRUE;
                }
            }
            else
            {
                /* Following MISRA rules */
            }
        }
        else
        {
            //TODO: must be edited
            Asu_Signal->ComSignalUpdated=TRUE;
        }
    }
}

void Com_WriteSignalDataToPduBuffer(const uint16 signalId, const void *signalData)
{
    uint32 bitPosition;
    uint8 data;
    uint8 mask;
    uint8 pduMask;
    uint8 signalMask;
    uint8 *pduBufferBytes = NULL_PTR;
    uint8 *pduBeforChange = NULL_PTR;
    uint8 *dataBytes = NULL_PTR;
    uint8 signalLength;
    uint8 BitOffsetInByte;
    uint8 pduStartByte;
    uint8 i;
    Com_Asu_IPdu_type *Asu_IPdu = NULL_PTR;



    const ComSignal_type * Signal =  GET_Signal(signalId);
    // Get PDU
    const ComIPdu_type *IPdu = GET_IPdu(Signal->ComIPduHandleId);
    void * const pduBuffer = IPdu->ComIPduDataPtr;

    bitPosition = Signal->ComBitPosition;
    BitOffsetInByte = bitPosition%8;
    pduStartByte = bitPosition / 8;
    pduBufferBytes = (uint8 *)pduBuffer;
    dataBytes = (uint8 *) signalData;
    signalLength = Signal->ComBitSize/8;
    pduBeforChange = pduBufferBytes;

    pduBufferBytes += pduStartByte;
    uint8 x;
    //TODO: using the approach of looping for u32
    //TODO: using switch case according to the size of the data
    for(i = 0; i<=signalLength; i++)
    {
        pduMask = 255;
        signalMask = 255;
        if( i == 0)
        {
            pduMask = pduMask >> (8 - BitOffsetInByte);
            signalMask = signalMask >> BitOffsetInByte;
            *pduBufferBytes = (* pduBufferBytes) & pduMask;
            data = (* dataBytes) & signalMask;
            data = data << BitOffsetInByte;
            *pduBufferBytes = (* pduBufferBytes) | data;
            x= *pduBufferBytes;
            pduBufferBytes ++;
        }
        else if(i==signalLength)
        {
            pduMask = pduMask << BitOffsetInByte;
            signalMask = signalMask << (8 - BitOffsetInByte);
            *pduBufferBytes = (* pduBufferBytes) & pduMask;
            data = (* dataBytes) & signalMask;
            data = data >> (8 - BitOffsetInByte);
            *pduBufferBytes = (* pduBufferBytes) | data;
            x= *pduBufferBytes;
        }
        else
        {
            //TODO: why using shifts
            pduMask = pduMask << BitOffsetInByte;
            signalMask = signalMask << (8 - BitOffsetInByte);
            *pduBufferBytes = (* pduBufferBytes) & pduMask;
            data = (* dataBytes) & signalMask;
            data = data >> (8 - BitOffsetInByte);
            *pduBufferBytes = (* pduBufferBytes) | data;

            dataBytes++;

            pduMask = 255;
            signalMask = 255;
            pduMask = pduMask >> (8 - BitOffsetInByte);
            signalMask = signalMask >> BitOffsetInByte;
            *pduBufferBytes = (* pduBufferBytes) & pduMask;
            data = (* dataBytes) & signalMask;
            data = data << BitOffsetInByte;
            *pduBufferBytes = (* pduBufferBytes) | data;
            x= *pduBufferBytes;
            pduBufferBytes ++;

        }
    }
}

void Com_ReadSignalDataFromPduBuffer(PduIdType ComRxPduId, ComSignal_type* SignalRef)
{
    //TODO: add the sequence of the TP case (for UINT8_N and UINT8_DYN)
    //TODO: handle the case of the size more than 8 Bytes

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

void Com_WriteSignalDataToSignalBuffer (uint16 signalId, const void * signalData)
{
    uint8 Size;
    if(ComSignals[signalId].ComSignalType == )
    {
    }
    else
    {
        Size=ComSignals[signalId].ComBitSize/8;
        if(ComSignals[signalId].ComBitSize%8)
        {
            Size++;
        }
        memcpy(ComSignals[signalId].ComSignalDataPtr, signalData, Size);
    }
}

void Com_ReadSignalDataFromSignalBuffer (uint16 signalId,  void * signalData)
{
    uint8 Size;
    //TODO: complete the if condition from Jo typing
    if(ComSignals[signalId].ComSignalType == )
    {
    }
    else
    {
        Size=ComSignals[signalId].ComBitSize/8;
        if(ComSignals[signalId].ComBitSize%8)
        {
            Size++;
        }
        memcpy(signalData, ComSignals[signalId].ComSignalDataPtr, ize);
    }
}

//void inline unlockBuffer(PduIdType id)
//{
//  Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);
//    Asu_IPdu->PduBufferState.Locked=FALSE;
//    Asu_IPdu->PduBufferState.CurrentPosition=0;
//}

//void inline lockBuffer(PduIdType id)
//{
//  Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);
//  Asu_IPdu->PduBufferState.Locked=TRUE;
//}

