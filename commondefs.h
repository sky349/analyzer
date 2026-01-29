#ifndef COMMONDEFS_H
#define COMMONDEFS_H

#include <QtGui>

enum TrackDataField
{
    Field_Source =1,
    Field_PoD,

    Field_PositionXY,
    Field_PositionAD,
    Field_PositionLL,

    Field_Speed,
    Field_VerticalSpeed,
    Field_Heading,

    Field_BoardNumber,
    Field_Height,
    Field_Alert,
    Field_SPI,

    Field_AircraftId,
    Field_AircraftAddress,
    Field_Name,
    Field_OnGround,

    Field_QualityPosition,
    Field_QualityVelocity,
    Field_QualitySIL,
    Field_MOPSVersion,
    Field_EmitterCategory,

    Field_ModeSServicesCap, //BDS1,0
    Field_AircraftIdCap,

    Field_MCPSelectedAltitude, //BDS4,0
    Field_FMSSelectedAltitude,
    Field_TargetAltitudeSource,
    Field_BarometricPressureSetting,
    Field_ApproachMode,
    Field_AltHoldMode,
    Field_VNAVMode,

    Field_SelectedAltitude,
    Field_MCPMode,
    Field_ModeSCaps,

    Field_RollAngle, //BDS5,0
    Field_TrueTrackAngle,
    Field_GroundSpeed,
    Field_TrackAngleRate,
    Field_TrueAirspeed,

    Field_MagneticHeading, //BDS6,0
    Field_IndicatedAirspeed,
    Field_Mach,
    Field_BarometricAltitudeRate,
    Field_IntertialVerticalVelocity
};

QString trackFieldName(TrackDataField field);

#endif // COMMONDEFS_H
