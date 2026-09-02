/// Aggregate connection / battery / key UI status.
enum ConnectionStateUi {
  disconnected,
  scanning,
  connecting,
  connected,
  mock,
}

class DeviceStatus {
  const DeviceStatus({
    this.connection = ConnectionStateUi.disconnected,
    this.deviceName,
    this.batteryPercent,
    this.protocolVersion,
    this.flags,
    this.sequence,
    this.quality,
    this.sensorStatus,
    this.temperatureState,
    this.coreState,
    this.lastError,
  });

  final ConnectionStateUi connection;
  final String? deviceName;

  /// Standard Battery Service level 0–100, null if unread.
  final int? batteryPercent;

  final int? protocolVersion;
  final int? flags;
  final int? sequence;
  final int? quality;
  final int? sensorStatus;
  final int? temperatureState;
  final int? coreState;

  final String? lastError;

  /// Keys are not exposed over BLE — always show placeholder.
  static const String keyPlaceholder = '未上报';

  DeviceStatus copyWith({
    ConnectionStateUi? connection,
    String? deviceName,
    int? batteryPercent,
    int? protocolVersion,
    int? flags,
    int? sequence,
    int? quality,
    int? sensorStatus,
    int? temperatureState,
    int? coreState,
    String? lastError,
    bool clearError = false,
    bool clearBattery = false,
  }) {
    return DeviceStatus(
      connection: connection ?? this.connection,
      deviceName: deviceName ?? this.deviceName,
      batteryPercent:
          clearBattery ? null : (batteryPercent ?? this.batteryPercent),
      protocolVersion: protocolVersion ?? this.protocolVersion,
      flags: flags ?? this.flags,
      sequence: sequence ?? this.sequence,
      quality: quality ?? this.quality,
      sensorStatus: sensorStatus ?? this.sensorStatus,
      temperatureState: temperatureState ?? this.temperatureState,
      coreState: coreState ?? this.coreState,
      lastError: clearError ? null : (lastError ?? this.lastError),
    );
  }
}
