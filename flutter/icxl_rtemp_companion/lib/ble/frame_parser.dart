import 'dart:typed_data';

import 'package:icxl_rtemp_companion/ble/uuids.dart';
import 'package:icxl_rtemp_companion/models/temp_sample.dart';

/// Parses the fixed 41-byte little-endian debug snapshot notify payload (`0x2111`).
///
/// Header (confirmed by firmware):
/// - offset 0: protocol_version (uint8)
/// - offset 1: flags (uint8)
/// - offset 2..5: sequence (uint32 LE)
///
/// Temperatures (int16 LE, units 0.01 °C; `0x7FFF` → null):
/// - offset 6:  sensor (raw)
/// - offset 10: skin (filtered)
/// - offset 14: published_core (core)
///
/// Tail status (confirmed by firmware):
/// - offset 32: quality (uint8)
/// - offset 33: sensor_status (uint8)
/// - offset 34: temperature_state (uint8)
/// - offset 35: core_state (uint8)
class FrameParser {
  const FrameParser();

  static const int offsetProtocolVersion = 0;
  static const int offsetFlags = 1;
  static const int offsetSequence = 2; // uint32 LE, bytes 2..5

  static const int offsetSensorRaw = 6;
  static const int offsetSkinFiltered = 10;
  static const int offsetPublishedCore = 14;

  static const int offsetQuality = 32;
  static const int offsetSensorStatus = 33;
  static const int offsetTemperatureState = 34;
  static const int offsetCoreState = 35;

  TempSample? parse(List<int> bytes, {DateTime? timestamp}) {
    if (bytes.length < kFrameLength) {
      return null;
    }
    final data = Uint8List.fromList(bytes.take(kFrameLength).toList());
    final bd = ByteData.sublistView(data);
    final ts = timestamp ?? DateTime.now();

    return TempSample(
      timestamp: ts,
      protocolVersion: data[offsetProtocolVersion],
      flags: data[offsetFlags],
      sequence: bd.getUint32(offsetSequence, Endian.little),
      sensorRaw: _tempAt(bd, offsetSensorRaw),
      skinFiltered: _tempAt(bd, offsetSkinFiltered),
      core: _tempAt(bd, offsetPublishedCore),
      quality: data[offsetQuality],
      sensorStatus: data[offsetSensorStatus],
      temperatureState: data[offsetTemperatureState],
      coreState: data[offsetCoreState],
    );
  }

  /// Converts int16 LE at [offset] to °C.
  /// Sentinel 0x7FFF as signed int16 is 32767 → null (do not plot).
  double? _tempAt(ByteData bd, int offset) {
    final raw = bd.getInt16(offset, Endian.little);
    if (raw == kInvalidTempSentinel) {
      return null;
    }
    return raw / 100.0;
  }

  /// Builds a synthetic 41-byte frame for offline mock mode.
  static List<int> buildMockFrame({
    required double sensorRawC,
    required double skinFilteredC,
    required double coreC,
    int protocolVersion = 1,
    int flags = 0x01,
    int sequence = 0,
    int quality = 0x00,
    int sensorStatus = 0x00,
    int temperatureState = 0x01,
    int coreState = 0x01,
    bool invalidateSensor = false,
    bool invalidateSkin = false,
    bool invalidateCore = false,
  }) {
    final data = Uint8List(kFrameLength);
    final bd = ByteData.sublistView(data);
    data[offsetProtocolVersion] = protocolVersion & 0xFF;
    data[offsetFlags] = flags & 0xFF;
    bd.setUint32(offsetSequence, sequence, Endian.little);
    data[offsetQuality] = quality & 0xFF;
    data[offsetSensorStatus] = sensorStatus & 0xFF;
    data[offsetTemperatureState] = temperatureState & 0xFF;
    data[offsetCoreState] = coreState & 0xFF;

    void putTemp(int offset, double c, bool invalidate) {
      final v = invalidate
          ? kInvalidTempSentinel
          : (c * 100).round().clamp(-32768, 32767);
      bd.setInt16(offset, v, Endian.little);
    }

    putTemp(offsetSensorRaw, sensorRawC, invalidateSensor);
    putTemp(offsetSkinFiltered, skinFilteredC, invalidateSkin);
    putTemp(offsetPublishedCore, coreC, invalidateCore);
    return data;
  }
}
