#include "device/prior/prior_proscan_prop_info.h"

namespace PriorProscan {

std::map<std::string, std::string> error_code = {
    {"E,1", "NO STAGE"},
    {"E,2", "NOT IDLE"},
    {"E,3", "NO DRIVE"},
    {"E,4", "STRING PARSE"},
    {"E,5", "COMMAND NOT FOUND"},
    {"E,6", "INVALID SHUTTER"},
    {"E,7", "NO FOCUS"},
    {"E,8", "VALUE OUT OF RANGE"},
    {"E,9", "INVALID WHEEL"},
    {"E,10", "ARG1 OUT OF RANGE"},
    {"E,11", "ARG2 OUT OF RANGE"},
    {"E,12", "ARG3 OUT OF RANGE"},
    {"E,13", "ARG4 OUT OF RANGE"},
    {"E,14", "ARG5 OUT OF RANGE"},
    {"E,15", "ARG6 OUT OF RANGE"},
    {"E,16", "INCORRECT STATE"},
    {"E,17", "WHEEL NOT FITTED"},
    {"E,18", "QUEUE FULL"},
    {"E,19", "COMPATIBILITY MODE SET"},
    {"E,20", "SHUTTER NOT FITTED"},
    {"E,21", "INVALID CHECKSUM"},
    {"E,60", "ENCODER ERROR"},
    {"E,61", "ENCODER RUN OFF"},
};

std::map<std::string, PropInfo> prop_info = {
    {"MotionStatus",
     {
         .name = "Motion Status",
         .description = "Reports status as a decimal number and gives motion "
                        "status of any axis of the controller. After binary "
                        "conversion convention is as follows:\n"
                        "  F2  F1  A   Z   Y   X\n"
                        "  D05 D04 D03 D02 D01 D00\n"
                        "But it is actually:\n"
                        "  F2  F1  F3  Z   Y   X",
         .getCommand = "$",
         .setCommand = "",
         .setResponse = "",
         .isVolatile = true,
     }},
    {"StopMotion",
     {
         .name = "Stop motion",
         .description = "Stops movement in a controlled manner to reduce the "
                        "risk of losing position",
         .getCommand = "",
         .setCommand = "I",
         .setResponse = "R",
         .isVolatile = false,
     }},
    {"XYResolution",
     {
         .name = "XY Resolution",
         .description = "Sets the desired resolution for stage (x, y)",
         .getCommand = "RES,s",
         .setCommand = "RES,s,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"RawXYPosition",
     {
         .name = "Raw XY Position",
         .description = "Position of stage (x, y) in the unit of XYResolution",
         .getCommand = "PS",
         .setCommand = "G,{}",
         .setResponse = "R",
         .isVolatile = true,
     }},
    {"XYPosition",
     {
         .name = "XY Position",
         .description = "Position of stage (x, y) in um. Computed from "
                        "RawXYPosition and XYResolution)",
         .getCommand = "",
         .setCommand = "",
         .setResponse = "",
         .isVolatile = true,
     }},
    //    {"RawXYRelativePosition", {
    //        .name        = "Raw Relative XY Position",
    //        .description = "",
    //        .getCommand  = "",
    //        .setCommand  = "GR,{}",
    //        .setResponse = "R",
    //        .isVolatile  = false,
    //    }},
    {"LumenShutter",
     {
         // Turn on/off shutter with the actual "Lumen Output Intensity"
         // command.
         .name = "Lumen Shutter",
         .description = "",
         .getCommand = "LIGHT",
         .setCommand = "LIGHT,{}",
         .setResponse = "R", // Manual says it is "0", but it is "R".
         .isVolatile = false,
     }},
    {"LumenOutputIntensity",
     {
         // We actually keep the number and only issue I/O when LumenShutter is
         // set.
         .name = "Lumen Output Intensity",
         .description = "",
         .getCommand = "",
         .setCommand = "",
         .setResponse = "",
         .isVolatile = false,
     }},
    {"FilterWheel1",
     {
         .name = "Filter Wheel 1 Position",
         .description = "",
         .getCommand = "7,1,F",
         .setCommand = "7,1,{}",
         .setResponse = "R",
         .isVolatile = true,
     }},
    //    {"FilterWheel2", {
    //        .name        = "Filter Wheel 2 Position",
    //        .description = "",
    //        .getCommand  = "7,2,F",
    //        .setCommand  = "7,2,{}",
    //        .setResponse = "R",
    //        .isVolatile  = false,
    //    }},
    {"FilterWheel3",
     {
         .name = "Filter Wheel 3 Position",
         .description = "",
         .getCommand = "7,3,F",
         .setCommand = "7,3,{}",
         .setResponse = "R",
         .isVolatile = true,
     }},
    {"Baudrate",
     {
         .name = "Baudrate",
         .description =
             "The baud rate of the port (write-only). As a protection measure, "
             "if no command is sent to the port while the controller is "
             "switched on, the baud rate will revert to 9600 after switching "
             "off and back on again twice. Allowable values for baud rate are "
             "9600 (argument 96), 19200 (argument 19) and 38400 (argument 38)",
         .getCommand = "",
         .setCommand = "BAUD,{}",
         .setResponse =
             "", // It actual has response, but it is unclear what it is.
         .isVolatile = false,
     }},
    {"CommandProtocol",
     {
         .name = "Command Protocol",
         .description =
             "Command protocol (Compatibility mode (1) or Standard mode (0)).",
         .getCommand = "COMP",
         .setCommand = "COMP,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    // "InstrumentVersion", {
    //     .name      = "Instrument Version",
    //     .getCommand = "DATE",
    //     .isVolatile = false,
    // }},
    {"SoftwareVersion",
     {
         .name = "Software Version",
         .description = "",
         .getCommand = "VERSION",
         .setCommand = "",
         .setResponse = "",
         .isVolatile = false,
     }},
    {"SerialNumber",
     {
         .name = "Serial Number",
         .description = "Reports the units’ serial number nnnnn, if the serial "
                        "number has not been set “00000” is returned.",
         .getCommand = "SERIAL",
         .setCommand = "",
         .setResponse = "",
         .isVolatile = false,
     }},
    {"Stage_MaxAcceleration",
     {
         .name = "Stage Maximum Acceleration",
         .description = "Maximum stage acceleration. Range is 1 to 100.",
         .getCommand = "SAS",
         .setCommand = "SAS,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"Stage_SCurveValue",
     {
         .name = "Stage S-Curve Value",
         .description =
             "Stage S-curve value is the rate of change of acceleration during "
             "the transition from stationary until the stage reaches the full "
             "acceleration. Range is 1 to 100.",
         .getCommand = "SCS",
         .setCommand = "SCS,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"Stage_MaxSpeed",
     {
         .name = "Stage Maximum Speed",
         .description = "Stage (x, y) maximum speed. Range is 1 to 100.",
         .getCommand = "SMS",
         .setCommand = "SMS,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel1_MaxAcceleration",
     {
         .name = "Filter Wheel 1 Maximum Acceleration",
         .description = "",
         .getCommand = "SAF,1",
         .setCommand = "SAF,1,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel1_SCurveValue",
     {
         .name = "Filter Wheel 1 S-Curve Value",
         .description = "",
         .getCommand = "SCF,1",
         .setCommand = "SCF,1,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel1_MaxSpeed",
     {
         .name = "Filter Wheel 1 Maximum Speed",
         .description = "",
         .getCommand = "SMF,1",
         .setCommand = "SMF,1,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel2_MaxAcceleration",
     {
         .name = "Filter Wheel 2 Maximum Acceleration",
         .description = "",
         .getCommand = "SAF,2",
         .setCommand = "SAF,2,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel2_SCurveValue",
     {
         .name = "Filter Wheel 2 S-Curve Value",
         .description = "",
         .getCommand = "SCF,2",
         .setCommand = "SCF,2,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel2_MaxSpeed",
     {
         .name = "Filter Wheel 2 Maximum Speed",
         .description = "",
         .getCommand = "SMF,2",
         .setCommand = "SMF,2,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel3_MaxAcceleration",
     {
         .name = "Filter Wheel 3 Maximum Acceleration",
         .description = "",
         .getCommand = "SAF,3",
         .setCommand = "SAF,3,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel3_SCurveValue",
     {
         .name = "Filter Wheel 3 S-Curve Value",
         .description = "",
         .getCommand = "SCF,3",
         .setCommand = "SCF,3,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"FilterWheel3_MaxSpeed",
     {
         .name = "Filter Wheel 3 Maximum Speed",
         .description = "",
         .getCommand = "SMF,3",
         .setCommand = "SMF,3,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
    {"TTLMotionIndicatorOut",
     {
         .name = "TTL Motion Indicator Out",
         .description = "",
         .getCommand = "",
         .setCommand = "TTLMOT,{}",
         .setResponse = "0",
         .isVolatile = false,
     }},
};

} // namespace PriorProscan
