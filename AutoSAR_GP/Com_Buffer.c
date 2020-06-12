#include "Std_Types.h"
#include "Com.h"
#include "Com_helper.h"
#include "Com_Buffer.h"


static void Com_WriteSignalDataToPduBuffer(const ComSignal_type* signal);

extern const ComIPdu_type ComIPdus[];

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


void Com_UnPackSignalsFromPdu(uint16 ComIPuId)
{
    uint8 signalID = 0;
    //	const ComSignal_type * signal = NULL_PTR;
    //	const Com_Asu_Signal_type * Asu_Signal = NULL_PTR;
    const ComIPdu_type *IPdu = GET_IPdu(ComIPuId);

    for ( signalID = 0; (IPdu->ComIPduSignalRef[signalID] != NULL_PTR); signalID++)
    {
        //		signal = IPdu->ComIPduSignalRef[signalID];
        //		Asu_Signal = GET_AsuSignal(signal->ComHandleId);
        //
        //		if(Asu_Signal->ComSignalUpdated)
        //		{
        Com_ReadSignalDataFromPduBuffer(IPdu->ComIPduSignalRef[signalID]->ComHandleId,IPdu->ComIPduSignalRef[signalID]->ComSignalDataPtr);
        //		}
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



void Com_ReadSignalDataFromPduBuffer(const uint16 signalId, void *signalData)
{
    uint8 signalLength;
    uint8 data;
    uint8 BitOffsetInByte;
    uint32 bitPosition;
    uint8 pduStartByte;
    uint8 pduMask;
    uint8 * dataBytes = NULL_PTR;
    uint8 i;
    const uint8 *pduBufferBytes = NULL_PTR;
    uint8 * signalDataBytes = NULL_PTR;

    const ComSignal_type * Signal = GET_Signal(signalId);
    const ComIPdu_type *IPdu = GET_IPdu(Signal->ComIPduHandleId);
    const void * pduBuffer = IPdu->ComIPduDataPtr;

    bitPosition = Signal->ComBitPosition;
    signalLength = Signal->ComBitSize / 8;
    BitOffsetInByte = bitPosition % 8;
    pduStartByte = bitPosition / 8;

    dataBytes = (uint8 *) signalData;
    memset(signalData, 0, signalLength);
    pduBufferBytes = (const uint8 *)pduBuffer;
    pduBufferBytes += pduStartByte;


    uint8 x;
    for(i = 0; i<=signalLength; i++)
    {
        pduMask = 255;
        if( i == 0)
        {
            pduMask = pduMask << BitOffsetInByte;
            data = (* pduBufferBytes) & pduMask;
            data = data >> BitOffsetInByte;
            *dataBytes = *dataBytes | data;
            x= *dataBytes;
            pduBufferBytes ++;
        }
        else if(i==signalLength)
        {
            pduMask = pduMask >> (8-BitOffsetInByte);
            data = (* pduBufferBytes) & pduMask;
            data = data << (8-BitOffsetInByte);
            *dataBytes = (* dataBytes) | data;
            x= *dataBytes;
        }
        else
        {
            pduMask = pduMask >> (8-BitOffsetInByte);
            data = (* pduBufferBytes) & pduMask;
            data = data << (8-BitOffsetInByte);
            *dataBytes = (* dataBytes) | data;

            dataBytes++;

            pduMask = 255;
            pduMask = pduMask << BitOffsetInByte;
            data = (* pduBufferBytes) & pduMask;
            data = data >> BitOffsetInByte;
            *dataBytes = (* pduBufferBytes) | data;
            x= *dataBytes;
            pduBufferBytes ++;

        }
    }
}



void Com_WriteSignalDataToSignalBuffer (const uint16 signalId, const void * signalData)
{
    const ComSignal_type * Signal =  GET_Signal(signalId);
    uint8 mod;
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
    const ComSignal_type * Signal =  GET_Signal(signalId);
    memcpy(signalData, Signal->ComSignalDataPtr, Signal->ComBitSize/8);
}



//void inline unlockBuffer(PduIdType id)
//{
//	Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);
//    Asu_IPdu->PduBufferState.Locked=FALSE;
//    Asu_IPdu->PduBufferState.CurrentPosition=0;
//}

//void inline lockBuffer(PduIdType id)
//{
//	Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);
//	Asu_IPdu->PduBufferState.Locked=TRUE;
//}



