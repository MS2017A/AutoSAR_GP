#include "PduR.h"
#include "Com_Cbk.h"
uint8 PduR_Buffer[10];
PduInfoType PduRInfoTypeData;
Std_ReturnType PduR_ComTransmit( PduIdType TxPduId, const PduInfoType* PduInfoPtr )
{
    Std_ReturnType Std_Return=E_OK;
    uint8 counter=0;
    if(TxPduId == 0)
    {
        PduRInfoTypeData.SduDataPtr=PduR_Buffer;
        PduRInfoTypeData.SduLength=PduInfoPtr->SduLength;
        //PduRInfoTypeData.MetaDataPtr=PduInfoPtr->MetaDataPtr;

        for(counter=0;counter<(PduInfoPtr->SduLength);counter++)
        {
            PduR_Buffer[counter] = PduInfoPtr->SduDataPtr[counter];
        }
        Com_RxIndication(1,&PduRInfoTypeData);
        Com_TxConfirmation(TxPduId,E_OK);
    }
    else
    {
        Std_Return = E_NOT_OK;
    }
    return Std_Return;
}
