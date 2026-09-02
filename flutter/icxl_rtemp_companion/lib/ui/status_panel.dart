import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';

class StatusPanel extends StatelessWidget {
  const StatusPanel({super.key, required this.status});

  final DeviceStatus status;

  String get _connLabel {
    switch (status.connection) {
      case ConnectionStateUi.disconnected:
        return '未连接';
      case ConnectionStateUi.scanning:
        return '扫描中…';
      case ConnectionStateUi.connecting:
        return '连接中…';
      case ConnectionStateUi.connected:
        return '已连接';
      case ConnectionStateUi.mock:
        return '模拟模式';
    }
  }

  Color get _connColor {
    switch (status.connection) {
      case ConnectionStateUi.connected:
      case ConnectionStateUi.mock:
        return Colors.green.shade700;
      case ConnectionStateUi.scanning:
      case ConnectionStateUi.connecting:
        return Colors.orange.shade700;
      case ConnectionStateUi.disconnected:
        return Colors.grey.shade700;
    }
  }

  String _hexOrDash(int? v) =>
      v == null ? '—' : '0x${v.toRadixString(16).padLeft(2, '0').toUpperCase()}';

  @override
  Widget build(BuildContext context) {
    final battery = status.batteryPercent;
    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      child: Padding(
        padding: const EdgeInsets.all(14),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              '状态面板',
              style: Theme.of(context).textTheme.titleMedium,
            ),
            const SizedBox(height: 10),
            _row('连接状态', _connLabel, valueColor: _connColor),
            _row(
              '设备',
              status.deviceName ?? '—',
            ),
            _row(
              '协议/序列',
              'ver=${status.protocolVersion ?? '—'}  '
                  'flags=${_hexOrDash(status.flags)}  '
                  'seq=${status.sequence ?? '—'}',
            ),
            _row(
              '传感器状态',
              'quality=${_hexOrDash(status.quality)}  '
                  'sensor=${_hexOrDash(status.sensorStatus)}  '
                  'temp=${_hexOrDash(status.temperatureState)}  '
                  'core=${_hexOrDash(status.coreState)}',
            ),
            _row(
              '电量',
              battery == null ? '—' : '$battery%',
            ),
            _row('按键', DeviceStatus.keyPlaceholder),
            if (status.lastError != null) ...[
              const SizedBox(height: 6),
              Text(
                status.lastError!,
                style: TextStyle(color: Colors.red.shade700, fontSize: 12),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _row(String label, String value, {Color? valueColor}) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 3),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          SizedBox(
            width: 88,
            child: Text(
              label,
              style: const TextStyle(
                fontWeight: FontWeight.w600,
                color: Colors.black54,
              ),
            ),
          ),
          Expanded(
            child: Text(
              value,
              style: TextStyle(color: valueColor),
            ),
          ),
        ],
      ),
    );
  }
}
