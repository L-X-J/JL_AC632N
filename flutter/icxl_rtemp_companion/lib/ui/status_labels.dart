/// Short Chinese labels for firmware status bytes.
///
/// Indexed by uint8 value; unknown codes fall back to 「状态 N」.
/// Source of truth aligned with WeChat rider_core_temp_debug STATUS_LABELS
/// and firmware enums (RIDER_TEMP_STATUS_*, RIDER_TEMP_STATE_*, RIDER_CORE_STATE_*).
abstract final class StatusLabels {
  static const List<String> sensorStatus = [
    '正常', // 0 RIDER_TEMP_STATUS_OK
    '无设备', // 1
    'CRC错误', // 2
    '范围错误', // 3
    '未佩戴', // 4
  ];

  static const List<String> temperatureState = [
    '无设备', // 0
    '未佩戴', // 1
    '接触稳定中', // 2
    '可信皮温', // 3
    '疑似脱落', // 4
    '数据过期', // 5
  ];

  static const List<String> coreState = [
    '空', // 0 RIDER_CORE_STATE_EMPTY
    '预热', // 1
    '就绪', // 2
    '保持', // 3
    '无效', // 4
  ];

  static String label(List<String> table, int? value) {
    if (value == null) return '—';
    if (value >= 0 && value < table.length) return table[value];
    return '状态$value';
  }

  static String sensor(int? v) => label(sensorStatus, v);
  static String temperature(int? v) => label(temperatureState, v);
  static String core(int? v) => label(coreState, v);
}
