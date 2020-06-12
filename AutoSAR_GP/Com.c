/***************************************************
 * File Name: Com.c 
 * Author: AUTOSAR COM Team
 * Date Created: Jun 2020
 * Version	: 4.0
 ****************************************************/
#include "Com.h"
#include "Com_helper.h"
#include "Com_Buffer.h"
#include "Com_Asu_Types.h"
#include "PduR_Com.h"
#include "PduR.h"
#include "Com_Cbk.h"



//TODO: check which file must be in
#define ENTER_CRITICAL_SECTION()             __asm("    cpsie   i\n");
#define EXIT_CRITICAL_SECTION()              __asm("    cpsid   i\n");



#define CompareData(SignalId,signalDataPtr,equalityCheck) do\
        {\
    signalSizeBytes=(ComSignals[SignalId].ComBitSize/8)+if(ComSignals[SignalId].ComBitSize%8);\
    for(counter=0;counter < signalSizeBytes; counter++)\
    {\
        signalBufferPtr = (uint8*)(ComSignals[SignalId].ComSignalDataPtr + counter);\
        dataBufferPtr   = (uint8*)(signalDataPtr + counter);\
        if((*signalBufferPtr)!=(*dataBufferPtr))\
        {\
            equalityCheck = 0;\
            break;\
        }\
        else\
        {\
            equalityCheck = 1;\
        }\
    }while(0);


#define TRIGGERED_WITHOUT_REPETITION() do{\
        Asu_IPdu->Com_Asu_TxIPduTimers.ComTxIPduNumberOfRepetitionsLeft = 1;\
}while(0);



#define numberOfAuxilaryArr             2
/* Global variables for the deferred and Received PDU */
uint16 numberOfRecievedPdu;
uint8  rxIndicationProcessingDeferredPduIndex;
uint16 rxindicationNumberOfRecievedPdu;
uint8  rxDeferredPduArr[numberOfAuxilaryArr][COM_NUM_OF_IPDU];

/*****************************************************************
 *                     Functions Definitions                     *
 *****************************************************************/

/* Com_Config declaration*/
const ComConfig_type * ComConfig;
uint8 com_pdur[] = {vcom};

/* Com_Asu_Config declaration*/
extern Com_Asu_Config_type ComAsuConfiguration;
static Com_Asu_Config_type * Com_Asu_Config = &ComAsuConfiguration;

void Com_Init( const ComConfig_type* config)
{
    /* 1- loop on IPDUs */
    uint16 pduId;
    for ( pduId = 0; pduId<COM_NUM_OF_IPDU; pduId++) {

        /* 1.1- Initialize I-PDU */
        Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(pduId);

        Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeTimePeriodTimer = \
                ComTxIPdus[pduId].ComTxModeTimePeriod;

        Asu_IPdu->Com_Asu_Pdu_changed = FALSE;
        Asu_IPdu->Com_Asu_First_Repetition = TRUE;

        /* Initialize the memory with the default value.] */
        if (ComTxIPdus[pduId].ComIPduDirection == SEND) {
            memset((void *)ComIPdus[pduId].ComIPduDataPtr, ComTxIPdus[pduId].ComTxIPduUnusedAreasDefault, ComIPdus[pduId].ComIPduSize);
        }

        /* For each signal in this PDU */
        uint16 signalIndex;
        for ( signalIndex = 0; (ComIPdus[pduId].ComIPduNumOfSignal > signalIndex); signalIndex++)
        {
            /* Check for the update Bit is enabled or disabled */
            if((ComIPdus[pduId].ComIPduSignalRef)[signalIndex].ComUpdateBitEnabled)
            {
                /* Clear update bits */
                CLEARBIT(ComIPdus[pduId].ComIPduDataPtr, (ComIPdus[pduId].ComIPduSignalRef)[signalIndex].ComUpdateBitPosition);
            }
            else
            {
                /* For MISRA rules */
            }
        }
    }
}

void Com_MainFunctionRx(void)
{
    uint16 mainRxNumberOfReceivedPdu;
    Com_Asu_IPdu_type *Asu_IPdu = NULL_PTR;
    uint16 signalID, pduId;
    uint16 Id;
    boolean pduUpdated = FALSE;

    ENTER_CRITICAL_SECTION();
    mainRxNumberOfReceivedPdu=rxindicationNumberOfRecievedPdu;
    rxindicationNumberOfRecievedPdu=0;
    rxIndicationProcessingDeferredPduIndex^=1;
    EXIT_CRITICAL_SECTION();
    for(Id=0;pduId<mainRxNumberOfReceivedPdu;Id++)
    {
        pduId = rxDeferredPduArr[Id];
        Asu_IPdu = GET_AsuIPdu(rxDeferredPduArr[Id]);
        if (ComIPdus[pduId].ComIPduType == NORMAL)
        {
            /* unlock the buffer */
            UNLOCKBUFFER(&Asu_IPdu->PduBufferState);

            /* copy the deferred buffer to the actual pdu buffer */
            Com_PduUnpacking(pduId);

            /* loop on the signal in this ipdu */
            for (signalID = 0; ComIPdus[pduId].ComIPduNumOfSignal ; signalID++)
            {
                ComIPdus[pduId].ComIPduSignalRef[signalID];
                Asu_Signal = GET_AsuSignal(signal->ComHandleId);

                /* if at least on signal is Updated, mark this Pdu as Updated */
                //TODO: must check is that true editing
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
    const ComIPdu_type *IPdu;
    Com_Asu_IPdu_type *Asu_IPdu;
    boolean mixed_t;
    //Todo:unused variable
    boolean mixedSent;

    //Loop on IPDUs
    uint16 pduId;
    for ( pduId = 0; pduId<COM_NUM_OF_IPDU; pduId++)
    {
        IPdu = GET_IPdu(pduId);
        Asu_IPdu = GET_AsuIPdu(pduId);


        /* if it is a send PDU*/
        if(IPdu->ComIPduDirection == SEND)
        {
            mixed_t = FALSE;

            switch(IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeMode)
            {
            /* if the transmission mode is mixed */
            case MIXED:
                mixed_t = TRUE;
                /* no break because the mixed is periodic and direct */
                /* if the transmission mode is periodic */
            case PERIODIC:

                timerDec(Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeTimePeriodTimer);

                if(Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeTimePeriodTimer<=0)
                {
                    //TODO: make it static
                    if(Com_TriggerIPDUSend(pduId) == E_OK)
                    {
                        Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeTimePeriodTimer = \
                                IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeTimePeriod;
                    }
                }
                if(!mixed_t)/* in case the Pdu is mixed don't break */
                    break;
                /* if the transmission mode is direct */
            case DIRECT:
                if(Asu_IPdu->Com_Asu_TxIPduTimers.ComTxIPduNumberOfRepetitionsLeft > 0)
                {
                    //TODO:menimum time in SWS.
                    timerDec(Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeRepetitionPeriodTimer);

                    if(Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeRepetitionPeriodTimer <= 0 || Asu_IPdu->Com_Asu_First_Repetition )
                    {
                        if(Com_TriggerIPDUSend(pduId)== E_OK)
                        {
                            Asu_IPdu->Com_Asu_TxIPduTimers.ComTxModeRepetitionPeriodTimer = \
                                    IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeRepetitionPeriod;

                            Asu_IPdu->Com_Asu_First_Repetition = FALSE;

                            Asu_IPdu->Com_Asu_TxIPduTimers.ComTxIPduNumberOfRepetitionsLeft --;
                        }
                    }
                }
            }
        }
    }
}

/* Updates the signal object identified by SignalId with the signal referenced by the SignalDataPtr parameter */
uint8 Com_SendSignal( uint16 SignalId, const void* SignalDataPtr )
{
    uint8 counter, signalSizeBytes;
    uint8* signalBufferPtr, dataBufferPtr;
    boolean equalityCheck;
    /* validate signalID */
    if(!validateSignalID(SignalId) )
        return E_NOT_OK;
    /* Get IPDU_Asu of signal ipduHandleId */
    Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(ComSignals[SignalId].ComIPduHandleId);
    switch(ComSignals[SignalId].ComTransferProperty)
    {
#if 1
    //TODO:remove code redundancy using macro like function
    case TRIGGERED_WITHOUT_REPETITION:
        TRIGGERED_WITHOUT_REPETITION();
        Asu_IPdu->Com_Asu_Pdu_changed = FALSE;//TODO:figure out the flag value
        break;
#if 0 /* Repetition is not supported as ComTxModeNumberOfRepetitions is not added to the ComTxIPdu_type yet */
    case TRIGGERED:
        Asu_IPdu->Com_Asu_TxIPduTimers.ComTxIPduNumberOfRepetitionsLeft = \
        (IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeNumberOfRepetitions) + 1;
        Asu_IPdu->Com_Asu_Pdu_changed = FALSE;
        break;

    case TRIGGERED_ON_CHANGE:
#define Compare_the_Signal_with_local_Buffer    1
        CompareData(SignalId,signalDataPtr,equalityCheck);
        if (equalityCheck == 1)
        {
            Asu_IPdu->Com_Asu_TxIPduTimers.ComTxIPduNumberOfRepetitionsLeft = \
                    (IPdu->ComTxIPdu.ComTxModeFalse.ComTxMode.ComTxModeNumberOfRepetitions) + 1;
            Asu_IPdu->Com_Asu_First_Repetition = TRUE;
            Asu_IPdu->Com_Asu_Pdu_changed = FALSE;
        }
        break;
#endif
    case TRIGGERED_ON_CHANGE_WITHOUT_REPETITION:
        CompareData(SignalId,signalDataPtr,equalityCheck);
        if (equalityCheck == 1)
        {
            TRIGGERED_WITHOUT_REPETITION();
            Asu_IPdu->Com_Asu_First_Repetition = TRUE;
            Asu_IPdu->Com_Asu_Pdu_changed = FALSE;
        }
        break;
    }
#endif
    /* update the Signal buffer with the signal data */
    Com_WriteSignalDataToSignalBuffer(ComSignals[SignalId].ComHandleId, SignalDataPtr);
    if(ComSignals[SignalId].ComUpdateBitEnabled)
    {
        /* Set the update bit of this signal */
        SETBIT(ComIPdus[ComSignals[SignalId].ComIPduHandleId].ComIPduDataPtr, ComSignals[SignalId].ComUpdateBitPosition);
    }
    return E_OK;
}

/* Copies the data of the signal identified by SignalId to the location specified by SignalDataPtr */
uint8 Com_ReceiveSignal( uint16 SignalId, void* SignalDataPtr )
{
    /* validate signalID */
    if(!validateSignalID(SignalId))
        return E_NOT_OK;

    /* check ipdu direction is receive */
    if(ComIPdus[ComSignals[SignalId].ComIPduHandleId].ComIPduDirection == RECEIVE)
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

BufReq_ReturnType Com_CopyRxData( PduIdType Pduid, const PduInfoType* info, PduLengthType* bufferSizePtr )
{
    ComIPdus[Pduid];
    Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(id);

    if( (ComIPdus[Pduid].ComIPduDirection == RECEIVE) &&\
            (ComIPdus[Pduid].ComIPduSize - Asu_IPdu->PduBufferState.CurrentPosition >= info->SduLength )&&\
            Asu_IPdu->PduBufferState.Locked)
    {
        void* distination =(void*)((uint8 *) ComIPdus[Pduid].ComIPduDataPtr+ Asu_IPdu->PduBufferState.CurrentPosition);
        if(info->SduDataPtr != NULL_PTR)
            memcpy(distination, info->SduDataPtr, info->SduLength);
        Asu_IPdu->PduBufferState.CurrentPosition += info->SduLength;
        *bufferSizePtr = ComIPdus[Pduid].ComIPduSize - Asu_IPdu->PduBufferState.CurrentPosition;

        return BUFREQ_OK;
    }
    else
    {
        return BUFREQ_E_NOT_OK;
    }
}

Std_ReturnType Com_TriggerIPDUSend( PduIdType PduId )
{
    const ComIPdu_type *IPdu = GET_IPdu(PduId);
    Com_Asu_IPdu_type *Asu_IPdu = GET_AsuIPdu(PduId);
    PduInfoType PduInfoPackage;
    uint8 signalID;

    //TODO: was not here
    if (Asu_IPdu->PduBufferState.Locked)
    {
        return E_NOT_OK;
    }

    Com_PackSignalsToPdu(PduId);
    PduInfoPackage.SduDataPtr = (uint8 *)IPdu->ComIPduDataPtr;
    PduInfoPackage.SduLength = IPdu->ComIPduSize;

    //TODO:else--> make local std_error type
    //TODO:check if sent to TPTrigger or transmit.
    if (PduR_ComTransmit(com_pdur[IPdu->ComIPduHandleId], &PduInfoPackage) == E_OK)
    {
        //TODO: Preprocessor checks if update bit is enabled
        // Clear all update bits for the contained signals
        if(IPdu->ComTxIPdu.ComTxIPduClearUpdateBit == TRANSMIT)
        {
            for ( signalID = 0; (IPdu->ComIPduSignalRef[signalID] != NULL_PTR); signalID++)
            {
                //TODO:check this signal update bit is enabled
                CLEARBIT(IPdu->ComIPduDataPtr, IPdu->ComIPduSignalRef[signalID]->ComUpdateBitPosition);
            }
        }

    }
    else
    {
        return E_NOT_OK;
    }
    return E_OK;
}



void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    ENTER_CRITICAL_SECTION();
    memcpy(IPdu->ComIPduDataPtr, PduInfoPtr->SduDataPtr, IPdu->ComIPduSize);
    EXIT_CRITICAL_SECTION();
    if(ComIPdus[ComRxPduId].ComIPduDirection == RECEIVE)
    {
        if(ComIPdus[ComRxPduId].ComIPduSignalProcessing == IMMEDIATE)
        {
            Com_PduUnpacking(ComRxPduId);
        }
        else
        {
            rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex][rxindicationNumberOfRecievedPdu]=rxComIPdus[ComRxPduId].ComIPduHandleId;
            rxindicationNumberOfRecievedPdu++;
        }
    }
}

BufReq_ReturnType Com_StartOfReception(PduIdType PduId,const PduInfoType *info,PduLengthType TpSduLength,PduLengthType *bufferSizePtr)
{
    ComIPdus[PduId];

    if((ComIPdus[PduId].ComIPduDirection==RECEIVE) && (ComIPdus[PduId].ComIPduType == TP))
    {
        //making sure that the buffer is unlocked
        if(!AsuIPdu->PduBufferState.Locked)
        {
            //making sure that we have the enough space for the sdu
            if(ComIPdus[PduId].ComIPduSize>=TpSduLength)
            {
                //lock the buffer until copying is done
                LOCKBUFFER(&AsuIPdu->PduBufferState);
                ///return the available buffer size
                *bufferSizePtr=ComIPdus[PduId].ComIPduSizee;
            }
            else
            {
                return BUFREQ_E_OVFL;
            }
        }
        else
        {
            //TODO: if there is a det
            //TODO: if there is no det function -> discard the data frame
            //TODO: return BUFREQ_E_NOT_OK
            return BUFREQ_E_BUSY;
        }
        return BUFREQ_OK;
    }
    return BUFREQ_E_NOT_OK;
}


//TODO: what we must do
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
            Com_PduUnpacking(id);
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
