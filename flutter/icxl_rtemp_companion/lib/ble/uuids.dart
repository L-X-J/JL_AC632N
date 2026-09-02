/// BLE UUID map for JL AC632N ICXL-RTemp companion protocol.
/// Always match by UUID — never by characteristic handle ordinals.
library;

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// Advertised device name used for scan filtering.
const String kDeviceAdvertisedName = 'ICXL-RTemp';

/// Primary temperature notify characteristic (41-byte debug snapshot).
final Guid kNotifyCharacteristicUuid =
    Guid('00002111-5B1E-4347-B07C-97B514DAE121');

/// Standard Battery Service.
final Guid kBatteryServiceUuid =
    Guid('0000180F-0000-1000-8000-00805F9B34FB');

/// Standard Battery Level characteristic.
final Guid kBatteryLevelUuid =
    Guid('00002A19-0000-1000-8000-00805F9B34FB');

/// Fixed notify payload length.
const int kFrameLength = 41;

/// Invalid temperature sentinel (int16 LE). Treat as null / do not plot.
const int kInvalidTempSentinel = 0x7FFF;
