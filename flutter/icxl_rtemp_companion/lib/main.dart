import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/theme/app_theme.dart';
import 'package:icxl_rtemp_companion/ui/scan_page.dart';

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  runApp(const IcxlRtempApp());
}

class IcxlRtempApp extends StatefulWidget {
  const IcxlRtempApp({super.key});

  @override
  State<IcxlRtempApp> createState() => _IcxlRtempAppState();
}

class _IcxlRtempAppState extends State<IcxlRtempApp> {
  late final BleController _controller;

  @override
  void initState() {
    super.initState();
    _controller = BleController();
  }

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ICXL-RTemp',
      debugShowCheckedModeBanner: false,
      theme: buildAppTheme(Brightness.light),
      darkTheme: buildAppTheme(Brightness.dark),
      themeMode: ThemeMode.system,
      home: ScanPage(controller: _controller),
    );
  }
}
