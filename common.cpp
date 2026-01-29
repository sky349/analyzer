
#include "commondefs.h"

QString trackFieldName(TrackDataField field)
{
    switch(field)
    {
    case Field_Source: return QObject::tr("Target source type"); //unused (?)
    case Field_PoD: return QObject::tr("Probability of detection");
    case Field_Name: return ""; //unnamed, используется только на миникарте

    case Field_PositionXY: return QObject::tr("Position (XY)");
    case Field_PositionAD: return QObject::tr("Position (POL)");
    case Field_PositionLL: return QObject::tr("Position (GEO)");

    case Field_BoardNumber: return QObject::tr("Mode-A code (squawk)");
    case Field_Height: return QObject::tr("Altitude");
    case Field_Alert: return QObject::tr("Alert");
    case Field_SPI: return QObject::tr("SPI");

    case Field_Speed: return QObject::tr("Ground speed");
    case Field_VerticalSpeed: return QObject::tr("Vertical rate");
    case Field_Heading: return QObject::tr("Heading");

    case Field_AircraftId: return QObject::tr("Aircraft identification");
    case Field_AircraftAddress: return QObject::tr("Aircraft address");
    case Field_OnGround: return QObject::tr("Aircraft on ground");

    case Field_MOPSVersion: return QObject::tr("MOPS version");
    case Field_QualityPosition: return QObject::tr("Quality (NIC/NUCp, NACp)");
    case Field_QualityVelocity: return QObject::tr("Quality (NACv/NUCr)");
    case Field_QualitySIL: return QObject::tr("Quality (SIL)");
    case Field_EmitterCategory: return QObject::tr("Emitter category");

    case Field_ModeSServicesCap: return QObject::tr("Caps: Mode S specific services");
    case Field_AircraftIdCap: return QObject::tr("Caps: Aircraft identification");

    case Field_SelectedAltitude: return QObject::tr("Selected altitude");
    case Field_MCPMode: return QObject::tr("MCP/FCU mode");
    case Field_ModeSCaps: return QObject::tr("Mode S capabilities");

    case Field_MCPSelectedAltitude: return QObject::tr("MCP/FCU selected altitude");
    case Field_FMSSelectedAltitude: return QObject::tr("FMS selected altitude");
    case Field_TargetAltitudeSource: return QObject::tr("Target altitude source");
    case Field_BarometricPressureSetting: return QObject::tr("Barometric pressure setting");
    case Field_ApproachMode: return QObject::tr("Approach mode");
    case Field_AltHoldMode: return QObject::tr("Altitude hold mode");
    case Field_VNAVMode: return QObject::tr("VNAV mode");

    case Field_RollAngle: return QObject::tr("Roll angle");
    case Field_TrueTrackAngle: return QObject::tr("True track angle");
    case Field_GroundSpeed: return QObject::tr("Ground speed");
    case Field_TrackAngleRate: return QObject::tr("Track angle rate");
    case Field_TrueAirspeed: return QObject::tr("True airspeed");

    case Field_MagneticHeading: return QObject::tr("Magnetic heading");
    case Field_IndicatedAirspeed: return QObject::tr("Indicated airspeed");
    case Field_Mach: return QObject::tr("Mach");
    case Field_BarometricAltitudeRate: return QObject::tr("Barometric altitude rate");
    case Field_IntertialVerticalVelocity: return QObject::tr("Intertial vertical velocity");
    }

    return QObject::tr("Unknown field");
}

