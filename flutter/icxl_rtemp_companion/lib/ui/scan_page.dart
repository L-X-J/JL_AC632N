import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/ble/uuids.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';
import 'package:icxl_rtemp_companion/theme/app_theme.dart';
import 'package:icxl_rtemp_companion/ui/device_page.dart';
import 'package:permission_handler/permission_handler.dart';

/// Scan page (disconnected) — Apple HIG inset-grouped device list.
class ScanPage extends StatefulWidget {
  const ScanPage({super.key, required this.controller});

  final BleController controller;

  @override
  State<ScanPage> createState() => _ScanPageState();
}

class _ScanPageState extends State<ScanPage> {
  BleController get c => widget.controller;

  bool _bluetoothDenied = false;
  String? _connectingId;

  @override
  void initState() {
    super.initState();
    c.addListener(_onController);
    WidgetsBinding.instance.addPostFrameCallback((_) => _refreshPermission());
  }

  @override
  void dispose() {
    c.removeListener(_onController);
    super.dispose();
  }

  void _onController() {
    if (!mounted) return;
    final st = c.status.connection;
    if (st == ConnectionStateUi.connected || st == ConnectionStateUi.mock) {
      _connectingId = null;
      _openDeviceIfNeeded();
    } else if (st == ConnectionStateUi.disconnected && _connectingId != null) {
      final err = c.status.lastError;
      setState(() => _connectingId = null);
      if (err != null && err.contains('连接失败')) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('连接失败')),
        );
      }
    } else {
      setState(() {});
    }
  }

  bool _deviceOpen = false;

  void _openDeviceIfNeeded() {
    if (_deviceOpen || !mounted) return;
    _deviceOpen = true;
    Navigator.of(context)
        .push(
      MaterialPageRoute<void>(
        builder: (_) => DevicePage(controller: c),
      ),
    )
        .then((_) {
      _deviceOpen = false;
      if (mounted) setState(() {});
    });
  }

  Future<void> _refreshPermission() async {
    final scan = await Permission.bluetoothScan.status;
    final connect = await Permission.bluetoothConnect.status;
    // Hard-denied → show 「打开设置」; soft deny still allows 扫描 to re-prompt.
    final hardDenied =
        scan.isPermanentlyDenied || connect.isPermanentlyDenied;
    if (mounted) setState(() => _bluetoothDenied = hardDenied);
  }

  Future<void> _onScanPressed() async {
    await _refreshPermission();
    if (_bluetoothDenied) {
      setState(() {});
      return;
    }
    final ok = await c.ensurePermissions();
    if (!ok) {
      final scan = await Permission.bluetoothScan.status;
      final connect = await Permission.bluetoothConnect.status;
      if (scan.isPermanentlyDenied || connect.isPermanentlyDenied) {
        if (mounted) setState(() => _bluetoothDenied = true);
        return;
      }
    }
    await c.startScan();
  }

  Future<void> _onStopPressed() => c.stopScan().then((_) {
        if (c.status.connection == ConnectionStateUi.scanning) {
          // Controller may leave scanning until timeout; force UI idle label.
        }
        setState(() {});
      });

  Future<void> _connect(ScanResult r) async {
    if (_connectingId != null) return;
    setState(() => _connectingId = r.device.remoteId.str);
    await c.connect(r.device);
    if (!mounted) return;
    if (c.status.connection != ConnectionStateUi.connected &&
        c.status.connection != ConnectionStateUi.mock) {
      setState(() => _connectingId = null);
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('连接失败')),
      );
    }
  }

  String _rssiHint(int rssi) {
    if (rssi >= -60) return '信号较强';
    if (rssi >= -75) return '信号一般';
    return '信号较弱';
  }

  String _deviceName(ScanResult r) {
    if (r.advertisementData.advName.isNotEmpty) {
      return r.advertisementData.advName;
    }
    if (r.device.platformName.isNotEmpty) return r.device.platformName;
    return kDeviceAdvertisedName;
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: c,
      builder: (context, _) {
        final scanning = c.status.connection == ConnectionStateUi.scanning;
        final theme = Theme.of(context);

        if (_bluetoothDenied) {
          return Scaffold(
            body: SafeArea(
              child: Center(
                child: Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 32),
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Text(
                        '需要蓝牙才能查找设备',
                        textAlign: TextAlign.center,
                        style: theme.textTheme.bodyLarge,
                      ),
                      const SizedBox(height: 24),
                      SizedBox(
                        width: double.infinity,
                        child: FilledButton(
                          onPressed: () async {
                            await openAppSettings();
                            await _refreshPermission();
                          },
                          child: const Text('打开设置'),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            ),
          );
        }

        return Scaffold(
          body: SafeArea(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Padding(
                  padding: const EdgeInsets.fromLTRB(
                    AppTokens.pageHorizontal,
                    8,
                    AppTokens.pageHorizontal,
                    0,
                  ),
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      GestureDetector(
                        onLongPress: () {
                          // Discreet mock entry: long-press title.
                          c.setMockMode(!c.mockMode);
                          if (c.mockMode) {
                            _openDeviceIfNeeded();
                          }
                        },
                        child: Text('设备', style: theme.textTheme.displayLarge),
                      ),
                      const SizedBox(height: 4),
                      Text(
                        '附近的 ICXL-RTemp',
                        style: theme.textTheme.bodyMedium,
                      ),
                      const SizedBox(height: 8),
                      // Discreet secondary mock switch
                      Row(
                        children: [
                          Text('模拟', style: theme.textTheme.bodyMedium),
                          const SizedBox(width: 8),
                          SizedBox(
                            height: 28,
                            child: FittedBox(
                              child: CupertinoSwitch(
                                value: c.mockMode,
                                activeTrackColor: AppTokens.systemBlue,
                                onChanged: (v) {
                                  c.setMockMode(v);
                                  if (v) {
                                    _openDeviceIfNeeded();
                                  }
                                },
                              ),
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                const SizedBox(height: 16),
                Expanded(
                  child: c.scanResults.isEmpty
                      ? _emptyState(theme, scanning)
                      : ListView(
                          padding: const EdgeInsets.symmetric(
                            horizontal: AppTokens.pageHorizontal,
                          ),
                          children: [
                            _InsetGroup(
                              children: [
                                for (var i = 0; i < c.scanResults.length; i++)
                                  _deviceRow(
                                    theme,
                                    c.scanResults[i],
                                    showDivider: i < c.scanResults.length - 1,
                                  ),
                              ],
                            ),
                          ],
                        ),
                ),
                Padding(
                  padding: const EdgeInsets.fromLTRB(
                    AppTokens.pageHorizontal,
                    8,
                    AppTokens.pageHorizontal,
                    16,
                  ),
                  child: SizedBox(
                    width: double.infinity,
                    height: AppTokens.minTouch,
                    child: FilledButton(
                      onPressed: scanning ? _onStopPressed : _onScanPressed,
                      child: Text(scanning ? '停止' : '扫描'),
                    ),
                  ),
                ),
              ],
            ),
          ),
        );
      },
    );
  }

  Widget _emptyState(ThemeData theme, bool scanning) {
    return Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Text('未发现设备', style: theme.textTheme.bodyLarge),
          const SizedBox(height: 12),
          TextButton(
            onPressed: scanning ? null : _onScanPressed,
            child: Text(
              '重新扫描',
              style: TextStyle(
                fontSize: AppTokens.secondarySize,
                color: scanning
                    ? secondaryLabel(context)
                    : AppTokens.systemBlue,
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _deviceRow(ThemeData theme, ScanResult r, {required bool showDivider}) {
    final id = r.device.remoteId.str;
    final connecting = _connectingId == id;
    return Column(
      children: [
        Material(
          color: Colors.transparent,
          child: InkWell(
            onTap: connecting ? null : () => _connect(r),
            child: ConstrainedBox(
              constraints: const BoxConstraints(minHeight: AppTokens.minTouch),
              child: Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: 16,
                  vertical: 10,
                ),
                child: Row(
                  children: [
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            _deviceName(r),
                            style: theme.textTheme.bodyLarge,
                          ),
                          const SizedBox(height: 2),
                          Text(
                            _rssiHint(r.rssi),
                            style: theme.textTheme.bodySmall,
                          ),
                        ],
                      ),
                    ),
                    if (connecting)
                      const SizedBox(
                        width: 22,
                        height: 22,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    else
                      Icon(
                        CupertinoIcons.chevron_forward,
                        size: 18,
                        color: secondaryLabel(context),
                      ),
                  ],
                ),
              ),
            ),
          ),
        ),
        if (showDivider)
          Divider(
            height: 1,
            indent: 16,
            color: Theme.of(context).dividerColor,
          ),
      ],
    );
  }
}

class _InsetGroup extends StatelessWidget {
  const _InsetGroup({required this.children});

  final List<Widget> children;

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: cardBackground(context),
        borderRadius: BorderRadius.circular(AppTokens.cardRadius),
      ),
      clipBehavior: Clip.antiAlias,
      child: Column(children: children),
    );
  }
}
