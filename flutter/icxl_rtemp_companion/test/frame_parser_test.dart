import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:icxl_rtemp_companion/ble/frame_parser.dart';
import 'package:icxl_rtemp_companion/ble/uuids.dart';

void main() {
  const parser = FrameParser();

  group('FrameParser', () {
    test('parses header/temps/tail at firmware offsets', () {
      final frame = FrameParser.buildMockFrame(
        sensorRawC: 36.50,
        skinFilteredC: 36.20,
        coreC: 37.00,
        protocolVersion: 2,
        flags: 0x11,
        sequence: 0x01020304,
        quality: 0x55,
        sensorStatus: 0x22,
        temperatureState: 0x33,
        coreState: 0x44,
      );
      expect(frame.length, kFrameLength);

      final bd = ByteData.sublistView(Uint8List.fromList(frame));
      expect(frame[0], 2);
      expect(frame[1], 0x11);
      expect(bd.getUint32(2, Endian.little), 0x01020304);
      expect(frame[32], 0x55);
      expect(frame[33], 0x22);
      expect(frame[34], 0x33);
      expect(frame[35], 0x44);

      final sample = parser.parse(frame, timestamp: DateTime.utc(2026, 1, 1));
      expect(sample, isNotNull);
      expect(sample!.protocolVersion, 2);
      expect(sample.flags, 0x11);
      expect(sample.sequence, 0x01020304);
      expect(sample.sensorRaw, closeTo(36.50, 0.005));
      expect(sample.skinFiltered, closeTo(36.20, 0.005));
      expect(sample.core, closeTo(37.00, 0.005));
      expect(sample.quality, 0x55);
      expect(sample.sensorStatus, 0x22);
      expect(sample.temperatureState, 0x33);
      expect(sample.coreState, 0x44);
      expect(sample.timestamp, DateTime.utc(2026, 1, 1));
    });

    test('0x7FFF sentinel becomes null (int16 32767)', () {
      final frame = FrameParser.buildMockFrame(
        sensorRawC: 36.5,
        skinFilteredC: 36.2,
        coreC: 37.0,
        invalidateSensor: true,
        invalidateSkin: true,
        invalidateCore: true,
      );

      final bd = ByteData.sublistView(Uint8List.fromList(frame));
      expect(bd.getInt16(FrameParser.offsetSensorRaw, Endian.little), 0x7FFF);
      expect(bd.getInt16(FrameParser.offsetSensorRaw, Endian.little), 32767);

      final sample = parser.parse(frame);
      expect(sample, isNotNull);
      expect(sample!.sensorRaw, isNull);
      expect(sample.skinFiltered, isNull);
      expect(sample.core, isNull);
      expect(sample.hasAnyTemp, isFalse);
    });

    test('mixed sentinel: only invalidated channels are null', () {
      final frame = FrameParser.buildMockFrame(
        sensorRawC: 35.00,
        skinFilteredC: 35.50,
        coreC: 36.00,
        invalidateSensor: false,
        invalidateSkin: true,
        invalidateCore: false,
      );
      final sample = parser.parse(frame)!;
      expect(sample.sensorRaw, closeTo(35.00, 0.005));
      expect(sample.skinFiltered, isNull);
      expect(sample.core, closeTo(36.00, 0.005));
      expect(sample.hasAnyTemp, isTrue);
    });

    test('short frame returns null', () {
      expect(parser.parse(<int>[]), isNull);
      expect(parser.parse(List<int>.filled(40, 0)), isNull);
      expect(parser.parse(List<int>.filled(20, 0xFF)), isNull);
    });

    test('negative temperature (-0.01°C) is not treated as sentinel', () {
      final data = Uint8List(kFrameLength);
      final bd = ByteData.sublistView(data);
      bd.setInt16(FrameParser.offsetSensorRaw, -1, Endian.little);
      bd.setInt16(FrameParser.offsetSkinFiltered, -100, Endian.little);
      bd.setInt16(FrameParser.offsetPublishedCore, 3700, Endian.little);

      final sample = parser.parse(data)!;
      expect(sample.sensorRaw, closeTo(-0.01, 0.0001));
      expect(sample.skinFiltered, closeTo(-1.00, 0.0001));
      expect(sample.core, closeTo(37.00, 0.0001));
    });
  });
}
