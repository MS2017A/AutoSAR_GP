/***************************************************
 * File Name: Com_Buffer.h
 * Author: AUTOSAR COM Team
 * Date Created: Jun 2020
 * Version  : 4.0
 ****************************************************/

#ifndef INCLUDE_COM_BUFFER_H_
#define INCLUDE_COM_BUFFER_H_

void Com_PackSignalsToPdu(PduIdType ComIPuId);

void Com_PduUnpacking(PduIdType ComRxPduId);

void Com_ReadSignalDataFromPduBuffer(PduIdType ComRxPduId,const ComSignal_type* const SignalRef);

void Com_WriteSignalDataToSignalBuffer (const uint16 signalId, const void * signalData);

void Com_ReadSignalDataFromSignalBuffer (const uint16 signalId,  void * signalData);

#endif /* INCLUDE_COM_BUFFER_H_ */
