import 'package:flutter/material.dart';
import 'package:icxl_rtemp_companion/ble/ble_service.dart';
import 'package:icxl_rtemp_companion/ble/uuids.dart';
import 'package:icxl_rtemp_companion/models/device_status.dart';

class ScanPage extends StatelessWidget {
  const ScanPage({super.key, required this.ble});

  final BleService ble;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: ble,
      builder: (context, _) {
        final scanning =
            ble.status.connection == ConnectionStateUi.scanning;
        return Scaffold(
          appBar: AppBar(
            title: const Text('扫描 ICXL-RTemp'),
            actions: [
              if (scanning)
                const Padding(
                  padding: EdgeInsets.all(16),
                  child: SizedBox(
                    width: 20,
                    height: 20,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  ),
                ),
            ],
          ),
          body: Column(
            children: [
              Padding(
                padding: const EdgeInsets.all(16),
                child: Text(
                  '按广播名 “$kDeviceAdvertisedName” 与服务/特征 UUID 匹配，'
                  '不依赖 handle 序号。',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ),
              Expanded(
                child: ble.scanResults.isEmpty
                    ? Center(
                        child: Text(
                          scanning ? '正在扫描…' : '未发现设备，请点击下方扫描',
                        ),
                      )
                    : ListView.builder(
                        itemCount: ble.scanResults.length,
                        itemBuilder: (context, i) {
                          final r = ble.scanResults[i];
                          final name = r.advertisementData.advName.isNotEmpty
                              ? r.advertisementData.advName
                              : (r.device.platformName.isNotEmpty
                                  ? r.device.platformName
                                  : 'Unknown');
                          final isTarget = name == kDeviceAdvertisedName;
                          return ListTile(
                            leading: Icon(
                              Icons.thermostat,
                              color: isTarget ? Colors.red : Colors.grey,
                            ),
                            title: Text(name),
                            subtitle: Text(
                              '${r.device.remoteId.str}  ·  RSSI ${r.rssi}',
                            ),
                            trailing: isTarget
                                ? const Chip(label: Text('目标'))
                                : null,
                            onTap: () async {
                              await ble.connect(r.device);
                              if (context.mounted) {
                                Navigator.of(context).pop();
                              }
                            },
                          );
                        },
                      ),
              ),
            ],
          ),
          floatingActionButton: FloatingActionButton.extended(
            onPressed: scanning
                ? () => ble.stopScan()
                : () => ble.startScan(),
            icon: Icon(scanning ? Icons.stop : Icons.bluetooth_searching),
            label: Text(scanning ? '停止' : '扫描'),
          ),
        );
      },
    );
  }
}
