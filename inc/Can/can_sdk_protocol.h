/////////////////////////////////////////////////////////////////////
/* Copyright (c) 2021-2030 ZCL Electronics Co., LTD.
* description SECC conversion board.
* SECC conversion board communication interface file, 
* provided for business developers to use the standard file. 
* The file contains the complete communication protocol interaction process of SECC conversion board, 
* and developers do not need to pay attention to the protocol process itself. 
* This interface file provides business developers to develop the sdk mode, 
* and business developers only need to fill in the interface data and complete 
* the data logic of the business layer according to the requirements. 
* This sdk mode can reduce the development difficulty of developers, 
* and can quickly access and use this SECC conversion board communication protocol.
*/
/////////////////////////////////////////////////////////////////////

#ifndef ZCL_SDK_PROTOCOL_
#define ZCL_SDK_PROTOCOL_

#include <stdio.h>
#include <string.h>


/*The CmdCode command code enumeration list is as follows.*/
enum SECC_SDK_CmdCode{
	CmdCode_CCB_SessionSetup 			= 0x50u,
	CmdCode_CCB_ChargeParameterDiscovery= 0x51u,
	CmdCode_CCB_CableCheck				= 0x52u,
	CmdCode_CCB_CurrentDemand 			= 0x53u,
	CmdCode_CCB_SessionStop				= 0x55u,
	CmdCode_CCU_SessionSetup 			= 0x80u,
	CmdCode_CCU_ChargeParameterDiscovery= 0x81u,
	CmdCode_CCU_CableCheckComplete 		= 0x82u,
	CmdCode_CCU_CurrentDemand			= 0x83u,
	CmdCode_CCU_SessionStop				= 0x85u,
};

/*Stop Code mapping table enum*/
enum SECC_SDK_ChargeStopCause{

	/*Conventional stop*/
	StopCause_CCUStop_Normal 							= 100,
	StopCause_CCUStop_Emergency							= 101,
	StopCause_Vehicle_RemainTimeis_0_Normal				= 102,
	StopCause_Vehicle_CharingDisabled_Normal			= 103,
	StopCause_Vehicle_ReqChangeto_0_Normal				= 104,
	StopCause_Vehicle_BeforeCharge_Normal				= 105,
	StopCause_Vehicle_Charing_ChargePermission_Disable	= 106,
	StopCause_CCS2_Trip_EVComplete_StopReq				= 107,
	StopCause_CCS2_Trip_SOCFull_StopReq					= 108,
	StopCause_CHADEMO_LockErr				 			= 200,
	
	/*CHAEDEMO only*/
	StopCause_CHADEMO_Start_NoPlugIn_TimeOut 			= 1000,
	StopCause_CHADEMO_WaitComm_TimeOut					= 1001,
	StopCause_CHADEMO_VehicleShiftPosition_Error		= 1002,
	StopCause_CHADEMO_Comm_VehicleParaUpdated_TimeOut	= 1003,
	StopCause_CHADEMO_Comm_ReqVolOverMaxVol				= 1004,
	StopCause_CHADEMO_Comm_ReqVolLessAvailableVol		= 1005,
	StopCause_CHADEMO_EV_AllowCharge_TimeOut			= 1006,
	StopCause_CHADEMO_WaitEvseSelfCheckOK_TimeOut		= 1007,
	StopCause_CHADEMO_CP3_ChargePermission_BeforeError	= 1008,
	StopCause_CHADEMO_102_5_ChargingBit_BeforeError		= 1009,
	StopCause_CHADEMO_PreChargeWaitEvContactors_TimeOut	= 1010,
	StopCause_CHADEMO_PreChargeWaitEvReqCurrent_TimeOut	= 1011,
	StopCause_CHADEMO_VEHICLE_OVERVOLT_Error			= 1012,
	StopCause_CHADEMO_VEHICLE_UNDERVOLT_Error			= 1013,
	StopCause_CHADEMO_VEHICLE_CURRDIFF_Error			= 1014,
	StopCause_CHADEMO_VEHICLE_TEMPERATURE_Error			= 1015,
	StopCause_CHADEMO_VEHICLE_VOLTDIFF_Error			= 1016,
	StopCause_CHADEMO_VehicleGENERAL_ERROR_Error		= 1017,
	StopCause_CHADEMO_Charing_ChargePermission_Disable	= 1018,
	StopCause_CHADEMO_EvseTimeOut_Error					= 1019,
	StopCause_CHADEMO_EvTimeOut_100_101_102_Error 		= 2000,


	/*CCS2 only*/
	StopCause_CCS2_FastProtect_CPliotDetected			= 6001,
	StopCause_CCS2_Slac_Error							= 6002,
	StopCause_CCS2_Session_Error						= 6003,
	StopCause_CCS2_EvseTimeOut_Error					= 6004,
	StopCause_CCS2_SlacDetected_Check_TimeOut			= 6006,
	StopCause_CCS2_Start_NoPlugIn_TimeOut				= 6007,
	StopCause_CCS2_Wait_NetWorkInitOK_TimeOut			= 6008,
	StopCause_CCS2_Wait_SDP_TimeOut						= 6009,
	StopCause_CCS2_Wait_SAP_TimeOut						= 6010,
	StopCause_CCS2_SLAC_Static_SET_KEY_TimeOut			= 7002,
	StopCause_CCS2_SLAC_Wait_CM_SLAC_PARM_REQ_TimeOut	= 7003,
	StopCause_CCS2_SLAC_Wait_ATTEN_CHAR_IND_TimeOut		= 7004,
	StopCause_CCS2_SLAC_Wait_ATTEN_CHAR_RSP_TimeOut		= 7005,
	StopCause_CCS2_SLAC_Wait_MATCH_OR_VALIDATE_TimeOut	= 7006,
	StopCause_CCS2_SLAC_Wait_LinkDetected_TimeOut		= 7007,
	StopCause_CCS2_SLAC_SlacTimeout						= 7008,
	StopCause_CCS2_SLAC_RSP_RETRY_LIMIT					= 7009,
	StopCause_CCS2_SLAC_Match_ErrLIMIT					= 7010,
	StopCause_CCS2_SLAC_CM_START_ATTEN_CHAR_TIMEOUT		= 7011,	
	StopCause_CCS2_ServiceCategoryFlag_Error			= 8001,
	StopCause_CCS2_SelectedPaymentOption_Error			= 8002,
	StopCause_CCS2_GenChallenge_Error					= 8003,
	StopCause_CCS2_AC_EVChargeParameter_Error			= 8004,
	StopCause_CCS2_PhysicalValue_Multiplier_Error		= 8005,
	StopCause_CCS2_PhysicalValue_Unit_Error				= 8006,
	StopCause_CCS2_RequestedEnergyTransferType_Error	= 8007,
	StopCause_CCS2_EVMaxVolLessThanSeccMinVol_Error		= 8008,
	StopCause_CCS2_EVMaxCurLessThanSeccMinCur_Error		= 8009,
	StopCause_CCS2_EVReqVolLessThanSeccMinVol_Error		= 8010,
	StopCause_CCS2_EVReqVolMoreThanSeccMaxVol_Error		= 8011,
	StopCause_CCS2_EVReqVolMoreThanEVMaxVol_Error		= 8012,
	StopCause_CCS2_EVReqCurMoreThanEVMaxCur_Error		= 8013,
	StopCause_CCS2_SessionUNKNOW_Error					= 8014,
	StopCause_CCS2_CP_Error								= 8015,
	StopCause_CCS2_NO_DC_EVPowerDeliveryParameter_Error	= 8016,
	StopCause_CCS2_CHARGING_PROFILE_INVALID_Error		= 8017,
	StopCause_CCS2_EVCloseSocket_Error					= 8018,
	StopCause_CCS2_Sequence_Timeout_SetupTimer_Error	= 8019,
	StopCause_CCS2_Sequence_Timeout_AppSession_Error	= 8020,
	StopCause_CCS2_CPState_Detection_Timeout_Error		= 8021,
	StopCause_CCS2_EV_BatteryTemperature_Error			= 8022,
	StopCause_CCS2_EV_ShiftPosition_Error				= 8023,
	StopCause_CCS2_EV_ConnectorLock_Error				= 8024,
	StopCause_CCS2_EV_BatteryMalfunction_Error			= 8025,
	StopCause_CCS2_EV_CurrentDifferential_Error			= 8026,
	StopCause_CCS2_EV_VoltageOutOfRange_Error			= 8027,
	StopCause_CCS2_EV_SystemIncompatable_Error			= 8028,
	StopCause_CCS2_EV_UNKNOWN_Error						= 8029,
	StopCause_CCS2_SERVICE_IDINVALID_Error				= 8030,

	/*ccu communication failure*/
	StopCause_ComTimeOut_startcharge					= 9000,
	StopCause_ComTimeOut_parametersync					= 9001,
	StopCause_ComTimeOut_selftestcomplete				= 9002,
	StopCause_ComTimeOut_chargingparameter				= 9003,
	StopCause_ComTimeOut_stopcharging					= 9004,

};

#endif

