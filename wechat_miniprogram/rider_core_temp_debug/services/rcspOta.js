const { sameUuid } = require('./ble');

const RCSP = {
  serviceUuid: 'AE00',
  writeUuid: 'AE01',
  notifyUuid: 'AE02',
  startTag: [0xfe, 0xdc, 0xba],
  endTag: 0xef,
  opcodes: {
    getFileInfoOffset: 0xe1,
    inquireCanUpdate: 0xe2,
    enterUpdateMode: 0xe3,
    exitUpdateMode: 0xe4,
    sendFirmwareBlock: 0xe5,
    getRefreshStatus: 0xe6,
    reboot: 0xe7,
  },
};

function u16le(value) {
  return [value & 0xff, (value >>> 8) & 0xff];
}

function buildPacket(opcode, payload, requestResponse) {
  const body = payload || new Uint8Array(0);
  const head = (opcode & 0xff) | ((requestResponse ? 1 : 0) << 14);
  const packet = new Uint8Array(3 + 2 + 2 + body.length + 1);
  packet.set(RCSP.startTag, 0);
  packet[3] = head & 0xff;
  packet[4] = (head >>> 8) & 0xff;
  packet.set(u16le(body.length), 5);
  packet.set(body, 7);
  packet[packet.length - 1] = RCSP.endTag;
  return packet;
}

function chunk(bytes, size) {
  const chunks = [];
  for (let offset = 0; offset < bytes.length; offset += size) {
    chunks.push(bytes.slice(offset, Math.min(offset + size, bytes.length)));
  }
  return chunks;
}

/**
 * RCSP transport adapter. The Rider board profile exposes the RCSP service,
 * but this adapter only probes the channel until its OTA state machine is
 * separately certified.
 */
class RiderRcspOta {
  constructor(ble) {
    this.ble = ble;
    this.serviceId = '';
    this.writeCharacteristicId = '';
    this.notifyCharacteristicId = '';
    this.available = false;
    this.reason = '尚未检查 RCSP OTA 通道';
    this.onProgress = null;
    this.onResponse = null;
  }

  probe() {
    return new Promise((resolve, reject) => {
      if (!this.ble || !this.ble.deviceId) {
        reject(new Error('请先连接 Rider 设备'));
        return;
      }
      wx.getBLEDeviceServices({
        deviceId: this.ble.deviceId,
        success: (serviceResult) => {
          const service = (serviceResult.services || []).find((item) => sameUuid(item.uuid, RCSP.serviceUuid));
          if (!service) {
            this.available = false;
            this.reason = '未发现 RCSP AE00 服务（请确认已刷入 CONFIG_APP_OTA_ENABLE=1 的 Rider 固件）';
            resolve(false);
            return;
          }
          this.serviceId = service.uuid;
          wx.getBLEDeviceCharacteristics({
            deviceId: this.ble.deviceId,
            serviceId: this.serviceId,
            success: (characteristicResult) => {
              const characteristics = characteristicResult.characteristics || [];
              const write = characteristics.find((item) => sameUuid(item.uuid, RCSP.writeUuid));
              const notify = characteristics.find((item) => sameUuid(item.uuid, RCSP.notifyUuid));
              if (!write || !notify) {
                this.reason = 'RCSP 特征不完整，未进入升级模式';
                resolve(false);
                return;
              }
              this.writeCharacteristicId = write.uuid;
              this.notifyCharacteristicId = notify.uuid;
              wx.notifyBLECharacteristicValueChanged({
                deviceId: this.ble.deviceId,
                serviceId: this.serviceId,
                characteristicId: this.notifyCharacteristicId,
                state: true,
                success: () => {
                  this.available = true;
                  this.reason = '已发现 RCSP 通道；当前小程序传输状态机尚未认证';
                  resolve(true);
                },
                fail: (error) => reject(error),
              });
            },
            fail: reject,
          });
        },
        fail: reject,
      });
    });
  }

  /** Keep the transport write path closed until the certified state machine exists. */
  async sendPacket() {
    throw new Error('OTA 传输尚未认证：原始 RCSP 包不会发送');
  }

  /**
   * Keep raw transfer opt-in. A valid firmware image alone is insufficient:
   * RCSP requires device authentication, file metadata and command ACKs.
   */
  async upload() {
    throw new Error('OTA 传输尚未认证：当前小程序尚未实现 RCSP 认证、文件信息、ACK 和重启流程');
  }
}

module.exports = { RCSP, RiderRcspOta, buildPacket, chunk };
