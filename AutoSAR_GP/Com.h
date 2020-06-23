
/*******************************************************
 *
 * File Name: Com.h
 *
 * Author: AUTOSAR COM Team 
 * 
 * Date Created: 6 March 2019
 * 
 * Version  : 01
 * 
 ********************************************************/

#ifndef COM_H_
#define COM_H_

#include "Com_Cfg.h"
#include "ComStack_Types.h"

//#include "Com_Types.h"  /* to be changed to ComStack_Types.h */

/************************************************************************
 *                          Preprocessor                                *
 ************************************************************************/

#define COM_BUSY 0x81

/************************************************************************
 *                            COM_Config                                *
 ************************************************************************/

/************************ComTransferProperty_type*************************/
#define PENDING                                     ((uint8)0)
#define TRIGGERED                                   ((uint8)1)
#define TRIGGERED_ON_CHANGE                         ((uint8)2)
#define TRIGGERED_ON_CHANGE_WITHOUT_REPETITION      ((uint8)3)
#define TRIGGERED_WITHOUT_REPETITION                ((uint8)4)

/***************************ComTxModeMode_type***************************/
#define DIRECT                                      ((uint8)0)
#define MIXED                                       ((uint8)1)
#define NONE                                        ((uint8)2)
#define PERIODIC                                    ((uint8)3)

/*************************ComIPduDirection_type**************************/
#define RECEIVE                                     ((uint8)0)
#define SEND                                        ((uint8)1)

/**********************ComIPduSignalProcessing_type**********************/
#define DEFERRED                                    ((uint8)0)
#define IMMEDIATE                                   ((uint8)1)

/******************************ComIPduType_type**************************/
#define NORMAL                                      ((uint8)0)
#define TP                                          ((uint8)1)

/*********************ComTxIPduClearUpdateBit_type***********************/
#define CONFIRMATION                                ((uint8)0)
#define TRANSMIT                                    ((uint8)1)
#define TRIGGER_TRANSMIT                            ((uint8)2)

/****************************ComSignalType_type**************************/
#define BOOLEAN                                     ((uint8)0 )
#define FLOAT32                                     ((uint8)1 )
#define FLOAT64                                     ((uint8)2 )
#define UINT8                                       ((uint8)3 )
#define UINT16                                      ((uint8)4 )
#define UINT32                                      ((uint8)5 )
#define UINT8_N                                     ((uint8)6 )
#define UINT8_DYN                                   ((uint8)7 )
#define SINT8                                       ((uint8)8 )
#define SINT16                                      ((uint8)9 )
#define SINT32                                      ((uint8)10)
#define SINT64                                      ((uint8)11)
#define UINT64                                      ((uint8)12)

/*******TODO: make com_type.h*****************************************************************
 *                       User-Defined Types                             *
 ************************************************************************/

typedef struct
{

}ComConfig_type;

/* Signal object identifier */
typedef uint16 Com_SignalIdType;

typedef uint16 Com_SignalGroupIdType;

typedef uint16 Com_IpduGroupIdType;

/****************************ComSignal_type*******************************/
typedef struct
{
    /*  This parameter refers to the position in the I-PDU and not in the shadow buffer.*/
    uint32              ComBitPosition;

    /*  Bit position of update-bit inside I-PDU.
        If this attribute is omitted then there is no update-bit. This setting must be consistently on sender and on receiver side.
        Range: 0..63 for CAN and LIN, 0..511 for CAN FD, 0..2031 for FlexRay, 0..4294967295 for TP.*/
    uint32              ComUpdateBitPosition;

    /*The supported maximum length is restricted by the used transportation system. For non TP-PDUs the maximum size of a PDU,
     * and therefore also of any included signal, is limited by the concrete bus characteristic. For example, the limit is 8 bytes
     * for CAN and LIN, 64 bytes for CAN FD and 254 for FlexRay.*/
    uint32              ComSignalLength;

    /* Pointer to the signal Data Buffer */
    void*               ComSignalDataPtr;

    /* notification function. */
    void (*ComNotification) (void);

    /* IPDU id of the IPDU that this signal belongs to.
     * This is initialized by Com_Init() and should not be configured.*/
    PduIdType           ComIPduHandleId;

    /* To check whether update bit is configured or not.*/
    boolean             ComUpdateBitEnabled;

    /* Size in bits, for integer signal types. For ComSignalType UINT8_N and UINT8_DYN
       the size shall be configured by ComSignalLength. For ComSignalTypes FLOAT32 and FLOAT64 the size is already defined by the signal type 
       and therefore may be omitted.*/
    uint8               ComBitSize;

    /*  The AUTOSAR type of the signal. Whether or not the signal is signed or unsigned can be found by examining the value of this attribute.
        This type could also be used to reserved appropriate storage in AUTOSAR COM.*/
    uint8               ComSignalType;

    /*  Defines if a write access to this signal can trigger the transmission of the correspon-ding I-PDU.
     *  If the I-PDU is triggered, depends also on the transmission mode of the corresponding I-PDU.*/
    uint8               ComTransferProperty;
}ComSignal_type;

/**************************ComSignalGroup_type****************************/
typedef struct
{
    /* notification function. */
    void (*ComNotification) (void);

    /* Pointer to the first group signal in the signal group.  */
    const ComSignal_type*       ComIPduSignalRef;

    /*  Bit position of update-bit inside I-PDU.
        If this attribute is omitted then there is no update-bit. This setting must be consistently on sender and on receiver side.
        Range: 0..63 for CAN and LIN, 0..511 for CAN FD, 0..2031 for FlexRay, 0..4294967295 for TP.*/
    uint32              ComUpdateBitPosition;

    /* The numerical value used as the ID of this I-PDU */
    PduIdType           ComIPduHandleId ;

    /* To check whether update bit is configured or not.*/
    boolean             ComUpdateBitEnabled;

    /*  Defines if a write access to this signal can trigger the transmission of the correspon-ding I-PDU.
     *  If the I-PDU is triggered, depends also on the transmission mode of the corresponding I-PDU.*/
    uint8               ComTransferProperty;

    /* Number of group signals in signal group. */
    uint8               ComIPduNumberOfGroupSignals;

}ComSignalGroup_type;

/****************************ComIPdu_type*******************************/
typedef struct
{
    /* Reference to the actual pdu data Buffer */
    void *              ComIPduDataPtr;

    /* Pointer to the first signal in the IPdu */
    const ComSignal_type*       ComIPduSignalRef;

    /* Pointer to the first signal group. TODO */
    const ComSignalGroup_type* ComIPduSignalGroupRef;

    /* size of the Pdu in bytes */
    uint32              ComIPduSize;

    /* The numerical value used as the ID of this I-PDU */
    PduIdType           ComIPduHandleId ;

    /* Index to the Ipdu of type Send */
    uint16              ComTxIPdu;

    /* sent or received */
    uint8               ComIPduDirection;

    /* Immidiate or deferred */
    uint8               ComIPduSignalProcessing;

    /* Normal or Tp */
    uint8               ComIPduType;

    /* Number of Signal */
    uint8               ComIPduNumOfSignals;

    /* Number of Signal group. TODO*/
    uint8               ComIPduNumberOfSignalGroups;
}ComIPdu_type;

/****************************ComTxIPdu_type*******************************/
typedef struct
{
#if COM_ENABLE_MDT_FOR_CYCLIC_TRANSMISSION
    /* Minimum delay between successive transmissions of the IPdu */
    float32             ComMinimumDelayTime;
#endif

    /* repetition period in seconds of the periodic transmission requests
       in case ComTxModeMode is configured to PERIODIC or MIXED.*/
    uint16              ComTxModeTimePeriod;

    /* Confirmation, Transmit or Trigger Transmit */
    uint8               ComTxIPduClearUpdateBit;

    /* COM will fill unused areas within an IPdu with this bit patter */
    uint8               ComTxIPduUnusedAreasDefault;

    /* DIRECT, MIXED, NONE or PERIODIC */
    uint8               ComTxModeMode;

#if 0
    /* number of repetitions for the transmission mode DIRECT and
        the event driven part of transmission mode MIXED.*/
    uint8               ComTxModeNumberOfRepetitions;

    /* repetition period in seconds of the multiple transmissions in
       case ComTxModeNumberOfRepetitions is configured greater than or
       equal to 1 and ComTxModeMode is configured to DIRECT or MIXED.*/
    uint16              ComTxModeRepetitionPeriod;

    /* period in seconds between the start of the I-PDU by
       Com_IpduGroupStart and the first transmission request in case
       ComTxModeMode is configured to PERIODIC or MIXED.*/
    uint16              ComTxModeTimeOffset;
#endif
}ComTxIPdu_type;

/************************************************************************
 *                      Functions Prototypes                            *
 ************************************************************************/

/*initializes internal and external interfaces and variables of the COM module */
void Com_Init( const ComConfig_type* config);

/* Processing of the AUTOSAR COM module's receive activities (PDU To Signal) */
void Com_MainFunctionRx(void);

/* Processing of the AUTOSAR COM module's transmission activities (Signal To PDU)*/
void Com_MainFunctionTx(void);

/* Updates the signal object identified by SignalId with the signal referenced by the SignalDataPtr parameter */
uint8 Com_SendSignal( Com_SignalIdType SignalId, const void* SignalDataPtr );

/*The service Com_ReceiveSignalGroup copies the received signal group from the I-PDU to the shadow buffer.*/
uint8 Com_ReceiveSignalGroup( Com_SignalGroupIdType SignalGroupId );

/*The service Com_SendSignalGroup copies the content of the associated shadow buffer to the associated I-PDU.*/
uint8 Com_SendSignalGroup( Com_SignalGroupIdType SignalGroupId );

/* Copies the data of the signal identified by SignalId to the location specified by SignalDataPtr */
uint8 Com_ReceiveSignal( Com_SignalIdType SignalId, void* SignalDataPtr );

/* the I-PDU with the given ID is triggered for transmission */
Std_ReturnType Com_TriggerIPDUSend( PduIdType PduId );


#endif

