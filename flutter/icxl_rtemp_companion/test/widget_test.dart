import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/ui/home_page.dart';

void main() {
  testWidgets('HomePage shows Chinese connection controls', (tester) async {
    final controller = BleController();
    addTearDown(controller.dispose);

    await tester.pumpWidget(
      MaterialApp(home: HomePage(controller: controller)),
    );

    expect(find.text('ICXL 核心体温伴侣'), findsOneWidget);
    expect(find.text('模拟模式'), findsOneWidget);
    expect(find.text('扫描 ICXL-RTemp'), findsOneWidget);
    expect(find.text('状态面板'), findsOneWidget);
    expect(find.text('按键'), findsOneWidget);
    expect(find.text('未上报'), findsOneWidget);
  });
}
