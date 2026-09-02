/// One parsed temperature sample from a 41-byte debug snapshot frame.
class TempSample {
  TempSample({
    required this.timestamp,
    this.protocolVersion,
    this.flags,
    this.sequence,
    this.sensorRaw,
    this.skinFiltered,
    this.core,
    this.quality,
    this.sensorStatus,
    this.temperatureState,
    this.coreState,
  });

  final DateTime timestamp;

  final int? protocolVersion;
  final int? flags;
  final int? sequence;

  /// °C, null when sentinel 0x7FFF.
  final double? sensorRaw;
  final double? skinFiltered;
  final double? core;

  final int? quality;
  final int? sensorStatus;
  final int? temperatureState;
  final int? coreState;

  bool get hasAnyTemp =>
      sensorRaw != null || skinFiltered != null || core != null;
}
