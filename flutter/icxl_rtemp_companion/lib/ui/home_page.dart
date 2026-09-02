import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/ble/ble_controller.dart';
import 'package:icxl_rtemp_companion/ui/scan_page.dart';

/// Compatibility entry — navigation is now Scan → Device (no multi-tab).
class HomePage extends StatelessWidget {
  const HomePage({super.key, required this.controller});

  final BleController controller;

  @override
  Widget build(BuildContext context) => ScanPage(controller: controller);
}
