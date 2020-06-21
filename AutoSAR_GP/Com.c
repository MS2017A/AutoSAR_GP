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

#define NUMBER_OF_AUXILARY_ARR             2


typedef struct
{
    uint32  CurrentPosition;
    /*TODO:move updated to private TX*/
    boolean updated;
    boolean locked;
}privateIPdu_type;

typedef struct
{
    float32 remainingTimePeriod;
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    float32 minimumDelayTimer;
#endif
    uint8   numberOfRepetitionsLeft;
}privateTxIPdu_type;

typedef void (*notificationType)(void);

/* Com_Config declaration*/
PduIdType com_pdur[] = {vcom};

extern const ComIPdu_type   ComIPdus[COM_NUM_OF_IPDU];
extern const ComSignal_type ComSignals[COM_NUM_OF_SIGNAL];
extern const ComTxIPdu_type ComTxIPdus[COM_NUM_OF_TX_IPDU];
extern const ComSignalGroup_type ComSignalGroups[COM_NUM_OF_GROUP_SIGNAL];

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
    uint16 signalGroupIndex;
    uint16 txIndex;
    uint32 Counter;

    txIndex=(uint16)0;
    for ( pduId = (uint16)0; pduId<(uint16)COM_NUM_OF_IPDU; pduId++)
    {
        if(ComIPdus[pduId].ComIPduDirection==SEND)
        {
            privateIPdus[pduId].updated=(boolean)TRUE;
            txIPdusIds[txIndex]=pduId;
            txIndex++;
        }

        /* Initialize the memory with the default value.] */
        if (ComIPdus[pduId].ComIPduDirection == SEND)
        {
            for(Counter=(uint32)0x00;Counter<ComIPdus[pduId].ComIPduSize;Counter++)
            {
                *(uint8*)ComIPdus[pduId].ComIPduDataPtr=ComTxIPdus[pduId].ComTxIPduUnusedAreasDefault;
            }
        }

        /* For each signal in this PDU */
        for ( signalIndex = (uint16)0; (uint16)ComIPdus[pduId].ComIPduNumOfSignals > signalIndex; signalIndex++)
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
        for ( signalGroupIndex = (uint16)0; (uint16)ComIPdus[pduId].ComIPduNumberOfSignalGroups > signalGroupIndex; signalGroupIndex++)
        {
            /* Check for the update Bit is enabled or disabled */
            if((ComIPdus[pduId].ComIPduSignalGroupRef)[signalGroupIndex].ComUpdateBitEnabled)
            {
                /* Clear update bits */
                CLEARBIT(ComIPdus[pduId].ComIPduDataPtr, (ComIPdus[pduId].ComIPduSignalGroupRef)[signalGroupIndex].ComUpdateBitPosition);
            }
            else
            {
                /* For MISRA rules */
            }
        }
    }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    for(txIndex=(uint16)0;txIndex<(uint16)COM_NUM_OF_TX_IPDU;txIndex++)
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
    rxindicationNumberOfRecievedPdu=(uint16)0x00;
    rxIndicationProcessingDeferredPduIndex^=(uint8)1;
    EXIT_CRITICAL_SECTION();
    for(DeferredIndex=(uint16)0x00 ; DeferredIndex<mainRxNumberOfReceivedPdu ; DeferredIndex++)
    {
        pduId =  rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex^((uint8)0x01)][DeferredIndex];
        /* copy the deferred buffer to the actual pdu buffer */
        privateIPdus[pduId].locked=(boolean)TRUE;
        Com_PduUnpacking(pduId);
        privateIPdus[pduId].locked=(boolean)FALSE;
    }
}

void Com_MainFunctionTx(void)
{
    const ComIPdu_type* IPdu;
    boolean mixed;
    uint16  sendIPduIndex;
    uint16  pendingIndex;
    uint16  mainTxPendingTxNotificationsNumber;

    for ( sendIPduIndex = (uint16)0x00; sendIPduIndex<(uint16)COM_NUM_OF_TX_IPDU; sendIPduIndex++)
    {
        IPdu = GET_IPdu(txIPdusIds[sendIPduIndex]);

        mixed = (boolean)FALSE;

        switch(ComTxIPdus[IPdu->ComTxIPdu].ComTxModeMode)
        {
        /* if the transmission mode is mixed */
        case MIXED:
            /*Violate MISRA rules intentionally: MISRA C-2004:15.2 */
            mixed = (boolean)TRUE;
            /* no break because the mixed is periodic and direct */
            /* if the transmission mode is direct */
        case DIRECT:
            /*Violate MISRA rules intentionally: MISRA C-2004:15.2 */
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
            if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer < ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
            {
                privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer+=COM_TX_TIMEBASE;
            }
#endif
            if(privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft > (uint8)0x00)
            {
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
#endif
                    if(Com_TriggerIPDUSend(txIPdusIds[sendIPduIndex])== E_OK)
                    {
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                        privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer=(float32)0.0;
#endif
                        privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft--;
                    }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                }
#endif
            }
            if(mixed==(boolean)FALSE)/* in case the Pdu is mixed don't break */
            {
                break;
            }
            /* if the transmission mode is periodic */
        case PERIODIC:
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
            if(mixed!=(boolean)FALSE)
            {
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer < ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
                    privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer+=COM_TX_TIMEBASE;
                }
            }
#endif
            if(privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod<=(float32)0.0)
            {
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                if(privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer >= ComTxIPdus[IPdu->ComTxIPdu].ComMinimumDelayTime)
                {
#endif
                    if(Com_TriggerIPDUSend(txIPdusIds[sendIPduIndex]) == E_OK)
                    {
                        privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod = \
                                (float32)ComTxIPdus[IPdu->ComTxIPdu].ComTxModeTimePeriod;
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                        privateTxIPdus[IPdu->ComTxIPdu].minimumDelayTimer=(float32)0.0;
#endif
                    }
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
                }
#endif
            }
            if (privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod > (float32)0.0)
            {
                privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod = \
                        privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod - COM_TX_TIMEBASE;
            }
            break;
        default:
            break;
        }
    }
    ENTER_CRITICAL_SECTION();
    mainTxPendingTxNotificationsNumber=pendingTxNotificationsNumber;
    pendingTxNotificationsNumber=(uint16)0x00;
    pendingTxNotificationsBufferIndex^=(uint8)0x01;
    EXIT_CRITICAL_SECTION();

    for ( pendingIndex = (uint16)0x00; pendingIndex<mainTxPendingTxNotificationsNumber; pendingIndex++)
    {
        pendingTxNotifications[pendingTxNotificationsBufferIndex^(uint8)0x01][pendingIndex]();
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
        signalUpdated=(boolean)FALSE;
        switch(Signal->ComTransferProperty)
        {
        case TRIGGERED_WITHOUT_REPETITION:
            signalUpdated=(boolean)TRUE;
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
                Asu_IPdu->Com_Asu_Pdu_changed = (boolean)FALSE;
            }
            break;
#endif
        case TRIGGERED_ON_CHANGE_WITHOUT_REPETITION:
            switch(Signal->ComSignalType)
            {
            case BOOLEAN:
            case UINT8:
            case SINT8:
                if((*((uint8*)(Signal->ComSignalDataPtr)))!=(*((const uint8*)SignalDataPtr)))
                {
                    signalUpdated=(boolean)TRUE;
                }
                break;
            case UINT16:
            case SINT16:
                if((*((uint16*)(Signal->ComSignalDataPtr)))!=(*((const uint16*)SignalDataPtr)))
                {
                    signalUpdated=(boolean)TRUE;
                }
                break;
            case FLOAT32:
            case UINT32:
            case SINT32:
                if((*((uint32*)(Signal->ComSignalDataPtr)))!=(*((const uint32*)SignalDataPtr)))
                {
                    signalUpdated=(boolean)TRUE;
                }
                break;
            case FLOAT64:
            case UINT64:
            case SINT64:
                if((*((uint64*)(Signal->ComSignalDataPtr)))!=(*((const uint64*)SignalDataPtr)))
                {
                    signalUpdated=(boolean)TRUE;
                }
                else
                {

                }
                break;
            case UINT8_N:
                for(byteIndex=(uint8)0x00;byteIndex<Signal->ComSignalLength;byteIndex++)
                {
                    if(((uint8*)(Signal->ComSignalDataPtr))[byteIndex]!=((const uint8*)SignalDataPtr)[byteIndex])
                    {
                        signalUpdated=(boolean)TRUE;
                        break;
                    }
                    else
                    {

                    }
                }
                break;
            default:
                break;
            }
            break;
            default:
                break;
        }

        /* update the Signal buffer with the signal data */
        if((boolean)TRUE==signalUpdated)
        {
            privateTxIPdus[IPdu->ComTxIPdu].numberOfRepetitionsLeft = (uint8)0x01;
            privateIPdu->updated=(boolean)TRUE;
            Com_WriteSignalDataToSignalBuffer(SignalId, SignalDataPtr);
            if(Signal->ComUpdateBitEnabled!=(boolean)FALSE)
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
    uint8 result = E_OK;
    /* validate signalID */
    if(!validateSignalID(SignalId))
    {
        result = E_NOT_OK;
    }
    else
    {
        /* check ipdu direction is receive */
        if(ComIPdus[ComSignals[SignalId].ComIPduHandleId].ComIPduDirection == RECEIVE)
        {
            Com_ReadSignalDataFromSignalBuffer(SignalId, SignalDataPtr);
        }
        else
        {
            result = E_NOT_OK;
        }
    }

    return result;
}

BufReq_ReturnType Com_CopyTxData( PduIdType PduId, const PduInfoType* info, const RetryInfoType* retry, PduLengthType* availableDataPtr )
{
    BufReq_ReturnType result = BUFREQ_OK;
    uint8 * source;
    if((ComIPdus[PduId].ComIPduDirection == SEND) && \
            (((uint32)privateIPdus[PduId].CurrentPosition + info->SduLength) <= (uint32)(ComIPdus[PduId].ComIPduSize)))
    {
        source = (uint8*)ComIPdus[PduId].ComIPduDataPtr;
        source += privateIPdus[PduId].CurrentPosition;
        privateIPdus[PduId].locked=(boolean)TRUE;
        memcpy((void*) info->SduDataPtr, (void*)source, info->SduLength);
        privateIPdus[PduId].CurrentPosition += info->SduLength;
        *availableDataPtr = (PduLengthType)ComIPdus[PduId].ComIPduSize - (PduLengthType)privateIPdus[PduId].CurrentPosition;
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
            ((uint32)ComIPdus[PduID].ComIPduSize - (uint32)privateIPdus[PduID].CurrentPosition >= info->SduLength ))
    {
        /*TODO: In case of SduLength = 0 --> ??? */
        /*TODO: In case of the SduDataPtr == NULL_PTR */
        destination = (uint8 *) ComIPdus[PduID].ComIPduDataPtr;
        destination += privateIPdus[PduID].CurrentPosition;
        if((info->SduDataPtr != NULL_PTR) && (info->SduLength !=(uint32)0x00))
        {
            memcpy(destination, info->SduDataPtr, info->SduLength);
            privateIPdus[PduID].CurrentPosition += info->SduLength;
            *bufferSizePtr = (uint32)ComIPdus[PduID].ComIPduSize - privateIPdus[PduID].CurrentPosition;
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
    IPdu=GET_IPdu(PduId);

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
            privateIPdu->locked=(boolean)TRUE;
            if(privateIPdu->updated)
            {
                privateIPdu->updated=(boolean)FALSE;
                Com_PackSignalsToPdu(PduId);
            }
            IPdu = GET_IPdu(PduId);
            PduInfoPackage.SduDataPtr = (uint8 *)IPdu->ComIPduDataPtr;
            PduInfoPackage.SduLength = (uint32)IPdu->ComIPduSize;
            if (PduR_ComTransmit(com_pdur[PduId], &PduInfoPackage) != (Std_ReturnType)E_OK)
            {
                result=E_NOT_OK;
            }
            else
            {

            }
            if((ComTxIPdus[IPdu->ComTxIPdu].ComTxIPduClearUpdateBit == TRIGGER_TRANSMIT)||\
                    ((ComTxIPdus[IPdu->ComTxIPdu].ComTxIPduClearUpdateBit == TRANSMIT)&&(result==E_OK)))
            {
                for ( signalIndex = (uint8)0x00 ; signalIndex < IPdu->ComIPduNumOfSignals ; signalIndex++ )
                {
                    if(IPdu->ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=(boolean)FALSE)/*Update bit is enabled*/
                    {
                        CLEARBIT(IPdu->ComIPduDataPtr, IPdu->ComIPduSignalRef[signalIndex].ComUpdateBitPosition);
                    }
                }
                for(signalGroupIndex=(uint8)0;signalGroupIndex<IPdu->ComIPduNumberOfSignalGroups;signalGroupIndex++)
                {
                    if(IPdu->ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled!=(boolean)FALSE)
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
    memcpy(ComIPdus[ComRxPduId].ComIPduDataPtr, PduInfoPtr->SduDataPtr, (uint32)ComIPdus[ComRxPduId].ComIPduSize);
    if((ComIPdus[ComRxPduId].ComIPduDirection == RECEIVE ) && (ComIPdus[ComRxPduId].ComIPduType == NORMAL))
    {
        if(ComIPdus[ComRxPduId].ComIPduSignalProcessing == IMMEDIATE)
        {
            privateIPdus[ComRxPduId].locked=(boolean)TRUE;
            Com_PduUnpacking(ComRxPduId);
            privateIPdus[ComRxPduId].locked=(boolean)FALSE;
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
                privateIPdus[PduId].locked=(boolean)TRUE;
                /* Initialize the current position */
                privateIPdus[PduId].CurrentPosition = (uint32)0;
                ///return the available buffer size
                *bufferSizePtr=(uint32)ComIPdus[PduId].ComIPduSize;
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
                privateIPdus[ComRxPduId].locked=(boolean)FALSE;
            }
            else
            {
                ENTER_CRITICAL_SECTION();
                rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex][rxindicationNumberOfRecievedPdu]=ComRxPduId;
                rxindicationNumberOfRecievedPdu++;
                EXIT_CRITICAL_SECTION();
                privateIPdus[ComRxPduId].locked=(boolean)FALSE;
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
            for ( signalIndex = (uint8)0 ; signalIndex < ComIPdus[TxPduId].ComIPduNumOfSignals ; signalIndex++ )
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=(boolean)FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
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
            for(signalGroupIndex=(uint8)0x00;signalGroupIndex<ComIPdus[TxPduId].ComIPduNumberOfSignalGroups;signalGroupIndex++)
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled!=(boolean)FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
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
        privateIPdus[TxPduId].locked=(boolean)FALSE;
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
            for ( signalIndex = (uint8)0x00 ; signalIndex < ComIPdus[TxPduId].ComIPduNumOfSignals ; signalIndex++ )
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalRef[signalIndex].ComUpdateBitEnabled!=(boolean)FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
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
            for(signalGroupIndex=(uint8)0;signalGroupIndex<ComIPdus[TxPduId].ComIPduNumberOfSignalGroups;signalGroupIndex++)
            {
                /*Update bit is enabled*/
                if((ComIPdus[TxPduId].ComIPduSignalGroupRef[signalGroupIndex].ComUpdateBitEnabled!=(boolean)FALSE)&&(ComTxIPdus[ComIPdus[TxPduId].ComTxIPdu].ComTxIPduClearUpdateBit==CONFIRMATION))
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
        privateIPdus[TxPduId].locked=(boolean)FALSE;
    }
}


uint8 Com_SendSignalGroup( Com_SignalGroupIdType SignalGroupId )
{
    uint8 result;
    uint8 signalIndex;

    result=E_OK;
    if((SignalGroupId<(Com_SignalGroupIdType)COM_NUM_OF_GROUP_SIGNAL)&&(privateIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].locked))
    {
        /*TODO: add lock to all pdus packing process*/
        for(signalIndex=(uint8)0;signalIndex< ComSignalGroups[SignalGroupId].ComIPduNumberOfGroupSignals;signalIndex++)
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
                break;
            default:
                break;
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
    if((SignalGroupId<(Com_SignalGroupIdType)COM_NUM_OF_GROUP_SIGNAL)&&(privateIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].locked))
    {
        /*TODO: add lock to all pdus packing process*/
        for(signalIndex=(uint8)0;signalIndex< ComSignalGroups[SignalGroupId].ComIPduNumberOfGroupSignals;signalIndex++)
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
                break;
            default:
                break;
            }
        }
    }
    else
    {
        result=E_NOT_OK;
    }
    return result;
}
