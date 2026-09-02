import 'dart:io';

import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';
import 'package:icxl_rtemp_companion/models/temp_sample.dart';

/// Local CSV export: timestamp_iso,sensor_raw,skin_filtered,core
class CsvExporter {
  static final DateFormat _fileStamp =
      DateFormat('yyyyMMdd_HHmmss');

  /// Writes CSV under app documents and opens the platform share sheet.
  Future<String> exportAndShare(List<TempSample> samples) async {
    final csv = buildCsv(samples);
    final dir = await getApplicationDocumentsDirectory();
    final name =
        'icxl_rtemp_${_fileStamp.format(DateTime.now())}.csv';
    final path = '${dir.path}/$name';
    final file = File(path);
    await file.writeAsString(csv, flush: true);

    await SharePlus.instance.share(
      ShareParams(
        files: [XFile(path, mimeType: 'text/csv', name: name)],
        subject: 'ICXL-RTemp CSV',
        text: 'ICXL-RTemp temperature export ($name)',
      ),
    );
    return path;
  }

  /// Pure CSV builder (testable offline).
  String buildCsv(List<TempSample> samples) {
    final buf = StringBuffer('timestamp_iso,sensor_raw,skin_filtered,core\n');
    for (final s in samples) {
      buf.writeln(
        '${s.timestamp.toUtc().toIso8601String()},'
        '${_fmt(s.sensorRaw)},'
        '${_fmt(s.skinFiltered)},'
        '${_fmt(s.core)}',
      );
    }
    return buf.toString();
  }

  String _fmt(double? v) => v == null ? '' : v.toStringAsFixed(2);
}
