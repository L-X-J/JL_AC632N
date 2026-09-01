const { RiderBle } = require('../../services/ble');
const { RiderRcspOta } = require('../../services/rcspOta');
const { decodeDebugSnapshot } = require('../../utils/debugCodec');
const { recordsToCsv } = require('../../utils/csv');

const STATUS_LABELS = {
  sensorStatus: ['正常', '无设备', 'CRC 错误', '范围错误', '未佩戴'],
  temperatureState: ['无设备', '未佩戴', '接触稳定中', '可信皮温', '疑似脱落', '数据过期'],
  coreState: ['空', '预热', '就绪', '保持', '无效'],
  quality: ['无效', '差', '一般', '良好', '优秀', '未知', '未知', '不适用'],
};

function label(list, value) {
  return list[value] || ('状态 ' + value);
}

Page({
  data: {
    status: 'idle',
    statusText: '未连接',
    devices: [],
    deviceName: '',
    connected: false,
    collecting: false,
    records: [],
    latest: null,
    rawHex: '',
    error: '',
    otaText: 'OTA 通道尚未检查',
    otaAvailable: false,
    shownRecordCount: 0,
  },

  onLoad() {
    this.ble = new RiderBle();
    this.ota = new RiderRcspOta(this.ble);
    this.ble.onDevice = (device) => this.addDevice(device);
    this.ble.onValue = (value) => this.onDebugValue(value);
    this.ble.onState = (state, detail) => this.onBleState(state, detail);
    this.ble.onError = (message) => this.setData({ error: message });
  },

  onUnload() {
    if (this.ble) this.ble.destroy();
  },

  addDevice(device) {
    const name = device.name || device.localName || '';
    if (!name || (name !== 'ICXL-RTemp' && name.indexOf('RTemp') < 0)) return;
    const devices = this.data.devices.slice();
    const index = devices.findIndex((item) => item.deviceId === device.deviceId);
    const normalized = Object.assign({}, device, { displayName: name });
    if (index >= 0) devices[index] = normalized;
    else devices.push(normalized);
    this.setData({ devices });
  },

  scan() {
    this.setData({ error: '', devices: [], statusText: '扫描中...' });
    this.ble.startDiscovery((device) => this.addDevice(device));
  },

  stopScan() {
    this.ble.stopDiscovery();
    this.setData({ statusText: this.data.connected ? '已连接' : '未连接' });
  },

  connect(event) {
    const index = Number(event.currentTarget.dataset.index);
    const device = this.data.devices[index];
    if (device) this.ble.connect(device);
  },

  disconnect() {
    this.ble.close();
    this.setData({ connected: false, collecting: false, status: 'idle', statusText: '未连接' });
  },

  onBleState(state, detail) {
    const updates = { status: state, error: '' };
    if (state === 'connected') {
      updates.connected = true;
      updates.deviceName = detail.name;
      updates.statusText = '已连接，已订阅 200 ms 数据';
    } else if (state === 'connecting') {
      updates.statusText = '连接中...';
    } else if (state === 'scanning') {
      updates.statusText = '扫描中...';
    } else if (state === 'disconnected' || state === 'idle') {
      updates.connected = false;
      updates.collecting = false;
      updates.otaAvailable = false;
      updates.otaText = 'OTA 通道尚未检查';
      updates.statusText = '未连接';
    } else if (state === 'error') {
      updates.connected = false;
      updates.collecting = false;
      updates.otaAvailable = false;
      updates.statusText = 'BLE 错误';
    }
    this.setData(updates);
  },

  onDebugValue(value) {
    try {
      const record = decodeDebugSnapshot(value);
      record.sensorLabel = record.sensorC === null ? '--' : record.sensorC.toFixed(2) + ' °C';
      record.contactLabel = record.contactC === null ? '--' : record.contactC.toFixed(2) + ' °C';
      record.skinLabel = record.skinC === null ? '--' : record.skinC.toFixed(2) + ' °C';
      record.coreCandidateLabel = record.coreCandidateC === null ? '--' : record.coreCandidateC.toFixed(2) + ' °C';
      record.publishedCoreLabel = record.publishedCoreC === null ? '--' : record.publishedCoreC.toFixed(2) + ' °C';
      record.qualityLabel = label(STATUS_LABELS.quality, record.quality);
      record.temperatureStateLabel = label(STATUS_LABELS.temperatureState, record.temperatureState);
      record.coreStateLabel = label(STATUS_LABELS.coreState, record.coreState);
      const records = this.data.collecting ? this.data.records.concat(record) : this.data.records;
      const retained = records.length > 5000 ? records.slice(-5000) : records;
      this.setData({ latest: record, rawHex: record.rawHex, records: retained, shownRecordCount: retained.length });
    } catch (error) {
      this.setData({ error: error.message || String(error) });
    }
  },

  toggleCollect() {
    this.setData({ collecting: !this.data.collecting });
  },

  clearRecords() {
    this.setData({ records: [], shownRecordCount: 0 });
  },

  exportCsv() {
    if (!this.data.records.length) {
      this.setData({ error: '没有可导出的记录' });
      return;
    }
    const csv = recordsToCsv(this.data.records);
    const filePath = `${wx.env.USER_DATA_PATH}/coretemp-${Date.now()}.csv`;
    const fs = wx.getFileSystemManager();
    fs.writeFile({
      filePath,
      data: '\uFEFF' + csv,
      encoding: 'utf8',
      success: () => wx.openDocument({
        filePath,
        fileType: 'csv',
        showMenu: true,
        fail: () => this.setData({ error: '文件已保存：' + filePath }),
      }),
      fail: (error) => this.setData({ error: error.errMsg || 'CSV 写入失败' }),
    });
  },

  probeOta() {
    this.setData({ otaText: '检查 RCSP 通道...' });
    this.ota.probe().then((available) => {
      this.setData({ otaAvailable: available, otaText: this.ota.reason });
    }).catch((error) => this.setData({ otaText: error.message || String(error) }));
  },

  chooseFirmware() {
    if (!this.data.otaAvailable) {
      this.setData({ error: this.ota.reason });
      return;
    }
    wx.chooseMessageFile({
      count: 1,
      type: 'file',
      extension: ['ufw', 'bin'],
      success: () => this.setData({ error: '文件已选择；认证 OTA 流程尚未开启，未发送固件' }),
      fail: (error) => this.setData({ error: error.errMsg || '未选择固件' }),
    });
  },
});
