/***************************************************
 * File Name: Com.c 
 * Author: AUTOSAR COM Team
 * Date Created: Jun 2020
 * Version  : 4.0
 ****************************************************/
#include "Com.h"
#include "Com_helper.h"
#include "Com_Buffer.h"
#include "PduR_Com.h"
#include "PduR.h"
#include "Com_Cbk.h"

#define SHADOW_BUFFER                               0
#define REAL_BUFFER                                 1

/*TODO: check which file must be in*/
#define ENTER_CRITICAL_SECTION()             __asm("    cpsie   i\n");
#define EXIT_CRITICAL_SECTION()              __asm("    cpsid   i\n");



#define COMPARE_DATA(SignalId,signalDataPtr,equalityCheck) do\
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
    }while(0)

#define NUMBER_OF_AUXILARY_ARR             2


typedef struct privateIPdu_type
{
    uint32  CurrentPosition;
    /*TODO:move updated to private TX*/
    boolean updated;
    boolean locked;
}privateIPdu_type;

typedef struct privateTxIPdu_type
{
    float32 remainingTimePeriod;
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    float32 minimumDelayTimer;
#endif
    uint8   numberOfRepetitionsLeft;
}privateTxIPdu_type;

typedef void (*notificationType)(void);

/* Com_Config declaration*/
uint8 com_pdur[] = {vcom};

extern const ComIPdu_type   ComIPdus[];
extern const ComSignal_type ComSignals[];
extern const ComTxIPdu_type ComTxIPdus[];
extern const ComSignalGroup_type ComSignalGroups[];

/* Global variables*/
static privateIPdu_type     privateIPdus[COM_NUM_OF_IPDU];
static privateTxIPdu_type   privateTxIPdus[COM_NUM_OF_TX_IPDU];
static uint16               txIPdusIds[COM_NUM_OF_TX_IPDU];
/*TODO: make number of signals here to be configured*/
static notificationType     pendingTxNotifications[NUMBER_OF_AUXILARY_ARR][COM_NUM_OF_SIGNAL];
static uint8                pendingTxNotificationsBufferIndex;
static uint16               pendingTxNotificationsNumber;

static uint8                rxIndicationProcessingDeferredPduIndex;
static uint16               rxindicationNumberOfRecievedPdu;
static PduIdType            rxDeferredPduArr[NUMBER_OF_AUXILARY_ARR][COM_NUM_OF_IPDU];

/*****************************************************************
 *                     Functions Definitions                     *
 *****************************************************************/

void Com_Init( const ComConfig_type* config)
{
    /* 1- loop on IPDUs */
    uint16 pduId;
    uint16 signalIndex;
    uint16 txIndex;

    txIndex=0;
    for ( pduId = 0; pduId<COM_NUM_OF_IPDU; pduId++) {

        if(ComIPdus[pduId].ComIPduDirection==SEND)
        {
            privateIPdus[pduId].updated=TRUE;
            txIPdusIds[txIndex]=pduId;
            txIndex++;
        }

        /* Initialize the memory with the default value.] */
        if (ComIPdus[pduId].ComIPduDirection == SEND)
        {
            memset((void *)ComIPdus[pduId].ComIPduDataPtr, ComTxIPdus[pduId].ComTxIPduUnusedAreasDefault, ComIPdus[pduId].ComIPduSize);
        }
    }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    for(txIndex=0;txIndex<COM_NUM_OF_TX_IPDU;txIndex++)
    {
        privateTxIPdus[txIndex].minimumDelayTimer=ComTxIPdus[txIndex].ComMinimumDelayTime;
    }
#endif
}

void Com_MainFunctionRx(void)
{
    uint16 mainRxNumberOfReceivedPdu;
    uint16 pduId;
    uint16 DeferredIndex;

    ENTER_CRITICAL_SECTION();
    mainRxNumberOfReceivedPdu=rxindicationNumberOfRecievedPdu;
    rxindicationNumberOfRecievedPdu=0;
    rxIndicationProcessingDeferredPduIndex^=1;
    EXIT_CRITICAL_SECTION();
    for(DeferredIndex=0;DeferredIndex<mainRxNumberOfReceivedPdu;DeferredIndex++)
    {
        pduId = rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex^1][DeferredIndex];
        /* copy the deferred buffer to the actual pdu buffer */
        privateIPdus[pduId].locked=TRUE;
        Com_PduUnpacking(pduId);
        privateIPdus[pduId].locked=FALSE;
    }
}

void Com_MainFunctionTx(void)
{
    const ComIPdu_type* IPdu;
    boolean mixed;
    uint16  sendIPduIndex;
    uint16  pendingIndex;
    uint16  mainTxPendingTxNotificationsNumber;

    for ( sendIPduIndex = 0; sendIPduIndex<COM_NUM_OF_TX_IPDU; sendIPduIndex++)
    {
        IPdu = GET_IPdu(txIPdusIds[sendIPduIndex]);

        mixed = FALSE;

        switch(ComTxIPdus[IPdu->ComTxIPdu].ComTxModeMode)
        {
        /* if the transmission mode is mixed */
        case MIXED:
            mixed = TRUE;
            /* no break because the mixed is periodic and direct */
            /* if the transmission mode is direct */
        case DIRECT:
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
            if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer < ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
            {
                privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer+=COM_TX_TIMEBASE;
            }
#endif
            if(privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft > 0)
            {
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
#endif
                    if(Com_TriggerIPDUSend(txIPdusIds[sendIPduIndex])== E_OK)
                    {
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                        privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer=0;
#endif
                        privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft--;
                    }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                }
#endif
            }
            if(mixed==FALSE)/* in case the Pdu is mixed don't break */
            {
                break;
            }
            /* if the transmission mode is periodic */
        case PERIODIC:
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
            if(mixed!=FALSE)
            {
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer < ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
                    privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer+=COM_TX_TIMEBASE;
                }
            }
#endif
            if(privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod<=0)
            {
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
#endif
                    if(Com_TriggerIPDUSend(txIPdusIds[sendIPduIndex]) == E_OK)
                    {
                        privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod = \
                                ComTxIPdus[IPdu->ComTxIPdu].ComTxModeTimePeriod;
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                        privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer=0;
#endif
                    }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                }
#endif
            }
            if (privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod > 0)
            {
                privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod = \
                        privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod - COM_TX_TIMEBASE;
            }
        }
    }
    ENTER_CRITICAL_SECTION();
    mainTxPendingTxNotificationsNumber=pendingTxNotificationsNumber;
    pendingTxNotificationsNumber=0;
    pendingTxNotificationsBufferIndex^=1;
    EXIT_CRITICAL_SECTION();

    for ( pendingIndex = 0; pendingIndex<mainTxPendingTxNotificationsNumber; pendingIndex++)
    {
        pendingTxNotifications[pendingTxNotificationsBufferIndex^1][pendingIndex]();
    }
}

/* Updates the signal object identified by SignalId with the signal referenced by the SignalDataPtr parameter */
uint8 Com_SendSignal( Com_SignalIdType SignalId, const void* SignalDataPtr )
{
    Std_ReturnType result;
    uint8 byteIndex;
    boolean signalUpdated;
    /* Get signal of "SignalId" */
    const ComSignal_type *Signal;

    /*Get IPdu of this signal */
    const ComIPdu_type* IPdu;
    privateIPdu_type* privateIPdu;
    result=E_OK;

    /* Get signal of "SignalId" */
    Signal = GET_Signal(SignalId);
    /*Get IPdu of this signal */
    IPdu = GET_IPdu(Signal->ComIPduHandleId);
    /* validate signalID */
    if((!validateSignalID(SignalId))&&(IPdu->ComIPduDirection==SEND) )
    {
        result=E_NOT_OK;
    }
    else
    {
        privateIPdu=&privateIPdus[Signal->ComIPduHandleId];
        signalUpdated=FALSE;
        switch(Signal->ComTransferProperty)
        {
        case TRIGGERED_WITHOUT_REPETITION:
            signalUpdated=TRUE;
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
                    signalUpdated=TRUE;
                }
                break;
            case UINT16:
            case SINT16:
                if((*((uint16*)(Signal->ComSignalDataPtr)))!=(*((uint16*)SignalDataPtr)))
                {
                    signalUpdated=TRUE;
                }
                break;
            case FLOAT32:
            case UINT32:
            case SINT32:
                if((*((uint32*)(Signal->ComSignalDataPtr)))!=(*((uint32*)SignalDataPtr)))
                {
                    signalUpdated=TRUE;
                }
                break;
            case FLOAT64:
            case UINT64:
            case SINT64:
                if((*((uint64*)(Signal->ComSignalDataPtr)))!=(*((uint64*)SignalDataPtr)))
                {
                    signalUpdated=TRUE;
                }
                else
                {

                }
                break;
            case UINT8_N:
                for(byteIndex=0;byteIndex<Signal->ComSignalLength;byteIndex++)
                {
                    if(((uint8*)(Signal->ComSignalDataPtr))[byteIndex]!=((uint8*)SignalDataPtr)[byteIndex])
                    {
                        signalUpdated=TRUE;
                        break;
                    }
                    else
                    {

                    }
                }
            }
        }

        /* update the Signal buffer with the signal data */
        if(TRUE==signalUpdated)
        {
            privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = 1;
            privateIPdu->updated=TRUE;
            Com_WriteSignalDataToSignalBuffer(SignalId, SignalDataPtr);
            if(Signal->ComUpdateBitEnabled!=FALSE)
            {
                /* Set the update bit of this signal */
                SETBIT(IPdu->ComIPduDataPtr, Signal->ComUpdateBitPosition);
            }
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

BufReq_ReturnType Com_CopyTxData( PduIdType PduId, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr )
{
    BufReq_ReturnType result = BUFREQ_OK;
    uint8 * source;
    if((ComIPdus[PduId].ComIPduDirection == SEND) && \
            ((privateIPdus[PduId].CurrentPosition + info->SduLength) <= (ComIPdus[PduId].ComIPduSize)))
    {
        source = (uint8*)ComIPdus[PduId].ComIPduDataPtr;
        source += privateIPdus[PduId].CurrentPosition;
        LOCKBUFFER(&privateIPdus[PduId].locked);
        memcpy((void*) info->SduDataPtr, (void*)source, info->SduLength);
        privateIPdus[PduId].CurrentPosition += info->SduLength;
        *availableDataPtr = ComIPdus[PduId].ComIPduSize - privateIPdus[PduId].CurrentPosition;
        result = BUFREQ_OK;
    }
    else
    {
        /*TODO: If not enough transmit data is available, no data is copied
         by the upper layer module and BUFREQ_E_BUSY is returned.*/
        result = BUFREQ_E_NOT_OK;
    }
    return result;
}

BufReq_ReturnType Com_CopyRxData( PduIdType PduID, const PduInfoType* info, PduLengthType* bufferSizePtr )
{
    BufReq_ReturnType result = BUFREQ_OK;
    uint8* destination;

    if((ComIPdus[PduID].ComIPduDirection == RECEIVE) &&\
            (ComIPdus[PduID].ComIPduSize - privateIPdus[PduID].CurrentPosition >= info->SduLength ))
    {
        /*TODO: In case of SduLength = 0 --> ??? */
        /*TODO: In case of the SduDataPtr == NULL_PTR */
        destination = (uint8 *) ComIPdus[PduID].ComIPduDataPtr;
        destination += privateIPdus[PduID].CurrentPosition;
        if((info->SduDataPtr != NULL_PTR) && (info->SduLength !=0))
        {
            memcpy(destination, info->SduDataPtr, info->SduLength);
            privateIPdus[PduID].CurrentPosition += info->SduLength;
            *bufferSizePtr = ComIPdus[PduID].ComIPduSize - privateIPdus[PduID].CurrentPosition;
        }
        result = BUFREQ_OK;
    }
    else
    {
        result = BUFREQ_E_NOT_OK;
    }
    return result;
}

Std_ReturnType Com_TriggerIPDUSend( PduIdType PduId )
{
    const ComIPdu_type* IPdu;
    PduInfoType PduInfoPackage;
    uint8 signalIndex;
    uint8 signalGroupIndex;
    Std_ReturnType result;
    privateIPdu_type* privateIPdu;

    result=E_OK;
    privateIPdu=&privateIPdus[PduId];

#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
    {
#endif

        if (privateIPdu->locked)
        {
            result=E_NOT_OK;
        }
        else
        {
            privateIPdu->locked=TRUE;
            if(privateIPdu->updated)
            {
                privateIPdu->updated=FALSE;
                Com_PackSignalsToPdu(PduId);
            }
            IPdu = GET_IPdu(PduId);
            PduInfoPackage.SduDataPtr = (uint8 *)IPdu->ComIPduDataPtr;
            PduInfoPackage.SduLength = IPdu->ComIPduSize;
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
                for(signalGroupIndex=0;signalGroupIndex<IPdu->ComIPduNumberOfSignalGroups;signalGroupIndex++)
                {
                    if(IPdu->ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled!=FALSE)
                    {
                        CLEARBIT(IPdu->ComIPduDataPtr,IPdu->ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitPosition);
                    }
                }
            }
        }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    }
    else
    {
        result=E_NOT_OK;
    }
#endif
    return result;
}

void Com_RxIndication(PduIdType ComRxPduId, const PduInfoType* PduInfoPtr)
{
    memcpy(ComIPdus[ComRxPduId].ComIPduDataPtr, PduInfoPtr->SduDataPtr, ComIPdus[ComRxPduId].ComIPduSize);
    if((ComIPdus[ComRxPduId].ComIPduDirection == RECEIVE ) && (ComIPdus[ComRxPduId].ComIPduType == NORMAL))
    {
        if(ComIPdus[ComRxPduId].ComIPduSignalProcessing == IMMEDIATE)
        {
            privateIPdus[ComRxPduId].locked=TRUE;
            Com_PduUnpacking(ComRxPduId);
            privateIPdus[ComRxPduId].locked=FALSE;
        }
        else
        {
            ENTER_CRITICAL_SECTION();
            rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex][rxindicationNumberOfRecievedPdu]=ComRxPduId;
            rxindicationNumberOfRecievedPdu++;
            EXIT_CRITICAL_SECTION();
        }
    }
}

BufReq_ReturnType Com_StartOfReception(PduIdType PduId,const PduInfoType *info,PduLengthType TpSduLength,PduLengthType *bufferSizePtr)
{
    BufReq_ReturnType result=BUFREQ_OK;
    if((ComIPdus[PduId].ComIPduDirection==RECEIVE) && (ComIPdus[PduId].ComIPduType == TP))
    {
        //making sure that the buffer is unlocked
        if(!privateIPdus[PduId].locked)
        {
            //making sure that we have the enough space for the sdu
            if(ComIPdus[PduId].ComIPduSize>=TpSduLength)
            {
                privateIPdus[PduId].locked=TRUE;
                /* Initialize the current position */
                privateIPdus[PduId].CurrentPosition = 0;
                ///return the available buffer size
                *bufferSizePtr=ComIPdus[PduId].ComIPduSize;
                result = BUFREQ_OK;
            }
            else
            {
                result = BUFREQ_E_OVFL;
            }
        }
        else
        {
            /*TODO: if there is a det
              TODO: if there is no det function -> discard the data frame
              TODO: return BUFREQ_E_NOT_OK*/
            result = BUFREQ_E_BUSY;
        }
    }
    else
    {
        result = BUFREQ_E_NOT_OK;
    }
    return result;

}

void Com_TpRxIndication(PduIdType ComRxPduId,Std_ReturnType Result)
{
    if (Result == E_OK)
    {
        if((ComIPdus[ComRxPduId].ComIPduDirection == RECEIVE) && (ComIPdus[ComRxPduId].ComIPduType == TP))
        {
            if(ComIPdus[ComRxPduId].ComIPduSignalProcessing == IMMEDIATE)
            {
                Com_PduUnpacking(ComRxPduId);
                privateIPdus[ComRxPduId].locked=FALSE;
            }
            else
            {
                ENTER_CRITICAL_SECTION();
                rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex][rxindicationNumberOfRecievedPdu]=ComRxPduId;
                rxindicationNumberOfRecievedPdu++;
                EXIT_CRITICAL_SECTION();
                privateIPdus[ComRxPduId].locked=FALSE;
            }
        }
    }
}

void Com_TpTxConfirmation( PduIdType TxPduId, Std_ReturnType result )
{
    uint8 signalIndex;
    uint8 signalGroupIndex;
    if((ComIPdus[TxPduId].ComIPduDirection==SEND) && (ComIPdus[TxPduId].ComIPduType == TP))
    {
        if(result==E_OK)
        {
            for ( signalIndex = 0 ; signalIndex < ComIPdus[TxPduId].ComIPduNumOfSignals ; signalIndex++ )
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
                {
                    CLEARBIT(ComIPdus[TxPduId].ComIPduDataPtr, ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
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
                    if(ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComNotification)
                    {
                        ENTER_CRITICAL_SECTION();
                        pendingTxNotifications[pendingTxNotificationsBufferIndex][pendingTxNotificationsNumber]=\
                                ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComNotification;
                        pendingTxNotificationsNumber++;
                        EXIT_CRITICAL_SECTION();
                    }
                }
            }
            for(signalGroupIndex=0;signalGroupIndex<ComIPdus[TxPduId].ComIPduNumberOfSignalGroups;signalGroupIndex++)
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled!=FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
                {
                    CLEARBIT(ComIPdus[TxPduId].ComIPduDataPtr, ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
                }
                if(ComIPdus[TxPduId].ComIPduSignalProcessing==IMMEDIATE)
                {
                    if(ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification)
                    {
                        ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification();
                    }
                }
                else
                {
                    if(ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification)
                    {
                        ENTER_CRITICAL_SECTION();
                        pendingTxNotifications[pendingTxNotificationsBufferIndex][pendingTxNotificationsNumber]=\
                                ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification;
                        pendingTxNotificationsNumber++;
                        EXIT_CRITICAL_SECTION();
                    }
                }
            }
        }
        privateIPdus[TxPduId].locked=FALSE;
    }
}

void Com_TxConfirmation( PduIdType TxPduId, Std_ReturnType result )
{
    uint8 signalIndex;
    uint8 signalGroupIndex;
    if((ComIPdus[TxPduId].ComIPduDirection==SEND) && (ComIPdus[TxPduId].ComIPduType == TP))
    {
        if(result==E_OK)
        {
            for ( signalIndex = 0 ; signalIndex < ComIPdus[TxPduId].ComIPduNumOfSignals ; signalIndex++ )
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
                {
                    CLEARBIT(ComIPdus[TxPduId].ComIPduDataPtr, ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
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
                    if(ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComNotification)
                    {
                        ENTER_CRITICAL_SECTION();
                        pendingTxNotifications[pendingTxNotificationsBufferIndex][pendingTxNotificationsNumber]=\
                                ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComNotification;
                        pendingTxNotificationsNumber++;
                        EXIT_CRITICAL_SECTION();
                    }
                }
            }
            for(signalGroupIndex=0;signalGroupIndex<ComIPdus[TxPduId].ComIPduNumberOfSignalGroups;signalGroupIndex++)
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled!=FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
                {
                    CLEARBIT(ComIPdus[TxPduId].ComIPduDataPtr, ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
                }
                if(ComIPdus[TxPduId].ComIPduSignalProcessing==IMMEDIATE)
                {
                    if(ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification)
                    {
                        ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification();
                    }
                }
                else
                {
                    if(ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification)
                    {
                        ENTER_CRITICAL_SECTION();
                        pendingTxNotifications[pendingTxNotificationsBufferIndex][pendingTxNotificationsNumber]=\
                                ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComNotification;
                        pendingTxNotificationsNumber++;
                        EXIT_CRITICAL_SECTION();
                    }
                }
            }
        }
        privateIPdus[TxPduId].locked=FALSE;
    }
}


uint8 Com_SendSignalGroup( Com_SignalGroupIdType SignalGroupId )
{
    uint8 result;
    uint8 signalIndex;

    result=E_OK;
    if((SignalGroupId<COM_NUM_OF_GROUP_SIGNAL)&&(privateIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].locked))
    {
        /*TODO: add lock to all pdus packing process*/
        for(signalIndex=0;signalIndex< ComSignalGroups[SignalGroupId].ComIPduNumberOfGroupSignals;signalIndex++)
        {
            switch(ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalType)
            {
            case BOOLEAN:
            case SINT8:
            case UINT8:
                ((uint8*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER]=\
                ((uint8*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER];
                break;
            case FLOAT32:
            case UINT32:
            case SINT32:
                ((uint32*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER]=\
                ((uint32*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER];
                break;
            case FLOAT64:
            case UINT64:
            case SINT64:
                ((uint64*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER]=\
                ((uint64*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER];
                break;
            case UINT16:
            case SINT16:
                ((uint16*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER]=\
                ((uint16*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER];
                break;
            case UINT8_N:
                memcpy((void*)(((uint8*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)+\
                        ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalLength)\
                        ,ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr,\
                        ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalLength);
            }
        }
    }
    else
    {
        result=E_NOT_OK;
    }
    return result;
}

uint8 Com_ReceiveSignalGroup( Com_SignalGroupIdType SignalGroupId )
{
    uint8 result;
    uint8 signalIndex;

    result=E_OK;
    if((SignalGroupId<COM_NUM_OF_GROUP_SIGNAL)&&(privateIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].locked))
    {
        /*TODO: add lock to all pdus packing process*/
        for(signalIndex=0;signalIndex< ComSignalGroups[SignalGroupId].ComIPduNumberOfGroupSignals;signalIndex++)
        {
            switch(ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalType)
            {
            case BOOLEAN:
            case SINT8:
            case UINT8:
                ((uint8*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER]=\
                ((uint8*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER];
                break;
            case FLOAT32:
            case UINT32:
            case SINT32:
                ((uint32*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER]=\
                ((uint32*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER];
                break;
            case FLOAT64:
            case UINT64:
            case SINT64:
                ((uint64*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER]=\
                ((uint64*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER];
                break;
            case UINT16:
            case SINT16:
                ((uint16*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[SHADOW_BUFFER]=\
                ((uint16*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)[REAL_BUFFER];
                break;
            case UINT8_N:
                memcpy(ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr,\
                       (void*)(((uint8*)ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalDataPtr)+\
                               ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalLength),\
                               ComSignalGroups[SignalGroupId].ComIPduSignalRef[signalIndex].ComSignalLength);
            }
        }
    }
    else
    {
        result=E_NOT_OK;
    }
    return result;
}
