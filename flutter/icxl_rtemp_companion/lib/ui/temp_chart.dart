import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/models/temp_sample.dart';
import 'package:icxl_rtemp_companion/theme/app_theme.dart';

/// Rolling last-5-minutes temperature chart.
/// Null / 0x7FFF → gap (FlSpot.nullSpot, no line through) + reading 「—」.
class TempChart extends StatelessWidget {
  const TempChart({super.key, required this.samples});

  final List<TempSample> samples;

  // Soft semantics + line-style differentiation (not neon-only).
  static const _rawColor = Color(0xFF8E8E93); // grey, thin solid
  static const _skinColor = Color(0xFF007AFF); // system blue, medium solid
  static const _coreColor = Color(0xFF34C759); // soft green, thicker / dashed

  @override
  Widget build(BuildContext context) {
    final theme = Theme.of(context);
    final window = BleController.chartWindow;
    final now = samples.isEmpty ? DateTime.now() : samples.last.timestamp;
    final origin = now.subtract(window);
    final maxX = window.inMilliseconds.toDouble();

    /// Include null spots so fl_chart breaks the stroke across gaps.
    List<FlSpot> spots(double? Function(TempSample) pick) {
      final out = <FlSpot>[];
      for (final s in samples) {
        final x = s.timestamp.difference(origin).inMilliseconds.toDouble();
        if (x < 0) continue;
        final v = pick(s);
        if (v == null) {
          out.add(FlSpot.nullSpot);
        } else {
          out.add(FlSpot(x, v));
        }
      }
      return out;
    }

    final raw = spots((s) => s.sensorRaw);
    final skin = spots((s) => s.skinFiltered);
    final core = spots((s) => s.core);

    var minY = 35.0;
    var maxY = 38.0;
    var haveY = false;
    void consider(List<FlSpot> list) {
      for (final p in list) {
        if (p.isNull()) continue;
        if (!haveY) {
          minY = p.y;
          maxY = p.y;
          haveY = true;
        } else {
          if (p.y < minY) minY = p.y;
          if (p.y > maxY) maxY = p.y;
        }
      }
    }

    consider(raw);
    consider(skin);
    consider(core);
    final pad = ((maxY - minY) * 0.15).clamp(0.2, 1.0);
    minY = (minY - pad).clamp(30.0, 45.0);
    maxY = (maxY + pad).clamp(30.0, 45.0);

    final hasData = samples.any((s) => s.hasAnyTemp);

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Padding(
          padding: const EdgeInsets.only(left: 4, bottom: 8),
          child: Text('温度', style: theme.textTheme.titleMedium),
        ),
        Container(
          decoration: BoxDecoration(
            color: cardBackground(context),
            borderRadius: BorderRadius.circular(AppTokens.cardRadius),
          ),
          padding: const EdgeInsets.fromLTRB(8, 12, 12, 12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              _legend(context),
              const SizedBox(height: 8),
              SizedBox(
                height: 240,
                child: !hasData
                    ? Center(
                        child: Text(
                          '暂无数据',
                          style: theme.textTheme.bodyMedium,
                        ),
                      )
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
                              color: Theme.of(context).dividerColor,
                              strokeWidth: 1,
                            ),
                          ),
                          borderData: FlBorderData(show: false),
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
                                  style: TextStyle(
                                    fontSize: AppTokens.footnoteSize,
                                    color: secondaryLabel(context),
                                  ),
                                ),
                              ),
                            ),
                            bottomTitles: AxisTitles(
                              sideTitles: SideTitles(
                                showTitles: true,
                                reservedSize: 22,
                                interval: maxX / 5,
                                getTitlesWidget: (v, _) {
                                  final sec = (v / 1000).round();
                                  final m = sec ~/ 60;
                                  final s = sec % 60;
                                  final label =
                                      '${m.toString()}:${s.toString().padLeft(2, "0")}';
                                  return Text(
                                    label,
                                    style: TextStyle(
                                      fontSize: AppTokens.footnoteSize,
                                      color: secondaryLabel(context),
                                    ),
                                  );
                                },
                              ),
                            ),
                          ),
                          lineTouchData: LineTouchData(
                            touchTooltipData: LineTouchTooltipData(
                              getTooltipItems: (touched) => touched
                                  .map(
                                    (s) => LineTooltipItem(
                                      '${s.y.toStringAsFixed(2)} ℃',
                                      TextStyle(
                                        color: s.bar.color,
                                        fontWeight: FontWeight.w600,
                                        fontSize: AppTokens.footnoteSize,
                                      ),
                                    ),
                                  )
                                  .toList(),
                            ),
                          ),
                          lineBarsData: [
                            _bar(
                              raw,
                              _rawColor,
                              width: 1.0,
                              dashed: false,
                            ),
                            _bar(
                              skin,
                              _skinColor,
                              width: 2.0,
                              dashed: false,
                            ),
                            _bar(
                              core,
                              _coreColor,
                              width: 2.5,
                              dashed: true,
                            ),
                          ],
                        ),
                      ),
              ),
              if (samples.isNotEmpty)
                Padding(
                  padding: const EdgeInsets.only(left: 4, top: 8),
                  child: Text(
                    _latestLine(samples.last),
                    style: theme.textTheme.bodySmall,
                  ),
                ),
            ],
          ),
        ),
      ],
    );
  }

  String _latestLine(TempSample s) {
    String f(double? v) => v == null ? '—' : '${v.toStringAsFixed(2)}℃';
    return '最新  原始传感器 ${f(s.sensorRaw)}  ·  滤波皮温 ${f(s.skinFiltered)}  ·  核心 ${f(s.core)}';
  }

  Widget _legend(BuildContext context) {
    Widget item({
      required Color color,
      required String label,
      required double width,
      required bool dashed,
    }) {
      return Padding(
        padding: const EdgeInsets.only(right: 12),
        child: Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            CustomPaint(
              size: Size(18, width + 2),
              painter: _LegendLinePainter(
                color: color,
                strokeWidth: width,
                dashed: dashed,
              ),
            ),
            const SizedBox(width: 4),
            Text(
              label,
              style: TextStyle(
                fontSize: AppTokens.footnoteSize,
                color: secondaryLabel(context),
              ),
            ),
          ],
        ),
      );
    }

    return Padding(
      padding: const EdgeInsets.only(left: 4),
      child: Wrap(
        children: [
          item(
            color: _rawColor,
            label: '原始传感器',
            width: 1.0,
            dashed: false,
          ),
          item(
            color: _skinColor,
            label: '滤波皮温',
            width: 2.0,
            dashed: false,
          ),
          item(
            color: _coreColor,
            label: '核心',
            width: 2.5,
            dashed: true,
          ),
        ],
      ),
    );
  }

  LineChartBarData _bar(
    List<FlSpot> spots,
    Color color, {
    required double width,
    required bool dashed,
  }) {
    return LineChartBarData(
      spots: spots,
      isCurved: false,
      color: color,
      barWidth: width,
      isStrokeCapRound: true,
      dotData: const FlDotData(show: false),
      dashArray: dashed ? <int>[6, 4] : null,
      belowBarData: BarAreaData(show: false),
    );
  }
}

class _LegendLinePainter extends CustomPainter {
  _LegendLinePainter({
    required this.color,
    required this.strokeWidth,
    required this.dashed,
  });

  final Color color;
  final double strokeWidth;
  final bool dashed;

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = strokeWidth
      ..style = PaintingStyle.stroke
      ..strokeCap = StrokeCap.round;
    final y = size.height / 2;
    if (!dashed) {
      canvas.drawLine(Offset(0, y), Offset(size.width, y), paint);
      return;
    }
    const dash = 3.0;
    const gap = 2.0;
    var x = 0.0;
    while (x < size.width) {
      final x2 = (x + dash).clamp(0.0, size.width);
      canvas.drawLine(Offset(x, y), Offset(x2, y), paint);
      x += dash + gap;
    }
  }

  @override
  bool shouldRepaint(covariant _LegendLinePainter old) =>
      old.color != color ||
      old.strokeWidth != strokeWidth ||
      old.dashed != dashed;
}
