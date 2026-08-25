const COLUMNS = [
  ['received_at', '接收时间'],
  ['sequence', '序号'],
  ['sensorC', '传感器温度_C'],
  ['contactC', '接触温度_C'],
  ['skinC', '皮温_C'],
  ['coreCandidateC', '核心候选_C'],
  ['publishedCoreC', '发布核心_C'],
  ['slopeCPerMin', '斜率_C每分钟'],
  ['skinBaselineC', '皮温基线_C'],
  ['skinDelta1mC', '皮温变化1分钟_C'],
  ['skinDelta5mC', '皮温变化5分钟_C'],
  ['heartRateDelta1m', '心率变化1分钟'],
  ['coreHistorySeconds', '核心历史秒数'],
  ['contactSamples', '接触样本数'],
  ['typicalSamples', '典型样本数'],
  ['heartRate', '心率'],
  ['quality', '质量'],
  ['sensorStatus', '传感器状态'],
  ['temperatureState', '皮温状态'],
  ['coreState', '核心状态'],
  ['freshness', '新鲜度'],
  ['confidence', '置信度'],
  ['modelMode', '模型模式'],
  ['modelVersion', '模型版本'],
  ['heartRateUsed', '使用心率'],
  ['rawHex', '原始十六进制'],
];

function escapeCsv(value) {
  if (value === null || value === undefined) return '';
  const text = String(value);
  return /[",\n]/.test(text) ? '"' + text.replace(/"/g, '""') + '"' : text;
}

function formatTime(timestamp) {
  const date = new Date(timestamp);
  const pad = (value) => (value < 10 ? '0' : '') + value;
  return date.getFullYear() + '-' + pad(date.getMonth() + 1) + '-' +
    pad(date.getDate()) + ' ' + pad(date.getHours()) + ':' +
    pad(date.getMinutes()) + ':' + pad(date.getSeconds()) + '.' +
    String(date.getMilliseconds()).padStart(3, '0');
}

function recordsToCsv(records) {
  const header = COLUMNS.map((column) => escapeCsv(column[1])).join(',');
  const rows = records.map((record) => COLUMNS.map((column) => {
    const value = column[0] === 'received_at' ? formatTime(record.receivedAt) : record[column[0]];
    return escapeCsv(value);
  }).join(','));
  return [header].concat(rows).join('\n') + '\n';
}

module.exports = { recordsToCsv };
