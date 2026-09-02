import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/export/csv_exporter.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';
import 'package:icxl_rtemp_companion/theme/app_theme.dart';
import 'package:icxl_rtemp_companion/ui/status_panel.dart';
import 'package:icxl_rtemp_companion/ui/temp_chart.dart';

/// Device page (connected) — status card + temperature chart + CSV share.
class DevicePage extends StatefulWidget {
  const DevicePage({super.key, required this.controller});

  final BleController controller;

  @override
  State<DevicePage> createState() => _DevicePageState();
}

class _DevicePageState extends State<DevicePage> {
  final _exporter = CsvExporter();
  bool _exporting = false;
  bool _intentionalDisconnect = false;

  BleController get c => widget.controller;

  @override
  void initState() {
    super.initState();
    c.addListener(_onController);
  }

  @override
  void dispose() {
    c.removeListener(_onController);
    super.dispose();
  }

  void _onController() {
    if (!mounted) return;
    final st = c.status.connection;
    if (_intentionalDisconnect && st == ConnectionStateUi.disconnected) {
      Navigator.of(context).pop();
      return;
    }
    setState(() {});
  }

  Future<void> _disconnect() async {
    _intentionalDisconnect = true;
    if (c.mockMode) {
      c.setMockMode(false);
      if (mounted) Navigator.of(context).pop();
      return;
    }
    await c.disconnect(clearSamples: false);
    if (mounted) Navigator.of(context).pop();
  }

  Future<void> _export() async {
    setState(() => _exporting = true);
    try {
      if (c.samples.isEmpty) {
        throw StateError('empty');
      }
      await _exporter.exportAndShare(c.samples);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('已导出')),
        );
      }
    } catch (_) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('导出失败，请重试')),
        );
      }
    } finally {
      if (mounted) setState(() => _exporting = false);
    }
  }

  bool get _showReconnectBanner {
    final st = c.status.connection;
    if (st == ConnectionStateUi.connected || st == ConnectionStateUi.mock) {
      return false;
    }
    // Unexpected drop or reconnect attempt while still on this page.
    return st == ConnectionStateUi.connecting ||
        st == ConnectionStateUi.disconnected;
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final st = c.status;
    final title = st.deviceName?.isNotEmpty == true
        ? st.deviceName!
        : 'ICXL-RTemp';

    return Scaffold(
      body: SafeArea(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Padding(
              padding: const EdgeInsets.fromLTRB(
                AppTokens.pageHorizontal,
                4,
                8,
                0,
              ),
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Expanded(
                    child: Text(
                      title,
                      style: theme.textTheme.displayLarge,
                      maxLines: 2,
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  TextButton(
                    onPressed: _disconnect,
                    style: TextButton.styleFrom(
                      foregroundColor: AppTokens.semanticRed,
                      minimumSize: const Size(44, AppTokens.minTouch),
                    ),
                    child: const Text(
                      '断开',
                      style: TextStyle(
                        fontSize: AppTokens.bodySize,
                        fontWeight: FontWeight.w400,
                      ),
                    ),
                  ),
                ],
              ),
            ),
            if (_showReconnectBanner)
              Padding(
                padding: const EdgeInsets.fromLTRB(
                  AppTokens.pageHorizontal,
                  8,
                  AppTokens.pageHorizontal,
                  0,
                ),
                child: Container(
                  padding: const EdgeInsets.symmetric(
                    horizontal: 12,
                    vertical: 10,
                  ),
                  decoration: BoxDecoration(
                    color: AppTokens.semanticAmber.withValues(alpha: 0.15),
                    borderRadius: BorderRadius.circular(10),
                  ),
                  child: Row(
                    children: [
                      const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          color: AppTokens.semanticAmber,
                        ),
                      ),
                      const SizedBox(width: 10),
                      Expanded(
                        child: Text(
                          '已断开，正在重连…',
                          style: theme.textTheme.bodyMedium?.copyWith(
                            color: AppTokens.semanticAmber,
                            fontSize: AppTokens.secondarySize,
                          ),
                        ),
                      ),
                    ],
                  ),
                ),
              ),
            Expanded(
              child: ListView(
                padding: const EdgeInsets.fromLTRB(
                  AppTokens.pageHorizontal,
                  16,
                  AppTokens.pageHorizontal,
                  24,
                ),
                children: [
                  StatusPanel(status: st),
                  const SizedBox(height: AppTokens.betweenCards),
                  TempChart(samples: c.samples),
                  const SizedBox(height: AppTokens.betweenCards),
                  SizedBox(
                    width: double.infinity,
                    height: AppTokens.minTouch,
                    child: OutlinedButton(
                      onPressed: _exporting ? null : _export,
                      child: _exporting
                          ? const SizedBox(
                              width: 20,
                              height: 20,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Text('导出 CSV'),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
