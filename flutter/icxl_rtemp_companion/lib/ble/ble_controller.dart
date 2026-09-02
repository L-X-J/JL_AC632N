import 'dart:async';
import 'dart:io' show Platform;
import 'dart:math';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:icxl_rtemp_companion/ble/frame_parser.dart';
import 'package:icxl_rtemp_companion/ble/uuids.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';
import 'package:icxl_rtemp_companion/models/temp_sample.dart';
import 'package:permission_handler/permission_handler.dart';

/// BLE + mock controller for ICXL-RTemp.
///
/// Critical connect order: **requestMtu(247) before setNotifyValue(true)**
/// so the 41-byte notify payload is not fragmented on Android.
class BleController extends ChangeNotifier {
  BleController({FrameParser parser = const FrameParser()}) : _parser = parser;

  final FrameParser _parser;

  DeviceStatus _status = const DeviceStatus();
  DeviceStatus get status => _status;

  final List<TempSample> _samples = <TempSample>[];
  List<TempSample> get samples => List.unmodifiable(_samples);

  final StreamController<TempSample> _sampleController =
      StreamController<TempSample>.broadcast();
  Stream<TempSample> get sampleStream => _sampleController.stream;

  /// Rolling chart window (last 5 minutes).
  static const Duration chartWindow = Duration(minutes: 5);
  static const int maxSamples = 1200;

  BluetoothDevice? _device;
  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<List<int>>? _notifySub;
  StreamSubscription<BluetoothConnectionState>? _connSub;
  Timer? _mockTimer;
  Timer? _batteryTimer;
  bool _mockMode = false;
  bool get mockMode => _mockMode;

  final List<ScanResult> _scanResults = <ScanResult>[];
  List<ScanResult> get scanResults => List.unmodifiable(_scanResults);

  Future<bool> ensurePermissions() async {
    if (kIsWeb) return false;
    final perms = <Permission>[
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ];
    // iOS: bluetooth permission is covered by bluetoothScan/Connect aliases
    // and Info.plist NSBluetoothAlwaysUsageDescription.
    final statuses = await perms.request();
    return statuses.values.every(
      (s) => s.isGranted || s.isLimited || s.isRestricted,
    );
  }

  Future<void> startScan({Duration timeout = const Duration(seconds: 12)}) async {
    if (_mockMode) return;
    await stopScan();
    _scanResults.clear();
    _setStatus(_status.copyWith(
      connection: ConnectionStateUi.scanning,
      clearError: true,
    ));

    final supported = await FlutterBluePlus.isSupported;
    if (!supported) {
      _setStatus(_status.copyWith(
        connection: ConnectionStateUi.disconnected,
        lastError: '此设备不支持蓝牙',
      ));
      return;
    }

    try {
      await FlutterBluePlus.adapterState
          .where((s) => s == BluetoothAdapterState.on)
          .first
          .timeout(const Duration(seconds: 5));
    } catch (_) {
      _setStatus(_status.copyWith(
        connection: ConnectionStateUi.disconnected,
        lastError: '请先打开蓝牙',
      ));
      return;
    }

    await ensurePermissions();

    _scanSub = FlutterBluePlus.onScanResults.listen((results) {
      // Primary filter: advertised / platform name == ICXL-RTemp
      final filtered = results.where((r) {
        final name = r.advertisementData.advName.isNotEmpty
            ? r.advertisementData.advName
            : r.device.platformName;
        return name == kDeviceAdvertisedName;
      }).toList();

      _scanResults
        ..clear()
        ..addAll(filtered);
      final seen = <String>{};
      _scanResults.retainWhere((r) => seen.add(r.device.remoteId.str));
      notifyListeners();
    }, onError: (Object e) {
      _setStatus(_status.copyWith(lastError: '扫描错误: $e'));
    });

    try {
      await FlutterBluePlus.startScan(
        timeout: timeout,
        withNames: [kDeviceAdvertisedName],
        androidUsesFineLocation: true,
      );
    } catch (e) {
      // withNames may not be supported on all platforms — fallback.
      try {
        await FlutterBluePlus.startScan(
          timeout: timeout,
          androidUsesFineLocation: true,
        );
      } catch (e2) {
        _setStatus(_status.copyWith(
          connection: ConnectionStateUi.disconnected,
          lastError: '无法开始扫描: $e2',
        ));
      }
    }

    Future<void>.delayed(timeout, () {
      if (_status.connection == ConnectionStateUi.scanning) {
        _setStatus(
          _status.copyWith(connection: ConnectionStateUi.disconnected),
        );
      }
    });
  }

  Future<void> stopScan() async {
    try {
      await FlutterBluePlus.stopScan();
    } catch (_) {}
    await _scanSub?.cancel();
    _scanSub = null;
    if (_status.connection == ConnectionStateUi.scanning) {
      _setStatus(
        _status.copyWith(connection: ConnectionStateUi.disconnected),
      );
    }
  }

  Future<void> connect(BluetoothDevice device) async {
    if (_mockMode) return;
    await stopScan();
    await disconnect(clearSamples: false);

    _device = device;
    _setStatus(_status.copyWith(
      connection: ConnectionStateUi.connecting,
      deviceName: device.platformName.isNotEmpty
          ? device.platformName
          : kDeviceAdvertisedName,
      clearError: true,
      clearBattery: true,
    ));

    _connSub = device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected &&
          _status.connection == ConnectionStateUi.connected) {
        _onDisconnected('设备已断开');
      }
    });

    try {
      // mtu:null — we request 247 explicitly before CCCD below.
      await device.connect(
        license: License.nonprofit,
        timeout: const Duration(seconds: 15),
        mtu: null,
      );
      await device.discoverServices();

      BluetoothCharacteristic? notifyChar;
      for (final service in device.servicesList) {
        for (final c in service.characteristics) {
          if (c.uuid == kNotifyCharacteristicUuid) {
            notifyChar = c;
            break;
          }
        }
        if (notifyChar != null) break;
      }

      if (notifyChar == null) {
        throw StateError(
          '未找到通知特征 $kNotifyCharacteristicUuid',
        );
      }

      // ── CRITICAL: request MTU 247 BEFORE enabling CCCD / notify ──
      // Android only; iOS negotiates ATT MTU differently.
      if (!kIsWeb && Platform.isAndroid) {
        try {
          await device.requestMtu(247);
        } catch (e) {
          debugPrint('requestMtu(247) failed: $e');
        }
      }

      await notifyChar.setNotifyValue(true);
      await _notifySub?.cancel();
      _notifySub = notifyChar.onValueReceived.listen(_onNotify);

      _setStatus(_status.copyWith(connection: ConnectionStateUi.connected));
      unawaited(_readBattery(device));
      _batteryTimer?.cancel();
      _batteryTimer = Timer.periodic(const Duration(seconds: 60), (_) {
        final d = _device;
        if (d != null) unawaited(_readBattery(d));
      });
    } catch (e) {
      _setStatus(_status.copyWith(
        connection: ConnectionStateUi.disconnected,
        lastError: '连接失败: $e',
      ));
      await disconnect();
    }
  }

  Future<void> _readBattery(BluetoothDevice device) async {
    try {
      for (final service in device.servicesList) {
        if (service.uuid != kBatteryServiceUuid) continue;
        for (final c in service.characteristics) {
          if (c.uuid != kBatteryLevelUuid) continue;
          final value = await c.read();
          if (value.isNotEmpty) {
            _setStatus(
              _status.copyWith(batteryPercent: value.first.clamp(0, 100)),
            );
          }
          return;
        }
      }
    } catch (e) {
      debugPrint('Battery read failed: $e');
    }
  }

  void _onNotify(List<int> value) {
    final sample = _parser.parse(value);
    if (sample == null) return;
    _appendSample(sample);
    _setStatus(_status.copyWith(
      protocolVersion: sample.protocolVersion,
      flags: sample.flags,
      sequence: sample.sequence,
      quality: sample.quality,
      sensorStatus: sample.sensorStatus,
      temperatureState: sample.temperatureState,
      coreState: sample.coreState,
    ));
  }

  void _appendSample(TempSample sample) {
    _samples.add(sample);
    if (!_sampleController.isClosed) {
      _sampleController.add(sample);
    }
    final cutoff = DateTime.now().subtract(chartWindow);
    while (_samples.isNotEmpty && _samples.first.timestamp.isBefore(cutoff)) {
      _samples.removeAt(0);
    }
    while (_samples.length > maxSamples) {
      _samples.removeAt(0);
    }
    if (!_disposed) notifyListeners();
  }

  void _onDisconnected(String reason) {
    _notifySub?.cancel();
    _notifySub = null;
    _batteryTimer?.cancel();
    _batteryTimer = null;
    _setStatus(_status.copyWith(
      connection: ConnectionStateUi.disconnected,
      lastError: reason,
      clearBattery: true,
    ));
  }

  Future<void> disconnect({bool clearSamples = false}) async {
    await _notifySub?.cancel();
    _notifySub = null;
    await _connSub?.cancel();
    _connSub = null;
    _batteryTimer?.cancel();
    _batteryTimer = null;
    final d = _device;
    _device = null;
    if (d != null) {
      try {
        await d.disconnect();
      } catch (_) {}
    }
    if (clearSamples) _samples.clear();
    if (!_mockMode) {
      _setStatus(_status.copyWith(
        connection: ConnectionStateUi.disconnected,
        clearBattery: true,
      ));
    }
  }

  // ── Mock / offline demo ──────────────────────────────────────────

  void setMockMode(bool enabled) {
    if (_mockMode == enabled) return;
    _mockMode = enabled;
    if (enabled) {
      unawaited(disconnect(clearSamples: true));
      _startMock();
    } else {
      _stopMock();
      _setStatus(const DeviceStatus());
      _samples.clear();
    }
  }

  void _startMock() {
    _stopMock();
    _setStatus(const DeviceStatus(
      connection: ConnectionStateUi.mock,
      deviceName: 'ICXL-RTemp (Mock)',
      batteryPercent: 87,
      protocolVersion: 1,
      flags: 0x01,
      sequence: 0,
      quality: 0x00,
      sensorStatus: 0x00,
      temperatureState: 0x01,
      coreState: 0x01,
    ));
    final rng = Random(42);
    var t = 0.0;
    _mockTimer = Timer.periodic(const Duration(milliseconds: 200), (_) {
      t += 0.2;
      // Sine-ish physiology around body temperature.
      final core = 36.6 + 0.3 * sin(t / 20) + rng.nextDouble() * 0.04;
      final skin = core - 0.8 + 0.15 * sin(t / 8) + rng.nextDouble() * 0.05;
      final raw = skin + 0.2 * sin(t / 3) + (rng.nextDouble() - 0.5) * 0.15;

      // Occasionally inject 0x7FFF gaps (~2%).
      final gap = rng.nextDouble() < 0.02;
      final frame = FrameParser.buildMockFrame(
        sensorRawC: raw,
        skinFilteredC: skin,
        coreC: core,
        invalidateSensor: gap && rng.nextBool(),
        invalidateSkin: gap && rng.nextBool(),
        invalidateCore: gap,
      );
      final sample = _parser.parse(frame);
      if (sample != null) {
        _appendSample(sample);
        _setStatus(_status.copyWith(
          protocolVersion: sample.protocolVersion,
          flags: sample.flags,
          sequence: sample.sequence,
          quality: sample.quality,
          sensorStatus: sample.sensorStatus,
          temperatureState: sample.temperatureState,
          coreState: sample.coreState,
          batteryPercent: 87 - (t ~/ 120).clamp(0, 20),
        ));
      }
    });
  }

  void _stopMock() {
    _mockTimer?.cancel();
    _mockTimer = null;
  }

  void clearSamples() {
    _samples.clear();
    if (!_disposed) notifyListeners();
  }

  bool _disposed = false;

  void _setStatus(DeviceStatus s) {
    _status = s;
    if (!_disposed) notifyListeners();
  }

  @override
  void dispose() {
    _disposed = true;
    _stopMock();
    _batteryTimer?.cancel();
    _batteryTimer = null;
    _notifySub?.cancel();
    _notifySub = null;
    _connSub?.cancel();
    _connSub = null;
    _scanSub?.cancel();
    _scanSub = null;
    final d = _device;
    _device = null;
    if (d != null) {
      unawaited(d.disconnect());
    }
    unawaited(stopScan());
    if (!_sampleController.isClosed) {
      _sampleController.close();
    }
    super.dispose();
  }
}
