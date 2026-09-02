import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';
import 'package:icxl_rtemp_companion/theme/app_theme.dart';
import 'package:icxl_rtemp_companion/ui/status_labels.dart';

/// Inset-grouped status card (连接 / 传感器 / 温度状态 / 核心状态 / 电量 / 按键).
class StatusPanel extends StatelessWidget {
  const StatusPanel({super.key, required this.status});

  final DeviceStatus status;

  String get _connLabel {
    switch (status.connection) {
      case ConnectionStateUi.disconnected:
        return '未连接';
      case ConnectionStateUi.scanning:
        return '扫描中';
      case ConnectionStateUi.connecting:
        return '连接中';
      case ConnectionStateUi.connected:
        return '已连接';
      case ConnectionStateUi.mock:
        return '模拟';
    }
  }

  Color _connColor(BuildContext context) {
    switch (status.connection) {
      case ConnectionStateUi.connected:
      case ConnectionStateUi.mock:
        return AppTokens.semanticGreen;
      case ConnectionStateUi.scanning:
      case ConnectionStateUi.connecting:
        return AppTokens.semanticAmber;
      case ConnectionStateUi.disconnected:
        return secondaryLabel(context);
    }
  }

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final battery = status.batteryPercent;
    final rows = <_StatusRowData>[
      _StatusRowData('连接', _connLabel, valueColor: _connColor(context)),
      _StatusRowData('传感器', StatusLabels.sensor(status.sensorStatus)),
      _StatusRowData('温度状态', StatusLabels.temperature(status.temperatureState)),
      _StatusRowData('核心状态', StatusLabels.core(status.coreState)),
      _StatusRowData('电量', battery == null ? '—' : '$battery%'),
      _StatusRowData(
        '按键',
        DeviceStatus.keyPlaceholder,
        valueColor: secondaryLabel(context),
      ),
    ];

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 4, bottom: 8),
          child: Text('状态', style: theme.textTheme.titleMedium),
        ),
        Container(
          decoration: BoxDecoration(
            color: cardBackground(context),
            borderRadius: BorderRadius.circular(AppTokens.cardRadius),
          ),
          padding: const EdgeInsets.symmetric(vertical: 4),
          child: Column(
            children: [
              for (var i = 0; i < rows.length; i++) ...[
                _StatusRow(data: rows[i]),
                if (i < rows.length - 1)
                  Divider(
                    height: 1,
                    indent: 16,
                    color: Theme.of(context).dividerColor,
                  ),
              ],
            ],
          ),
        ),
      ],
    );
  }
}

class _StatusRowData {
  const _StatusRowData(this.label, this.value, {this.valueColor});
  final String label;
  final String value;
  final Color? valueColor;
}

class _StatusRow extends StatelessWidget {
  const _StatusRow({required this.data});

  final _StatusRowData data;

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    return ConstrainedBox(
      constraints: const BoxConstraints(minHeight: AppTokens.minTouch),
      child: Padding(
        padding: const EdgeInsets.symmetric(
          horizontal: AppTokens.cardPadding,
          vertical: 8,
        ),
        child: Row(
          children: [
            Expanded(
              child: Text(data.label, style: theme.textTheme.bodyLarge),
            ),
            Flexible(
              child: Text(
                data.value,
                textAlign: TextAlign.end,
                style: theme.textTheme.bodyLarge?.copyWith(
                  color: data.valueColor ?? secondaryLabel(context),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
