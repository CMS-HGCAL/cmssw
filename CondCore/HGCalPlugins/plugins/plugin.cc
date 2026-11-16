#include "CondCore/ESSources/interface/registration_macros.h"
#include "CondFormats/DataRecord/interface/HGCalADCPedestalsRcd.h"
#include "CondFormats/DataRecord/interface/HGCalNoiseRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCommonModePedestalsRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCommonModeSlopeRcd.h"
#include "CondFormats/DataRecord/interface/HGCalCalibrationValidityRcd.h"
#include "CondFormats/HGCalObjects/interface/HGCalADCPedestalsPayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalNoisePayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalCommonModePedestalsPayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalCommonModeSlopePayload.h"
#include "CondFormats/HGCalObjects/interface/HGCalCalibrationValidityPayload.h"

REGISTER_PLUGIN(HGCalADCPedestalsRcd, HGCalADCPedestalsPayload);
REGISTER_PLUGIN(HGCalNoiseRcd, HGCalNoisePayload);
REGISTER_PLUGIN(HGCalCommonModePedestalsRcd, HGCalCommonModePedestalsPayload);
REGISTER_PLUGIN(HGCalCommonModeSlopeRcd, HGCalCommonModeSlopePayload);
REGISTER_PLUGIN(HGCalCalibrationValidityRcd, HGCalCalibrationValidityPayload);
