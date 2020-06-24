#include "Std_Types.h"
#include "Com.h"
#include "Com_helper.h"
#include "Com_Buffer.h"

#define NORMAL_SIGNAL   ((uint8)0x00)
#define GROUP_SIGNAL    ((uint8)0xff)

LOCAL FUNC(void, memclass) Com_WriteSignalDataToPduBuffer(CONSTP2CONST(ComSignal_type, memclass, ptrclass) signal,VAR(uint8, memclass) type);
LOCAL FUNC(void, memclass) Com_ReadSignalDataFromPduBuffer(VAR(PduIdType, memclass) ComRxPduId,CONSTP2CONST(ComSignal_type, memclass, ptrclass) SignalRef,VAR(uint8, memclass) type);

extern CONST(ComIPdu_type, memclass)        ComIPdus[COM_NUM_OF_IPDU];
extern CONST(ComSignal_type, memclass)      ComSignals[COM_NUM_OF_SIGNAL];
extern CONST(ComSignalGroup_type, memclass) ComSignalGroups[COM_NUM_OF_GROUP_SIGNAL];

FUNC(void, memclass)
Com_PackSignalsToPdu(VAR(uint16, memclass) ComIPuId)
{
    VAR(uint8, memclass) signalIndex;
    VAR(uint8, memclass) signalGroupIndex;
    CONSTP2VAR(ComIPdu_type, memclass, ptrclass) IPdu;

    IPdu = GET_IPdu(ComIPuId);

    for( signalIndex =(uint8) 0 ; signalIndex < IPdu->ComIPduNumOfSignals ; signalIndex++ )
    {
        Com_WriteSignalDataToPduBuffer(&IPdu->ComIPduSignalRef[signalIndex] ,NORMAL_SIGNAL);
    }

    for(signalGroupIndex=(uint8)0;signalGroupIndex<IPdu->ComIPduNumberOfSignalGroups;signalGroupIndex++)
    {
        for( signalIndex =(uint8) 0 ; signalIndex < IPdu->ComIPduSignalGroupRef[signalGroupIndex].ComIPduNumberOfGroupSignals ; signalIndex++ )
        {
            Com_WriteSignalDataToPduBuffer(&IPdu->ComIPduSignalGroupRef[signalGroupIndex].ComIPduSignalRef[signalIndex],GROUP_SIGNAL);
        }
    }
}

FUNC(void, memclass)
Com_PduUnpacking(VAR(PduIdType, memclass) ComRxPduId)
{
    VAR(uint8, memclass) signalIndex;
    VAR(uint8, memclass) signalGroupIndex;
    for ( signalIndex = (uint8)0; (ComIPdus[ComRxPduId].ComIPduNumOfSignals > signalIndex); signalIndex++)
    {
        if(ComIPdus[ComRxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitEnabled)
        {
            if (CHECKBIT(ComIPdus[ComRxPduId].ComIPduDataPtr, ComIPdus[ComRxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitPosition))
            {
                /*TODO:add this part in an inline function*/
                /*TODO:rename this function*/
                Com_ReadSignalDataFromPduBuffer(ComRxPduId,&ComIPdus[ComRxPduId].ComIPduSignalRef[signalIndex],NORMAL_SIGNAL);
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

            }
        }
        else
        {
            Com_ReadSignalDataFromPduBuffer(ComRxPduId,&ComIPdus[ComRxPduId].ComIPduSignalRef[signalIndex],NORMAL_SIGNAL);
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
    for(signalGroupIndex=(uint8)0;signalGroupIndex<ComIPdus[ComRxPduId].ComIPduNumberOfSignalGroups;signalGroupIndex++)
    {
        if(ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled)
        {
            if (CHECKBIT(ComIPdus[ComRxPduId].ComIPduDataPtr, ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComIPduSignalRef[signalIndex].ComUpdateBitPosition))
            {
                for(signalIndex=(uint8)0;signalIndex<ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComIPduNumberOfGroupSignals;signalIndex++)
                {
                    Com_ReadSignalDataFromPduBuffer(ComRxPduId,&ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComIPduSignalRef[signalIndex],GROUP_SIGNAL);
                }
                if(ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification)
                {
                    ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification();
                }
            }
        }
        else
        {
            for(signalIndex=(uint8)0;signalIndex<ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComIPduNumberOfGroupSignals;signalIndex++)
            {
                Com_ReadSignalDataFromPduBuffer(ComRxPduId,&ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComIPduSignalRef[signalIndex],GROUP_SIGNAL);
            }
            if(ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification)
            {
                ComIPdus[ComRxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification();
            }
        }
    }
}


LOCAL FUNC(void, memclass)
Com_WriteSignalDataToPduBuffer(CONSTP2CONST(ComSignal_type, memclass, ptrclass) const signal,VAR(uint8, memclass) type)
{
    P2VAR(uint8, memclass, ptrclass)    pdu;
    VAR(uint64, memclass)               mask;
    VAR(uint32, memclass)               position;
    VAR(uint32, memclass)               length;

    pdu=ComIPdus[signal->ComIPduHandleId].ComIPduDataPtr;
    if(signal->ComSignalType==UINT8_N)/*TODO:check UINT8_DYN*/
    {
        if(type==NORMAL_SIGNAL)
        {
            memcpy((void*)(pdu+(signal->ComBitPosition/(uint32)8)),(void*) signal->ComSignalDataPtr,signal->ComSignalLength);
        }
        else
        {
            memcpy((void*)(pdu+(signal->ComBitPosition/(uint32)8)), (void*)(((uint8*)signal->ComSignalDataPtr)+signal->ComSignalLength),signal->ComSignalLength);
        }
    }
    else
    {
        mask=(uint64)0xffffffffffffffffu;
        position=signal->ComBitPosition;
        length=(uint32)signal->ComBitSize;
        mask=mask<<((uint32)64 - (position+length));
        mask=mask>>((uint32)64 - (length));
        mask=mask<<position;
        (*((uint64*)pdu))&=~mask;
        if(type==NORMAL_SIGNAL)
        {
            (*((uint64*)pdu))|=(((uint64)(*((uint64*)signal->ComSignalDataPtr))<<position))&mask;
        }
        else
        {
            switch(signal->ComSignalType)
            {
            case BOOLEAN:
            case SINT8:
            case UINT8:
                (*((uint64*)pdu))|=(((uint64)(*((uint64*)(((uint8*)signal->ComSignalDataPtr)+1)))<<position))&mask;
                break;
            case FLOAT32:
            case UINT32:
            case SINT32:
                (*((uint64*)pdu))|=(((uint64)(*((uint64*)(((uint8*)signal->ComSignalDataPtr)+4)))<<position))&mask;
                break;
            case FLOAT64:
            case UINT64:
            case SINT64:
                (*((uint64*)pdu))|=(((uint64)(*((uint64*)(((uint8*)signal->ComSignalDataPtr)+8)))<<position))&mask;
                break;
            case UINT16:
            case SINT16:
                (*((uint64*)pdu))|=(((uint64)(*((uint64*)(((uint8*)signal->ComSignalDataPtr)+2)))<<position))&mask;
                break;
            default:
                /*MISRA c*/
                break;
            }
        }
    }
}

LOCAL FUNC(void, memclass)
Com_ReadSignalDataFromPduBuffer(VAR(PduIdType, memclass) ComRxPduId,CONSTP2CONST(ComSignal_type, memclass, ptrclass) SignalRef,VAR(uint8, memclass) type)
{
    /*TODO: add the sequence of the TP case (for UINT8_DYN)*/

    VAR(uint8, memclass) signalLength;
    VAR(uint32, memclass) startBit;
    VAR(uint64, memclass) Data;

    startBit = SignalRef->ComBitPosition;
    signalLength = SignalRef->ComBitSize;

    switch(SignalRef->ComSignalType)
    {
    case BOOLEAN:
    case SINT8:
    case UINT8:
        if(type==NORMAL_SIGNAL)
        {
            Data=*((uint64*)ComIPdus[ComRxPduId].ComIPduDataPtr);
        }
        else
        {
            Data = *((uint64*)(((uint8*)ComIPdus[ComRxPduId].ComIPduDataPtr)+1));
        }
        Data = Data << ((uint32)64 - (startBit+signalLength));
        Data = Data >> ((uint32)64 - (signalLength));
        *((uint8*)SignalRef->ComSignalDataPtr)=(uint8)Data;
        break;
    case FLOAT32:
    case UINT32:
    case SINT32:
        if(type==NORMAL_SIGNAL)
        {
            Data=*((uint64*)ComIPdus[ComRxPduId].ComIPduDataPtr);
        }
        else
        {
            Data = *((uint64*)(((uint8*)ComIPdus[ComRxPduId].ComIPduDataPtr)+4));
        }
        Data = Data << ((uint32)64 - (startBit+signalLength));
        Data = Data >> ((uint32)64 - (signalLength));
        *((uint32*)SignalRef->ComSignalDataPtr)=(uint32)Data;
        break;
    case FLOAT64:
    case UINT64:
    case SINT64:
        if(type==NORMAL_SIGNAL)
        {
            Data=*((uint64*)ComIPdus[ComRxPduId].ComIPduDataPtr);
        }
        else
        {
            Data = *((uint64*)(((uint8*)ComIPdus[ComRxPduId].ComIPduDataPtr)+8));
        }
        Data = Data << ((uint32)64 - (startBit+signalLength));
        Data = Data >> ((uint32)64 - (signalLength));
        *((uint64*)SignalRef->ComSignalDataPtr)=(uint64)Data;
        break;
    case UINT16:
    case SINT16:
        if(type==NORMAL_SIGNAL)
        {
            Data=*((uint64*)ComIPdus[ComRxPduId].ComIPduDataPtr);
        }
        else
        {
            Data = *((uint64*)(((uint8*)ComIPdus[ComRxPduId].ComIPduDataPtr)+2));
        }
        Data = Data << ((uint32)64 - (startBit+signalLength));
        Data = Data >> ((uint32)64 - (signalLength));
        *((uint16*)SignalRef->ComSignalDataPtr)=(uint16)Data;
        break;
    case UINT8_N:
        if(type==NORMAL_SIGNAL)
        {
            memcpy(SignalRef->ComSignalDataPtr, (void*)&((uint8*)ComIPdus[ComRxPduId].ComIPduDataPtr)[startBit/(uint32)8],SignalRef->ComSignalLength);
        }
        else
        {
            memcpy((void*)(((uint8*)SignalRef->ComSignalDataPtr)+SignalRef->ComSignalLength), (void*)&((uint8*)ComIPdus[ComRxPduId].ComIPduDataPtr)[startBit/(uint32)8],SignalRef->ComSignalLength);
        }
        break;
    default:
        /*MISRA c*/
        break;
    }
}



FUNC(void, memclass)
Com_WriteSignalDataToSignalBuffer (CONST(uint16, memclass) signalId, CONSTP2VAR(void, memclass, ptrclass) signalData)
{
    CONSTP2VAR(ComSignal_type, memclass, ptrclass) Signal;
    VAR(uint8, memclass) mod;

    Signal =  GET_Signal(signalId);
    if(Signal->ComSignalType==UINT8_N)
    {
        memcpy(Signal->ComSignalDataPtr, signalData, Signal->ComSignalLength);
    }
    else
    {
        if(Signal->ComBitSize%(uint8)8)
        {
            mod=(uint8)1;
        }
        else
        {
            mod=(uint8)0;
        }
        memcpy(Signal->ComSignalDataPtr, signalData, (uint32)((Signal->ComBitSize/(uint8)8)+(uint8)mod));
    }
}

/*TODO: add critical section*/
FUNC(void, memclass)
Com_ReadSignalDataFromSignalBuffer (CONST(uint16, memclass)  signalId,  P2VAR(void, memclass, ptrclass) signalData)
{
    VAR(uint8, memclass) Size;
    if(ComSignals[signalId].ComSignalType==UINT8_N)
    {
        memcpy(signalData, ComSignals[signalId].ComSignalDataPtr,ComSignals[signalId].ComSignalLength);
    }
    else
    {
        Size=ComSignals[signalId].ComBitSize/(uint8)8;
        if(ComSignals[signalId].ComBitSize%(uint8)8)
        {
            Size++;
        }
        memcpy(signalData, ComSignals[signalId].ComSignalDataPtr, (uint32)Size);
    }
}
