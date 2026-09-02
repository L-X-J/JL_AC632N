import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/theme/app_theme.dart';
import 'package:icxl_rtemp_companion/ui/scan_page.dart';
import 'package:icxl_rtemp_companion/ui/status_labels.dart';
import 'package:icxl_rtemp_companion/ui/status_panel.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';

void main() {
  testWidgets('ScanPage shows HIG copy', (tester) async {
    final controller = BleController();
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      MaterialApp(
        theme: buildAppTheme(Brightness.light),
        home: ScanPage(controller: controller),
      ),
    );

    expect(find.text('设备'), findsOneWidget);
    expect(find.text('附近的 ICXL-RTemp'), findsOneWidget);
    expect(find.text('未发现设备'), findsOneWidget);
    expect(find.text('重新扫描'), findsOneWidget);
    expect(find.text('扫描'), findsOneWidget);
    expect(find.text('模拟'), findsOneWidget);
  });

  testWidgets('StatusPanel shows Chinese status rows', (tester) async {
    await tester.pumpWidget(
      MaterialApp(
        theme: buildAppTheme(Brightness.light),
        home: Scaffold(
          body: StatusPanel(
            status: const DeviceStatus(
              connection: ConnectionStateUi.connected,
              deviceName: 'ICXL-RTemp',
              batteryPercent: 80,
              sensorStatus: 0,
              temperatureState: 3,
              coreState: 2,
            ),
          ),
        ),
      ),
    );

    expect(find.text('连接'), findsOneWidget);
    expect(find.text('已连接'), findsOneWidget);
    expect(find.text('传感器'), findsOneWidget);
    expect(find.text('正常'), findsOneWidget);
    expect(find.text('温度状态'), findsOneWidget);
    expect(find.text('可信皮温'), findsOneWidget);
    expect(find.text('核心状态'), findsOneWidget);
    expect(find.text('就绪'), findsOneWidget);
    expect(find.text('电量'), findsOneWidget);
    expect(find.text('80%'), findsOneWidget);
    expect(find.text('按键'), findsOneWidget);
    expect(find.text('未上报'), findsOneWidget);
  });

  test('StatusLabels maps known firmware codes', () {
    expect(StatusLabels.sensor(0), '正常');
    expect(StatusLabels.sensor(4), '未佩戴');
    expect(StatusLabels.temperature(3), '可信皮温');
    expect(StatusLabels.core(2), '就绪');
    expect(StatusLabels.sensor(99), '状态99');
    expect(StatusLabels.sensor(null), '—');
  });
}
