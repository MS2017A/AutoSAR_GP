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
#include "Det.h"

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

typedef P2FUNC(void, ptrclass, notificationType)(void);

LOCAL FUNC(void, memclass) privateGeneralTxConfirmation(VAR(PduIdType,memclass) TxPduId);

/* Com_Config declaration*/
VAR(PduIdType,memclass) com_pdur[] = {vcom};

extern CONST(ComIPdu_type, memclass)          ComIPdus[COM_NUM_OF_IPDU];
extern CONST(ComSignal_type, memclass)        ComSignals[COM_NUM_OF_SIGNAL];
extern CONST(ComTxIPdu_type, memclass)        ComTxIPdus[COM_NUM_OF_TX_IPDU];
extern CONST(ComSignalGroup_type, memclass)   ComSignalGroups[COM_NUM_OF_GROUP_SIGNAL];

/* Global variables*/
LOCAL VAR(privateIPdu_type, memclass)         privateIPdus[COM_NUM_OF_IPDU];
LOCAL VAR(privateTxIPdu_type, memclass)       privateTxIPdus[COM_NUM_OF_TX_IPDU];
LOCAL VAR(uint16, memclass)                   txIPdusIds[COM_NUM_OF_TX_IPDU];
/*TODO: make number of signals here to be configured*/
LOCAL VAR(notificationType, memclass)         pendingTxNotifications[NUMBER_OF_AUXILARY_ARR][COM_NUM_OF_SIGNAL];
LOCAL VAR(uint8, memclass)                    pendingTxNotificationsBufferIndex;
LOCAL VAR(uint16, memclass)                   pendingTxNotificationsNumber;

LOCAL VAR(uint8, memclass)                    rxIndicationProcessingDeferredPduIndex;
LOCAL VAR(uint16, memclass)                   rxindicationNumberOfRecievedPdu;
LOCAL VAR(PduIdType, memclass)                rxDeferredPduArr[NUMBER_OF_AUXILARY_ARR][COM_NUM_OF_IPDU];

/*****************************************************************
 *                     Functions Definitions                     *
 *****************************************************************/

FUNC(void, memclass)
Com_Init(CONSTP2VAR(ComConfig_type, memclass, ptrclass) config)
{
    /* 1- loop on IPDUs */
    VAR(uint16, memclass) pduId;
    VAR(uint16, memclass) signalIndex;
    VAR(uint16, memclass) signalGroupIndex;
    VAR(uint16, memclass) txIndex;
    VAR(uint32, memclass) Counter;

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

FUNC(void, memclass)
Com_MainFunctionRx(VAR(void, memclass))
{
    VAR(uint16, memclass) mainRxNumberOfReceivedPdu;
    VAR(uint16, memclass) pduId;
    VAR(uint16, memclass) DeferredIndex;

    ENTER_CRITICAL_SECTION();
    mainRxNumberOfReceivedPdu=rxindicationNumberOfRecievedPdu;

    /*TODO:comment*/
    rxindicationNumberOfRecievedPdu=(uint16)0x00;
    /*TODO:flip rx indication buffer macro*/
    rxIndicationProcessingDeferredPduIndex^=(uint8)1;
    EXIT_CRITICAL_SECTION();
    for(DeferredIndex=(uint16)0x00 ; DeferredIndex<mainRxNumberOfReceivedPdu ; DeferredIndex++)
    {
        pduId =  rxDeferredPduArr[rxIndicationProcessingDeferredPduIndex^((uint8)0x01)][DeferredIndex];
        /* copy the deferred buffer to the actual pdu buffer TODO: remove lock */
        privateIPdus[pduId].locked=(boolean)TRUE;
        Com_PduUnpacking(pduId);
        privateIPdus[pduId].locked=(boolean)FALSE;
    }
}

FUNC(void, memclass)
Com_MainFunctionTx(VAR(void, memclass))
{
    AUTOMATIC CONSTP2VAR(ComIPdu_type, memclass, ptrclass) IPdu;
    VAR(boolean, memclass)                       mixed;
    VAR(boolean, memclass)                       sent;
    VAR(uint16, memclass)                        sendIPduIndex;
    VAR(uint16, memclass)                        pendingIndex;
    VAR(uint16, memclass)                        mainTxPendingTxNotificationsNumber;

    for ( sendIPduIndex = (uint16)0x00; sendIPduIndex<(uint16)COM_NUM_OF_TX_IPDU; sendIPduIndex++)
    {
        IPdu = GET_IPdu(txIPdusIds[sendIPduIndex]);
        /*TODO: make one pdu to be sent once*/
        /*TODO: make function for menimum delay time*/
        mixed = (boolean)FALSE;
        sent  = (boolean)FALSE;

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
                        sent=(boolean)TRUE;
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
            if((privateTxIPdus[IPdu->ComTxIPdu].remainingTimePeriod<=(float32)0.0)&&(sent==FALSE))
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
                else
                {
#if COM_DEV_ERROR_DETECT
                    Det_ReportError(COM_MODULE_ID, COM_INSTANCE_ID, COM_MAIN_FUNCTION_TX, COM_E_SKIPPED_TRANSMISSION);
#endif
                }
#endif
            }
            else
            {
#if COM_DEV_ERROR_DETECT
                if(sent==TRUE)
                {
                    Det_ReportError(COM_MODULE_ID, COM_INSTANCE_ID, COM_MAIN_FUNCTION_TX, COM_E_SKIPPED_TRANSMISSION);
                }
                else
                {

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
FUNC(uint8, memclass)
Com_SendSignal( VAR(Com_SignalIdType, memclass) SignalId, CONSTP2VAR(void, memclass, ptrclass) SignalDataPtr )
{
    VAR(Std_ReturnType, memclass)                   result;
    VAR(uint8, memclass)                            byteIndex;
    VAR(boolean, memclass)                          signalUpdated;
    /* Get signal of "SignalId" */
    AUTOMATIC CONSTP2VAR(ComSignal_type, memclass, ptrclass)  Signal;

    /*Get IPdu of this signal */
    AUTOMATIC CONSTP2VAR(ComIPdu_type, memclass, ptrclass)    IPdu;
    AUTOMATIC P2VAR(privateIPdu_type, memclass, ptrclass)     privateIPdu;
    result=E_OK;

    /* validate signalID */
    if((!validateSignalID(SignalId))&&(IPdu->ComIPduDirection!=SEND) )
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_SEND_SIGNAL_ID,COM_E_PARAM);
#endif
        result=E_NOT_OK;
    }
    else
    {
        if(SignalDataPtr)
        {
            /* Get signal of "SignalId" */
            Signal = GET_Signal(SignalId);
            /*Get IPdu of this signal */
            IPdu = GET_IPdu(Signal->ComIPduHandleId);
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
                else
                {

                }
            }
            else
            {

            }
        }
        else
        {
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_SEND_SIGNAL_ID,COM_E_PARAM_POINTER);
#endif
            result=E_NOT_OK;
        }
    }
    return result;
}

/* Copies the data of the signal identified by SignalId to the location specified by SignalDataPtr */
FUNC(uint8, memclass)
Com_ReceiveSignal( VAR(Com_SignalIdType, memclass) SignalId, P2VAR(void, memclass, ptrclass) SignalDataPtr)
{
    /*Return uint8 result*/
    VAR(uint8, memclass) result = E_OK;
    /* validate signalID */
    if((!validateSignalID(SignalId))&&(ComIPdus[ComSignals[SignalId].ComIPduHandleId].ComIPduDirection != RECEIVE))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_RECEIVE_SIGNAL_ID,COM_E_PARAM);
#endif
        result = E_NOT_OK;
    }
    else
    {
        if(SignalDataPtr)
        {
            /*Extract the signal data from the Pdu buffer*/
            Com_ReadSignalDataFromSignalBuffer(SignalId, SignalDataPtr);
        }
        else
        {
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_RECEIVE_SIGNAL_ID,COM_E_PARAM_POINTER);
#endif
            result = E_NOT_OK;
        }
    }
    return result;
}

FUNC(BufReq_ReturnType, memclass)
Com_CopyTxData(VAR(PduIdType, memclass) PduId, CONSTP2VAR(PduInfoType,memclass,ptrclass) info, CONSTP2VAR(RetryInfoType, memclass, ptrclass) retry, P2VAR(PduLengthType, memclass, ptrclass) availableDataPtr )
{
    /*Return BufReq_ReturnType result */
    VAR(BufReq_ReturnType, memclass)  result = BUFREQ_OK;
    /*Pointer to variable for the source (Pdu buffer)*/
    AUTOMATIC P2VAR(uint8, memclass, ptrclass) source;
    /*Validate the Pdu is available*/
    if((!validateSignalID(PduId))&&(ComIPdus[PduId].ComIPduDirection != SEND)&&(ComIPdus[PduId].ComIPduType != TP))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_COPY_TX_DATA_ID,COM_E_PARAM);
#endif
        result = BUFREQ_E_NOT_OK;
    }
    else
    {
        if(info&&availableDataPtr)
        {
            if(((uint32)privateIPdus[PduId].CurrentPosition + info->SduLength) <= (uint32)(ComIPdus[PduId].ComIPduSize))
            {
                /*Point to the Pdu data buffer*/
                source = (uint8*)ComIPdus[PduId].ComIPduDataPtr;
                /*Move to required next data*/
                source += privateIPdus[PduId].CurrentPosition;
                /*Lock the buffer*/
                privateIPdus[PduId].locked=(boolean)TRUE;
                /*Copy the data from the Pdu data buffer to the lower layer data buffer*/
                memcpy((void*) info->SduDataPtr, (void*)source, info->SduLength);
                /*Move forward to the next data position*/
                privateIPdus[PduId].CurrentPosition += info->SduLength;
                /*Calculate the new data buffer available*/
                *availableDataPtr = (PduLengthType)ComIPdus[PduId].ComIPduSize - (PduLengthType)privateIPdus[PduId].CurrentPosition;
                result = BUFREQ_OK;
            }
            else
            {
                /*TODO: If not enough transmit data is available, no data is copied
              by the upper layer module and BUFREQ_E_BUSY is returned.*/
                result = BUFREQ_E_NOT_OK;
            }
        }
        else
        {
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_COPY_TX_DATA_ID,COM_E_PARAM_POINTER);
#endif
            result = BUFREQ_E_NOT_OK;
        }
    }
    return result;
}

FUNC(BufReq_ReturnType, memclass)
Com_CopyRxData(VAR(PduIdType, memclass)  PduID, CONSTP2VAR(PduInfoType, memclass, ptrclass) info, P2VAR(PduLengthType, memclass, ptrclass) bufferSizePtr )
{
    /*Return BufReq_ReturnType result*/
    VAR(BufReq_ReturnType, memclass) result = BUFREQ_OK;
    /*Pointer to variable for the destination (Pdu buffer)*/
    AUTOMATIC P2VAR(uint8, memclass, ptrclass) destination;
    /*Validate the Pdu is available*/
    if(!validateSignalID(PduID)&&(ComIPdus[PduID].ComIPduDirection != RECEIVE)&&(ComIPdus[PduID].ComIPduType !=TP))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_COPY_RX_DATA_ID,COM_E_PARAM);
#endif
        result = E_NOT_OK;
    }
    else
    {
        if(info&&bufferSizePtr)
        {
            if((uint32)ComIPdus[PduID].ComIPduSize - (uint32)privateIPdus[PduID].CurrentPosition >= info->SduLength )
            {
                /*Point to the Pdu data buffer*/
                destination = (uint8 *) ComIPdus[PduID].ComIPduDataPtr;
                /*Move to required next data*/
                destination += privateIPdus[PduID].CurrentPosition;
                if((info->SduDataPtr != NULL_PTR) && (info->SduLength !=(uint32)0x00))
                {
                    /*Copy the data from the lower layer data buffer  buffer to the Pdu data*/
                    memcpy(destination, info->SduDataPtr, info->SduLength);
                    /*Move forward to the next data position*/
                    privateIPdus[PduID].CurrentPosition += info->SduLength;
                    /*Calculate the new data buffer available*/
                    *bufferSizePtr = (uint32)ComIPdus[PduID].ComIPduSize - privateIPdus[PduID].CurrentPosition;
                }
                result = BUFREQ_OK;
            }
            else
            {
                result = BUFREQ_E_NOT_OK;
            }
        }
        else
        {
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_COPY_RX_DATA_ID,COM_E_PARAM_POINTER);
#endif
            result = E_NOT_OK;
        }
    }
    return result;
}

FUNC(Std_ReturnType, memclass)
Com_TriggerIPDUSend(VAR(PduIdType, memclass) PduId)
{
    AUTOMATIC CONSTP2VAR(ComIPdu_type, memclass, ptrclass) IPdu;
    VAR(PduInfoType, memclass)  PduInfoPackage;
    VAR(uint8, memclass)  signalIndex;
    VAR(uint8, memclass)  signalGroupIndex;
    /*Return Std_ReturnType result*/
    VAR(Std_ReturnType, memclass)  result;
    AUTOMATIC P2VAR(privateIPdu_type, memclass, ptrclass) privateIPdu;

    result=E_OK;

    if((!validateSignalID(PduId))&&(ComIPdus[PduId].ComIPduDirection!= SEND))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_TRIGGER_IPDU_SEND_ID,COM_E_PARAM);
#endif
        result = E_NOT_OK;
    }
    else
    {
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
                if(IPdu->ComIPduType==NORMAL)
                {
                    privateIPdu->locked=(boolean)FALSE;
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
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_TRIGGER_IPDU_SEND_ID,COM_E_SKIPPED_TRANSMISSION);
#endif
            result=E_NOT_OK;
        }
#endif
    }
    return result;
}

FUNC(void, memclass)
Com_RxIndication(VAR(PduIdType, memclass) ComRxPduId,CONSTP2VAR(PduInfoType, memclass, ptrclass) PduInfoPtr)
{
    VAR(uint8, memclass) result=E_OK;
    if((!validateSignalID(ComRxPduId))&&(ComIPdus[ComRxPduId].ComIPduDirection != RECEIVE )&&(ComIPdus[ComRxPduId].ComIPduType != NORMAL))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_RX_INDICATION_ID,COM_E_PARAM);
#endif
        result = E_NOT_OK;
    }
    else
    {
        if(PduInfoPtr)
        {
            /*TODO: add in critical section*/
            memcpy(ComIPdus[ComRxPduId].ComIPduDataPtr, PduInfoPtr->SduDataPtr, (uint32)ComIPdus[ComRxPduId].ComIPduSize);
            if(ComIPdus[ComRxPduId].ComIPduSignalProcessing == IMMEDIATE)
            {
                /*TODO: Remove lock buffer from signal group*/
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
        else
        {
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_RX_INDICATION_ID,COM_E_PARAM_POINTER);
#endif
            result = E_NOT_OK;
        }
    }
}

FUNC(BufReq_ReturnType, memclass)
Com_StartOfReception(VAR(PduIdType, memclass) PduId,CONSTP2VAR(PduInfoType, memclass, ptrclass) info,VAR(PduLengthType, memclass) TpSduLength,P2VAR(PduLengthType, memclass, ptrclass) bufferSizePtr)
{
    VAR(BufReq_ReturnType, memclass) result=BUFREQ_OK;
    if((!validateSignalID(PduId))&&(ComIPdus[PduId].ComIPduDirection!=RECEIVE) && (ComIPdus[PduId].ComIPduType != TP))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_START_OF_RECEPTION_ID,COM_E_PARAM);
#endif
        result = BUFREQ_E_NOT_OK;
    }
    else
    {
        if(info&&bufferSizePtr)
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
#if COM_DEV_ERROR_DETECT
            Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_START_OF_RECEPTION_ID,COM_E_PARAM_POINTER);
#endif
            result = BUFREQ_E_NOT_OK;
        }
    }
    return result;
}

FUNC(void, memclass)
Com_TpRxIndication(VAR(PduIdType, memclass) ComRxPduId,VAR(Std_ReturnType, memclass) Result)
{
    if((!validateSignalID(ComRxPduId))&&(ComIPdus[ComRxPduId].ComIPduDirection != RECEIVE) && (ComIPdus[ComRxPduId].ComIPduType != TP))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_TP_RX_INDICATION_ID,COM_E_PARAM);
#endif
        result = E_NOT_OK;
    }
    else
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
}

LOCAL FUNC(void, memclass)
privateGeneralTxConfirmation(VAR(PduIdType, memclass) TxPduId)
{
    VAR(uint8, memclass)  signalIndex;
    VAR(uint8, memclass)  signalGroupIndex;
    VAR(BufReq_ReturnType, memclass) result=E_OK;
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

FUNC(void, memclass)
Com_TpTxConfirmation(VAR(PduIdType, memclass) TxPduId, VAR(Std_ReturnType, memclass) result)
{
    if((!validateSignalID(TxPduId))&&(ComIPdus[TxPduId].ComIPduDirection!=SEND)&&(ComIPdus[TxPduId].ComIPduType != TP))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_TP_TX_CONFIRMATION_ID,COM_E_PARAM);
#endif
    }
    else
    {
        if(result==E_OK)
        {
            if()
            {
                privateIPdus[TxPduId].locked=(uint8)FALSE;
                privateGeneralTxConfirmation(TxPduId);
            }
        }
    }
}

FUNC(void, memclass)
Com_TxConfirmation(VAR(PduIdType, memclass) TxPduId, VAR(Std_ReturnType, memclass) result)
{
    if((!validateSignalID(TxPduId))&&(ComIPdus[TxPduId].ComIPduDirection!=SEND)&&(ComIPdus[TxPduId].ComIPduType != NORMAL))
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_TX_CONFIRMATION_ID,COM_E_PARAM);
#endif
    }
    else
    {
        if(result==E_OK)
        {
            privateGeneralTxConfirmation(TxPduId);
        }
        validateResult=E_OK;
    }
}

FUNC(uint8, memclass)
Com_SendSignalGroup(VAR(Com_SignalGroupIdType, memclass) SignalGroupId )
{
    VAR(uint8, memclass)  result;
    VAR(uint8, memclass)  signalIndex;

    result=E_OK;
    if((SignalGroupId<(Com_SignalGroupIdType)COM_NUM_OF_GROUP_SIGNAL)&&(ComIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].ComIPduDirection==SEND))
    {
        if(privateIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].locked)
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
    }
    else
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_SEND_SIGNAL_GROUP_ID,COM_E_PARAM);
#endif
        result=E_NOT_OK;
    }
    return result;
}

FUNC(uint8, memclass)
Com_ReceiveSignalGroup(VAR(Com_SignalGroupIdType, memclass) SignalGroupId )
{
    VAR(uint8, memclass) result;
    VAR(uint8, memclass) signalIndex;

    result=E_OK;
    if((SignalGroupId<(Com_SignalGroupIdType)COM_NUM_OF_GROUP_SIGNAL)&&(ComIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].ComIPduDirection==RECEIVE))
    {
        /*TODO:remove buffer lock check*/
        if(privateIPdus[ComSignalGroups[SignalGroupId].ComIPduHandleId].locked)
        {
            for(signalIndex=(uint8)0;signalIndex< ComSignalGroups[SignalGroupId].ComIPduNumberOfGroupSignals;signalIndex++)
            {
                /*TODO:add critical section to memory copy*/
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
    }
    else
    {
#if COM_DEV_ERROR_DETECT
        Det_ReportError(COM_MODULE_ID,COM_INSTANCE_ID,COM_RECEIVE_SIGNAL_GROUP_ID,COM_E_PARAM);
#endif
        result=E_NOT_OK;
    }
    return result;
}
