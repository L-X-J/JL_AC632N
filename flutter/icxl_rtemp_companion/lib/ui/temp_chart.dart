import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/models/temp_sample.dart';

/// Rolling ~3 min temperature chart. Null / 0x7FFF values are skipped (gaps).
class TempChart extends StatelessWidget {
  const TempChart({super.key, required this.samples});

  final List<TempSample> samples;

  static const _rawColor = Color(0xFFE53935);
  static const _skinColor = Color(0xFF1E88E5);
  static const _coreColor = Color(0xFF43A047);

  @override
  Widget build(BuildContext context) {
    final window = BleController.chartWindow;
    final now = samples.isEmpty ? DateTime.now() : samples.last.timestamp;
    final origin = now.subtract(window);
    final maxX = window.inMilliseconds.toDouble();

    List<FlSpot> spots(double? Function(TempSample) pick) {
      final out = <FlSpot>[];
      for (final s in samples) {
        final v = pick(s);
        if (v == null) continue; // skip sentinel / null gaps
        final x = s.timestamp.difference(origin).inMilliseconds.toDouble();
        if (x < 0) continue;
        out.add(FlSpot(x, v));
      }
      return out;
    }

    final raw = spots((s) => s.sensorRaw);
    final skin = spots((s) => s.skinFiltered);
    final core = spots((s) => s.core);

    double? minY;
    double? maxY;
    for (final list in [raw, skin, core]) {
      for (final p in list) {
        minY = minY == null ? p.y : (p.y < minY ? p.y : minY);
        maxY = maxY == null ? p.y : (p.y > maxY ? p.y : maxY);
      }
    }
    minY ??= 35.0;
    maxY ??= 38.0;
    final pad = ((maxY - minY) * 0.15).clamp(0.2, 1.0);
    minY = (minY - pad).clamp(30.0, 45.0);
    maxY = (maxY + pad).clamp(30.0, 45.0);

    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      child: Padding(
        padding: const EdgeInsets.fromLTRB(8, 12, 16, 8),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Padding(
              padding: const EdgeInsets.only(left: 8, bottom: 8),
              child: Text(
                '温度曲线（约 3 分钟）',
                style: Theme.of(context).textTheme.titleMedium,
              ),
            ),
            _legend(),
            const SizedBox(height: 8),
            SizedBox(
              height: 240,
              child: raw.isEmpty && skin.isEmpty && core.isEmpty
                  ? const Center(child: Text('暂无数据'))
                  : LineChart(
                      LineChartData(
                        minX: 0,
                        maxX: maxX,
                        minY: minY,
                        maxY: maxY,
                        clipData: const FlClipData.all(),
                        gridData: FlGridData(
                          show: true,
                          drawVerticalLine: false,
                          horizontalInterval: 0.5,
                          getDrawingHorizontalLine: (v) => FlLine(
                            color: Colors.grey.shade300,
                            strokeWidth: 1,
                          ),
                        ),
                        borderData: FlBorderData(
                          show: true,
                          border: Border.all(color: Colors.grey.shade400),
                        ),
                        titlesData: FlTitlesData(
                          topTitles: const AxisTitles(
                            sideTitles: SideTitles(showTitles: false),
                          ),
                          rightTitles: const AxisTitles(
                            sideTitles: SideTitles(showTitles: false),
                          ),
                          leftTitles: AxisTitles(
                            sideTitles: SideTitles(
                              showTitles: true,
                              reservedSize: 40,
                              interval: 0.5,
                              getTitlesWidget: (v, _) => Text(
                                v.toStringAsFixed(1),
                                style: const TextStyle(fontSize: 10),
                              ),
                            ),
                          ),
                          bottomTitles: AxisTitles(
                            sideTitles: SideTitles(
                              showTitles: true,
                              reservedSize: 22,
                              interval: maxX / 3,
                              getTitlesWidget: (v, _) {
                                final sec = (v / 1000).round();
                                final m = sec ~/ 60;
                                final s = sec % 60;
                                final label = '${m.toString()}:${s.toString().padLeft(2, "0")}';
                                return Text(
                                  label,
                                  style: const TextStyle(fontSize: 10),
                                );
                              },
                            ),
                          ),
                        ),
                        lineTouchData: LineTouchData(
                          touchTooltipData: LineTouchTooltipData(
                            getTooltipItems: (spots) => spots
                                .map(
                                  (s) => LineTooltipItem(
                                    '${s.y.toStringAsFixed(2)} °C',
                                    TextStyle(
                                      color: s.bar.color,
                                      fontWeight: FontWeight.w600,
                                      fontSize: 12,
                                    ),
                                  ),
                                )
                                .toList(),
                          ),
                        ),
                        lineBarsData: [
                          _bar(raw, _rawColor),
                          _bar(skin, _skinColor),
                          _bar(core, _coreColor),
                        ],
                      ),
                    ),
            ),
            if (samples.isNotEmpty)
              Padding(
                padding: const EdgeInsets.only(left: 8, top: 6),
                child: Text(
                  _latestLine(samples.last),
                  style: const TextStyle(fontSize: 12, color: Colors.black54),
                ),
              ),
          ],
        ),
      ),
    );
  }

  String _latestLine(TempSample s) {
    String f(double? v) => v == null ? '—' : '${v.toStringAsFixed(2)}°C';
    return '最新  原始 ${f(s.sensorRaw)}  ·  滤波 ${f(s.skinFiltered)}  ·  核心 ${f(s.core)}';
  }

  Widget _legend() {
    Widget item(Color c, String label) => Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Container(width: 14, height: 3, color: c),
            const SizedBox(width: 4),
            Text(label, style: const TextStyle(fontSize: 12)),
            const SizedBox(width: 12),
          ],
        );
    return Padding(
      padding: const EdgeInsets.only(left: 8),
      child: Row(
        children: [
          item(_rawColor, '原始温度'),
          item(_skinColor, '滤波温度'),
          item(_coreColor, '核心温度'),
        ],
      ),
    );
  }

  LineChartBarData _bar(List<FlSpot> spots, Color color) {
    return LineChartBarData(
      spots: spots,
      isCurved: false,
      color: color,
      barWidth: 1.6,
      isStrokeCapRound: true,
      dotData: const FlDotData(show: false),
      // Do not connect across gaps — spots already omit nulls; fl_chart
      // connects consecutive spots. Gaps appear as jumps in time only.
      belowBarData: BarAreaData(show: false),
    );
  }
}
