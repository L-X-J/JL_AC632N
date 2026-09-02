import 'package:flutter_test/flutter_test.dart';
import 'package:icxl_rtemp_companion/export/csv_exporter.dart';
import 'package:icxl_rtemp_companion/models/temp_sample.dart';

void main() {
  test('CSV header and null formatting', () {
    final csv = CsvExporter().buildCsv([
      TempSample(
        timestamp: DateTime.utc(2026, 9, 2, 6, 0, 0),
        sensorRaw: 36.5,
        skinFiltered: null,
        core: 37.0,
      ),
    ]);
    expect(
      csv.startsWith('timestamp_iso,sensor_raw,skin_filtered,core\n'),
      isTrue,
    );
    expect(csv.contains('2026-09-02T06:00:00.000Z,36.50,,37.00'), isTrue);
  });
}
