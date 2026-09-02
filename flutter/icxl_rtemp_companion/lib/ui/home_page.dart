import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/export/csv_exporter.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';
import 'package:icxl_rtemp_companion/ui/status_panel.dart';
import 'package:icxl_rtemp_companion/ui/temp_chart.dart';

class HomePage extends StatefulWidget {
  const HomePage({super.key, required this.controller});

  final BleController controller;

  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final _exporter = CsvExporter();
  bool _exporting = false;

  BleController get c => widget.controller;

  Future<void> _export() async {
    if (c.samples.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('暂无数据可导出')),
      );
      return;
    }
    setState(() => _exporting = true);
    try {
      final path = await _exporter.exportAndShare(c.samples);
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('已导出: $path')),
        );
      }
    } catch (e) {
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('导出失败: $e')),
        );
      }
    } finally {
      if (mounted) setState(() => _exporting = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: c,
      builder: (context, _) {
        final st = c.status;
        final connected = st.connection == ConnectionStateUi.connected ||
            st.connection == ConnectionStateUi.mock;
        final scanning = st.connection == ConnectionStateUi.scanning;

        return Scaffold(
          appBar: AppBar(
            title: const Text('ICXL 核心体温伴侣'),
            actions: [
              IconButton(
                tooltip: '导出 CSV',
                onPressed: _exporting ? null : _export,
                icon: _exporting
                    ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Icon(Icons.ios_share),
              ),
            ],
          ),
          body: ListView(
            children: [
              Padding(
                padding:
                    const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                child: Card(
                  child: Padding(
                    padding: const EdgeInsets.all(12),
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        Text(
                          '连接控制',
                          style: Theme.of(context).textTheme.titleMedium,
                        ),
                        const SizedBox(height: 8),
                        SwitchListTile(
                          contentPadding: EdgeInsets.zero,
                          title: const Text('模拟模式'),
                          subtitle: const Text('离线演示，约 5 Hz 正弦体温'),
                          value: c.mockMode,
                          onChanged: (v) => c.setMockMode(v),
                        ),
                        const SizedBox(height: 4),
                        Wrap(
                          spacing: 8,
                          runSpacing: 8,
                          children: [
                            FilledButton.icon(
                              onPressed: c.mockMode || scanning
                                  ? null
                                  : () => c.startScan(),
                              icon: const Icon(Icons.bluetooth_searching),
                              label: Text(scanning ? '扫描中…' : '扫描 ICXL-RTemp'),
                            ),
                            OutlinedButton.icon(
                              onPressed: scanning ? () => c.stopScan() : null,
                              icon: const Icon(Icons.stop),
                              label: const Text('停止扫描'),
                            ),
                            OutlinedButton.icon(
                              onPressed: connected && !c.mockMode
                                  ? () => c.disconnect(clearSamples: false)
                                  : null,
                              icon: const Icon(Icons.link_off),
                              label: const Text('断开'),
                            ),
                            TextButton.icon(
                              onPressed:
                                  c.samples.isEmpty ? null : c.clearSamples,
                              icon: const Icon(Icons.clear_all),
                              label: const Text('清空曲线'),
                            ),
                          ],
                        ),
                        if (!c.mockMode && c.scanResults.isNotEmpty) ...[
                          const Divider(height: 20),
                          Text(
                            '发现的设备',
                            style: Theme.of(context).textTheme.titleSmall,
                          ),
                          ...c.scanResults.map(_deviceTile),
                        ],
                      ],
                    ),
                  ),
                ),
              ),
              StatusPanel(status: st),
              TempChart(samples: c.samples),
              const SizedBox(height: 24),
            ],
          ),
        );
      },
    );
  }

  Widget _deviceTile(ScanResult r) {
    final name = r.advertisementData.advName.isNotEmpty
        ? r.advertisementData.advName
        : (r.device.platformName.isNotEmpty
            ? r.device.platformName
            : r.device.remoteId.str);
    return ListTile(
      dense: true,
      leading: const Icon(Icons.thermostat),
      title: Text(name),
      subtitle: Text('${r.device.remoteId.str}  ·  RSSI ${r.rssi}'),
      trailing: FilledButton(
        onPressed: () => c.connect(r.device),
        child: const Text('连接'),
      ),
    );
  }
}
