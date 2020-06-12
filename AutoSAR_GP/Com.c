/***************************************************
 *
 * File Name: Com.c 
 *
 * Author: AUTOSAR COM Team
 * 
 * Date Created: 6 MAsuh 2019
 * 
 * Version	: 01	
 * 
 ****************************************************/
#include "Com.h"
#include "Com_helper.h"
#include "Com_Buffer.h"
#include "PduR_Com.h"
#include "PduR.h"
#include "Com_Cbk.h"

/*****************************************************************
 *                     Functions Definitions                     *
 *****************************************************************/

typedef struct privateIPdu_type
{
    boolean updated;
    boolean locked;
}privateIPdu_type;

typedef struct privateTxTPdu_type
{
    float32 remainingTimePeriod;
    float32 minimumDelayTimer;
    uint8   numberOfRepetitionsLeft;
}privateTxTPdu_type;

typedef void (*notificationType)(void);

/* Com_Config declaration*/
uint8 com_pdur[] = {vcom};

/* Com_Asu_Config declaration*/
extern const ComIPdu_type ComIPdus[];
extern const ComSignal_type ComSignals[];
extern const ComTxIPdu_type ComTxIPdus[];

privateIPdu_type privateIPdus[COM_NUM_OF_IPDU];
privateTxTPdu_type privateTxIPdus[sizeof(ComTxIPdus)/sizeof(ComTxIPdu_type)];
const uint16 numberOfSendIPdus=sizeof(ComTxIPdus)/sizeof(ComTxIPdu_type);
uint16 sendIPdusIds[sizeof(ComTxIPdus)/sizeof(ComTxIPdu_type)];
notificationType pendingNotifications[COM_NUM_OF_SIGNAL];

void Com_Init( const ComConfig_type* config)
{
    /*TODO:add all sendIPdusIds*/
    /*TODO:init TX time periods*/
    /* Initialize ComConfig */
    ComConfig = config;
    const ComSignal_type *Signal;

    //1- loop on IPDUs
    uint16 pduId;
    for ( pduId = 0; pduId<COM_NUM_OF_IPDU; pduId++) {

        // 1.1- Initialize I-PDU
        const ComIPdu_type *IPdu = GET_IPdu(pduId);
        Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(pduId);

        Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeRepetitionPeriodTimer = \
                IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeRepetitionPeriod;

        Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeTimePeriodTimer = \
                IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeTimePeriod;

        Asu_IPdu->Com_Asu_Pdu_changed = FALSE;

        Asu_IPdu->Com_Asu_First_Repetition = TRUE;


        // Initialize the memory with the default value.
        if (IPdu->ComIPduDirection == SEND)
        {
            memset((void *)IPdu->ComIPduDataPtr, IPdu->ComTxIPdu.ComTxIPduUnusedAreasDefault, IPdu->ComIPduSize);
        }

        // For each signal in this PDU
        uint16 signalId;
        for ( signalId = 0; (IPdu->ComIPduSignalRef != NULL_PTR) && \
        (IPdu->ComIPduSignalRef[signalId] != NULL_PTR); signalId++)
        {
            Signal = IPdu->ComIPduSignalRef[signalId];

            // Clear update bits
            CLEARBIT(IPdu->ComIPduDataPtr, Signal->ComUpdateBitPosition);
        }

    }
}

static void Com_RxProcessSignals(PduIdType ComRxPduId)
{
    const ComSignal_type *comSignal = NULL_PTR;
    const ComIPdu_type *IPdu = GET_IPdu(ComRxPduId);
    Com_Asu_Signal_type * Asu_Signal = NULL_PTR;
    uint8 signalId;
    for( signalId = 0; IPdu->ComIPduSignalRef[signalId] != NULL_PTR; signalId++)
    {
        comSignal = IPdu->ComIPduSignalRef[signalId];
        Asu_Signal = GET_AsuSignal(comSignal->ComHandleId);

        if (CHECKBIT(IPdu->ComIPduDataPtr, comSignal->ComUpdateBitPosition))
        {
            if (IPdu->ComIPduSignalProcessing == IMMEDIATE)
            {
                // unpack the pdu and update signal buffer
                Com_UnPackSignalsFromPdu(ComRxPduId);

                // If signal processing mode is IMMEDIATE, notify the signal callback.
                if (IPdu->ComIPduSignalRef[signalId]->ComNotification != NULL_PTR)
                {
                    IPdu->ComIPduSignalRef[signalId]->ComNotification();
                }
            }
            else
            {
                // Signal processing mode is DEFERRED, mark the signal as updated.
                Asu_Signal->ComSignalUpdated = 1;
            }
        }
    }
}

void Com_MainFunctionRx(void)
{
    const ComIPdu_type *IPdu = NULL_PTR;
    const ComSignal_type *signal = NULL_PTR;
    Com_Asu_Signal_type * Asu_Signal = NULL_PTR;
    Com_Asu_IPdu_type *Asu_IPdu = NULL_PTR;
    uint16 signalID;
    boolean pduUpdated = FALSE;

    /* Loop on IPDUs */
    uint16 pduId;
    for ( pduId = 0; pduId < COM_NUM_OF_IPDU; pduId++)
    {
        IPdu = GET_IPdu(pduId);
        Asu_IPdu = GET_AsuIPdu(pduId);

        for (signalID = 0; (IPdu->ComIPduSignalRef != NULL_PTR) && (IPdu->ComIPduSignalRef[signalID] != NULL_PTR); signalID++)
        {
            if (Asu_Signal->ComSignalUpdated)
            {
                pduUpdated = TRUE;
            }
        }

        if (pduUpdated && IPdu->ComIPduSignalProcessing == DEFERRED && IPdu->ComIPduDirection == RECEIVE && IPdu->ComIPduType == NORMAL)
        {
            /* unlock the buffer */
            UNLOCKBUFFER(&Asu_IPdu->PduBufferState);

            /* copy the deferred buffer to the actual pdu buffer */
            Com_UnPackSignalsFromPdu(pduId);

            /* loop on the signal in this ipdu */
            for (signalID = 0; (IPdu->ComIPduSignalRef != NULL_PTR) && (IPdu->ComIPduSignalRef[signalID] != NULL_PTR); signalID++)
            {
                signal = IPdu->ComIPduSignalRef[signalID];
                Asu_Signal = GET_AsuSignal(signal->ComHandleId);

                /* if at least on signal is Updated, mark this Pdu as Updated */
                if (Asu_Signal->ComSignalUpdated)
                {
                    if (signal->ComNotification != NULL_PTR)
                    {
                        signal->ComNotification();
                    }
                    Asu_Signal->ComSignalUpdated = FALSE;
                }
            }
        }
    }
}

void Com_MainFunctionTx(void)
{
    const ComIPdu_type* IPdu;
    boolean mixed;
    uint16  sendIPduIndex;

    for ( sendIPduIndex = 0; sendIPduIndex<numberOfSendIPdus; sendIPduIndex++)
    {
        IPdu = GET_IPdu(sendIPdusIds[sendIPduIndex]);

        mixed = FALSE;

        switch(ComTxIPdus[IPdu->ComTxIPdu].ComTxModeMode)
        {
        /* if the transmission mode is mixed */
        case MIXED:
            mixed = TRUE;
            /* no break because the mixed is periodic and direct */
            /* if the transmission mode is direct */
        case DIRECT:
            /*TODO:check MDT macro*/
            if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer < ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
            {
                privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer+=COM_TX_TIME_BASE;
            }
            if(privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft > 0)
            {
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
                    if(Com_TriggerIPDUSend(sendIPdusIds[sendIPduIndex])== E_OK)
                    {
                        privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer=0;
                        privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft--;
                    }
                }
            }
            if(mixed==FALSE)/* in case the Pdu is mixed don't break */
            {
                break;
            }
            /* if the transmission mode is periodic */
        case PERIODIC:
            if(mixed!=FALSE)
            {
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer < ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
                    privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer+=COM_TX_TIME_BASE;
                }
            }
            if((privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod<=0)&&\
                    (privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime))
            {
                if(Com_TriggerIPDUSend(sendIPdusIds[sendIPduIndex]) == E_OK)
                {
                    privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod = \
                            ComTxIPdus[IPdu->ComTxIPdu].ComTxModeTimePeriod;
                    privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer=0;
                }
            }
            timerDec(privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod);
        }
    }
}

/* Updates the signal object identified by SignalId with the signal referenced by the SignalDataPtr parameter */
uint8 Com_SendSignal( Com_SignalIdType SignalId, const void* SignalDataPtr )
{
    Std_ReturnType result;
    uint8 byteIndex;
    /* Get signal of "SignalId" */
    const ComSignal_type *Signal;

    /*Get IPdu of this signal */
    const ComIPdu_type* IPdu;
    const privateIPdu_type* privateIPdu;
    result=E_OK;

    /* validate signalID */
    if((!validateSignalID(SignalId))&&(/*TODO:send signal?*/) )
    {
        result=E_NOT_OK;
    }
    else
    {
        /* Get signal of "SignalId" */
        Signal = GET_Signal(SignalId);

        /*Get IPdu of this signal */
        IPdu = GET_IPdu(Signal->ComIPduHandleId);
        privateIPdu=&privateIPdus[Signal->ComIPduHandleId];

        switch(Signal->ComTransferProperty)
        {
        case TRIGGERED_WITHOUT_REPETITION:
            privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
            privateIPdu->updated=TRUE;
            break;
#if 0 /*repitition is not supported now*/
        case TRIGGERED:
            privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = \
            (ComTxIPdus[IPdu->ComTxIPdu].ComTx) + 1;
            break;

        case TRIGGERED_ON_CHANGE:
            if (Asu_IPdu->Com_Asu_Pdu_changed)
            {
                Asu_IPdu->Com_Asu_TxIPduTimers.ComTxIPduNumberOfRepetitionsLeft = \
                        (IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeNumberOfRepetitions) + 1;
                Asu_IPdu->Com_Asu_Pdu_changed = FALSE;
            }
            break;
#endif
        case TRIGGERED_ON_CHANGE_WITHOUT_REPETITION:
            switch(Signal->ComSignalType)
            {
            case BOOLEAN:
            case UINT8:
            case SINT8:
                if((*((uint8*)(Signal->ComSignalDataPtr)))!=(*((uint8*)SignalDataPtr)))
                {
                    privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
                    privateIPdu->updated=TRUE;
                }
                break;
            case UINT16:
            case SINT16:
                if((*((uint16*)(Signal->ComSignalDataPtr)))!=(*((uint16*)SignalDataPtr)))
                {
                    privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
                    privateIPdu->updated=TRUE;
                }
                break;
            case FLOAT32:
            case UINT32:
            case SINT32:
                if((*((uint32*)(Signal->ComSignalDataPtr)))!=(*((uint32*)SignalDataPtr)))
                {
                    privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
                    privateIPdu->updated=TRUE;
                }
                break;
            case FLOAT64:
            case UINT64:
            case SINT64:
                if((*((uint64*)(Signal->ComSignalDataPtr)))!=(*((uint64*)SignalDataPtr)))
                {
                    privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
                    privateIPdu->updated=TRUE;
                }
                else
                {

                }
                break;
                /*TODO:UINT8_DYN*/
            case UINT8_N:
                for(byteIndex=0;byteIndex<Signal->ComSignalLength;byteIndex++)
                {
                    if(((uint8*)(Signal->ComSignalDataPtr))[byteIndex]!=((uint8*)SignalDataPtr)[byteIndex])
                    {
                        privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
                        privateIPdu->updated=TRUE;
                        break;
                    }
                    else
                    {

                    }
                }
            }
        }

        /* update the Signal buffer with the signal data */
        if(TRUE==privateIPdu->updated)
        {
            Com_WriteSignalDataToSignalBuffer(Signal->ComHandleId, SignalDataPtr);
            /*TODO:check if update bit is enabled*/
            /* Set the update bit of this signal */
            SETBIT(IPdu->ComIPduDataPtr, Signal->ComUpdateBitPosition);
        }
        else
        {

        }
    }
    return result;
}

/* Copies the data of the signal identified by SignalId to the location specified by SignalDataPtr */
uint8 Com_ReceiveSignal( Com_SignalIdType SignalId, void* SignalDataPtr )
{
    /* validate signalID */
    if(!validateSignalID(SignalId) )
        return E_NOT_OK;

    /* Get signal of "SignalId" */
    const ComSignal_type *Signal = GET_Signal(SignalId);

    /* Get IPDU of signal ipduHandleId */
    const ComIPdu_type *IPdu = GET_IPdu(Signal->ComIPduHandleId);

    /* check ipdu direction is receive */
    if(IPdu->ComIPduDirection == RECEIVE)
    {
        Com_ReadSignalDataFromSignalBuffer(SignalId, SignalDataPtr);
    }
    else
    {
        return E_NOT_OK;
    }
    return E_OK;
}

BufReq_ReturnType Com_CopyTxData( PduIdType id, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr )
{
    ComIPdu_type *IPdu = GET_IPdu(id);
    Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);

    if( (IPdu->ComIPduDirection == SEND) &&
            (Asu_IPdu->PduBufferState.CurrentPosition + info->SduLength <= IPdu->ComIPduSize) )
    {
        void * source = (uint8*)IPdu->ComIPduDataPtr + Asu_IPdu->PduBufferState.CurrentPosition;
        LOCKBUFFER(&Asu_IPdu->PduBufferState);
        memcpy( (void*) info->SduDataPtr, source, info->SduLength);
        Asu_IPdu->PduBufferState.CurrentPosition += info->SduLength;
        *availableDataPtr = IPdu->ComIPduSize - Asu_IPdu->PduBufferState.CurrentPosition;
        return BUFREQ_OK;
    }
    else
    {
        return BUFREQ_E_NOT_OK;
    }

}

BufReq_ReturnType Com_CopyRxData( PduIdType id, const PduInfoType* info, PduLengthType* bufferSizePtr )
{
    ComIPdu_type *IPdu = GET_IPdu(id);
    Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);

    if( (IPdu->ComIPduDirection == RECEIVE) &&\
            (IPdu->ComIPduSize - Asu_IPdu->PduBufferState.CurrentPosition >= info->SduLength )&&\
            Asu_IPdu->PduBufferState.Locked)
    {
        void* distination =(void*)((uint8 *) IPdu->ComIPduDataPtr+ Asu_IPdu->PduBufferState.CurrentPosition);
        if(info->SduDataPtr != NULL_PTR)
            memcpy(distination, info->SduDataPtr, info->SduLength);
        Asu_IPdu->PduBufferState.CurrentPosition += info->SduLength;
        *bufferSizePtr = IPdu->ComIPduSize - Asu_IPdu->PduBufferState.CurrentPosition;

        return BUFREQ_OK;
    }
    else
    {
        return BUFREQ_E_NOT_OK;
    }
}

Std_ReturnType Com_TriggerIPDUSend( PduIdType PduId )
{
    const ComIPdu_type* IPdu;
    PduInfoType PduInfoPackage;
    uint8 signalIndex;
    Std_ReturnType result;
    const privateIPdu_type* privateIPdu;

    result=E_OK;
    privateIPdu=&privateIPdus[PduId];
    if(privateIPdu->updated)
    {
        (privateIPdu->updated=FALSE;
        Com_PackSignalsToPdu(PduId);
    }
    IPdu = GET_IPdu(PduId);
    PduInfoPackage.SduDataPtr = (uint8 *)IPdu->ComIPduDataPtr;
    PduInfoPackage.SduLength = IPdu->ComIPduSize;

    if (privateIPdu->locked)
    {
        result=E_NOT_OK;
    }
    else
    {
        privateIPdu->locked=TRUE;
        if (PduR_ComTransmit(com_pdur[PduId], &PduInfoPackage) != E_OK)
        {
            result=E_NOT_OK;
        }
        else
        {

        }
        if((ComTxIPdus[IPdu->ComTxIPdu].ComTxIPduClearUpdateBit == TRIGGER_TRANSMIT)||\
                ((ComTxIPdus[IPdu->ComTxIPdu].ComTxIPduClearUpdateBit == TRANSMIT)&&(result==E_OK)))
        {
            for ( signalIndex = 0 ; signalIndex < IPdu->ComIPduNumOfSignals ; signalIndex++ )
            {
                if(IPdu->ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=FALSE)/*Update bit is enabled*/
                {
                    CLEARBIT(IPdu->ComIPduDataPtr, IPdu->ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
                }
            }
        }
    }
    return result;
}

void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    const ComIPdu_type *IPdu = GET_IPdu(ComRxPduId);

    memcpy(IPdu->ComIPduDataPtr, PduInfoPtr->SduDataPtr, IPdu->ComIPduSize);

    Com_RxProcessSignals(ComRxPduId);

    return;
}

BufReq_ReturnType Com_StartOfReception(PduIdType id,const PduInfoType *info,PduLengthType TpSduLength,PduLengthType *bufferSizePtr)
{
    Com_Asu_IPdu_type *AsuIPdu=GET_AsuIPdu(id);
    PduLengthType ComIPduSize;

    if(GET_IPdu(id)->ComIPduDirection==RECEIVE && GET_IPdu(id)->ComIPduType == TP)
    {
        //making sure that the buffer is unlocked
        if(!AsuIPdu->PduBufferState.Locked)
        {
            //getting the ipdu size
            ComIPduSize = GET_IPdu(id)->ComIPduSize;
            //making sure that we have the enough space for the sdu
            if(ComIPduSize>=TpSduLength)
            {
                //lock the buffer until copying is done
                LOCKBUFFER(&AsuIPdu->PduBufferState);
                ///return the available buffer size
                *bufferSizePtr=ComIPduSize;
            }
            else
            {
                return BUFREQ_E_OVFL;
            }
        }
        else
        {
            return BUFREQ_E_BUSY;
        }
        return BUFREQ_OK;
    }
    return BUFREQ_E_NOT_OK;
}



void Com_TpRxIndication(PduIdType id,Std_ReturnType Result)
{
    const ComIPdu_type *ipdu=GET_IPdu(id);
    Com_Asu_IPdu_type *AsuIPdu=GET_AsuIPdu(id);

    if (Result == E_OK)
    {
        if (ipdu->ComIPduSignalProcessing == IMMEDIATE)
        {
            UNLOCKBUFFER(&AsuIPdu->PduBufferState);

            // In deferred mode, buffers are unlocked in mainfunction
            Com_RxProcessSignals(id);
        }
    }
    else
    {
        UNLOCKBUFFER(&AsuIPdu->PduBufferState);
    }
}


void Com_TpTxConfirmation(PduIdType PduId, Std_ReturnType Result)
{
    uint8 signalId;
    ComSignal_type * signal = NULL_PTR;
    ComIPdu_type *ipdu=GET_IPdu(PduId);
    Com_Asu_IPdu_type *AsuIPdu=GET_AsuIPdu(PduId);

    UNLOCKBUFFER(&AsuIPdu->PduBufferState);

    if (ipdu->ComTxIPdu.ComTxIPduClearUpdateBit == CONFIRMATION)
    {
        for(signalId = 0; (ipdu->ComIPduSignalRef != NULL_PTR) && (ipdu->ComIPduSignalRef[signalId] != NULL_PTR) ; signalId++)
        {
            signal = ipdu->ComIPduSignalRef[signalId];
            CLEARBIT(ipdu->ComIPduDataPtr,signal->ComUpdateBitPosition);
        }
    }
}

void Com_TxConfirmation( PduIdType TxPduId, Std_ReturnType result )
{
    uint8 signalIndex;
    if((result==E_OK)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
    {
        for ( signalIndex = 0 ; signalIndex < ComIPdus[TxPduId].ComIPduNumOfSignals ; signalIndex++ )
        {
            if(ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=FALSE)/*Update bit is enabled*/
            {
                CLEARBIT(IPdu->ComIPduDataPtr, IPdu->ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
            }
            if(ComIPdus[TxPduId].ComIPduSignalProcessing==IMMEDIATE)
            {
                if(ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComNotification)
                {
                    ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComNotification();
                }
            }
            else
            {
                /*TODO:same as mostafa did, double buffer*/
            }
        }
    }
    privateIPdu[TxPduId].locked=FALSE;
}
