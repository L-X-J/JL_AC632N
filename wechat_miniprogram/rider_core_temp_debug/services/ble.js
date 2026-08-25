const PROTOCOL = require('../types/protocol');

function normalizeUuid(uuid) {
  const compact = String(uuid || '').replace(/-/g, '').toLowerCase();
  return compact.length === 4
    ? '0000' + compact + '00001000800000805f9b34fb'
    : compact;
}

function sameUuid(left, right) {
  return normalizeUuid(left) === normalizeUuid(right);
}

/** Small callback-based wrapper around the WeChat BLE central APIs. */
class RiderBle {
  constructor() {
    this.deviceId = '';
    this.deviceName = '';
    this.serviceId = '';
    this.dataCharacteristicId = '';
    this.scanning = false;
    this.connected = false;
    this.connecting = false;
    this.onDevice = null;
    this.onValue = null;
    this.onState = null;
    this.onError = null;
    this.deviceFoundBound = false;
    this.handleValueChange = this.handleValueChange.bind(this);
    this.handleConnectionState = this.handleConnectionState.bind(this);
    wx.onBLECharacteristicValueChange(this.handleValueChange);
    wx.onBLEConnectionStateChange(this.handleConnectionState);
  }

  emitState(state, detail) {
    if (this.onState) this.onState(state, detail || {});
  }

  fail(error) {
    const message = error && error.errMsg ? error.errMsg : String(error || 'BLE 操作失败');
    if (this.connecting || this.connected) {
      this.resetConnection();
    }
    if (this.onError) this.onError(message);
    this.emitState('error', { message });
  }

  /** Release a partially discovered link so the next attempt starts cleanly. */
  resetConnection() {
    const deviceId = this.deviceId;
    this.connecting = false;
    this.connected = false;
    this.deviceId = '';
    this.deviceName = '';
    this.serviceId = '';
    this.dataCharacteristicId = '';
    if (deviceId) {
      wx.closeBLEConnection({ deviceId });
    }
  }

  openAdapter(callback) {
    wx.openBluetoothAdapter({
      success: () => callback(),
      fail: (error) => this.fail(error),
    });
  }

  startDiscovery(onDevice) {
    this.onDevice = onDevice;
    this.openAdapter(() => {
      if (!this.deviceFoundBound) {
        wx.onBluetoothDeviceFound((result) => {
          const devices = result.devices || [];
          devices.forEach((device) => {
            if (device.name || device.localName) {
              this.onDevice && this.onDevice(device);
            }
          });
        });
        this.deviceFoundBound = true;
      }
      wx.startBluetoothDevicesDiscovery({
        allowDuplicatesKey: false,
        // Rider keeps 2110 out of the legacy 31-byte advertisement. Filter by
        // name in the page, then verify the service after connecting.
        success: () => {
          this.scanning = true;
          this.emitState('scanning');
        },
        fail: (error) => this.fail(error),
      });
    });
  }

  stopDiscovery() {
    if (!this.scanning) return;
    wx.stopBluetoothDevicesDiscovery({ complete: () => {
      this.scanning = false;
      this.emitState('idle');
    } });
  }

  connect(device) {
    if (this.connecting || this.connected) {
      this.resetConnection();
    }
    this.stopDiscovery();
    this.deviceId = device.deviceId;
    this.deviceName = device.name || device.localName || 'ICXL-RTemp';
    this.connecting = true;
    this.emitState('connecting', { name: this.deviceName });
    wx.createBLEConnection({
      deviceId: this.deviceId,
      timeout: 10000,
      success: () => this.discoverDebugService(),
      fail: (error) => this.fail(error),
    });
  }

  discoverDebugService() {
    wx.getBLEDeviceServices({
      deviceId: this.deviceId,
      success: (result) => {
        const service = (result.services || []).find((item) => sameUuid(item.uuid, PROTOCOL.serviceUuid));
        if (!service) {
          this.fail('未找到 Rider 调试服务 2110');
          return;
        }
        this.serviceId = service.uuid;
        wx.getBLEDeviceCharacteristics({
          deviceId: this.deviceId,
          serviceId: this.serviceId,
          success: (characteristics) => {
            const data = (characteristics.characteristics || []).find((item) => sameUuid(item.uuid, PROTOCOL.dataUuid));
            if (!data) {
              this.fail('未找到调试数据特征 2111');
              return;
            }
            this.dataCharacteristicId = data.uuid;
            const subscribe = () => wx.notifyBLECharacteristicValueChanged({
              deviceId: this.deviceId,
              serviceId: this.serviceId,
              characteristicId: this.dataCharacteristicId,
              state: true,
              success: () => {
                this.connected = true;
                this.connecting = false;
                this.emitState('connected', {
                  name: this.deviceName,
                  serviceId: this.serviceId,
                  characteristicId: this.dataCharacteristicId,
                });
              },
              fail: (error) => this.fail(error),
            });
            // A 41-byte notification needs an ATT payload above the default
            // 20-byte legacy limit. Older clients may lack setBLEMTU; the
            // fallback keeps discovery usable and surfaces send errors.
            if (typeof wx.setBLEMTU === 'function') {
              wx.setBLEMTU({
                deviceId: this.deviceId,
                mtu: 247,
                success: subscribe,
                fail: subscribe,
              });
            } else {
              subscribe();
            }
          },
          fail: (error) => this.fail(error),
        });
      },
      fail: (error) => this.fail(error),
    });
  }

  handleValueChange(result) {
    if (result.deviceId !== this.deviceId || result.characteristicId !== this.dataCharacteristicId) return;
    if (this.onValue) this.onValue(result.value);
  }

  handleConnectionState(result) {
    if (result.deviceId !== this.deviceId) return;
    this.connected = !!result.connected;
    if (!this.connected) {
      this.resetConnection();
      this.emitState('disconnected', { reason: '设备断开' });
    }
  }

  write(value, options) {
    const bytes = value instanceof Uint8Array ? value : new Uint8Array(value);
    return new Promise((resolve, reject) => {
      wx.writeBLECharacteristicValue({
        deviceId: this.deviceId,
        serviceId: (options && options.serviceId) || this.serviceId,
        characteristicId: (options && options.characteristicId) || this.dataCharacteristicId,
        value: bytes.buffer,
        writeType: (options && options.writeType) || 'writeNoResponse',
        success: resolve,
        fail: (error) => {
          this.fail(error);
          reject(error);
        },
      });
    });
  }

  close() {
    this.stopDiscovery();
    this.resetConnection();
    this.emitState('idle');
  }

  destroy() {
    this.close();
    if (typeof wx.offBLECharacteristicValueChange === 'function') {
      wx.offBLECharacteristicValueChange(this.handleValueChange);
    }
    if (typeof wx.offBLEConnectionStateChange === 'function') {
      wx.offBLEConnectionStateChange(this.handleConnectionState);
    }
  }
}

module.exports = { RiderBle, normalizeUuid, sameUuid };
