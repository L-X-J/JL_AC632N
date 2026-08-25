/** UUIDs and byte-layout constants shared by the BLE and view layers. */
const PROTOCOL = {
  serviceUuid: '00002110-5B1E-4347-B07C-97B514DAE121',
  dataUuid: '00002111-5B1E-4347-B07C-97B514DAE121',
  coreServiceUuid: '00002100-5B1E-4347-B07C-97B514DAE121',
  debugFrameVersion: 1,
  debugFrameSize: 41,
  temperatureUnavailable: 0x7fff,
  flags: {
    sensorValid: 0x01,
    contactValid: 0x02,
    skinValid: 0x04,
    coreEstimate: 0x08,
    publishedCore: 0x10,
    heartRateValid: 0x20,
    coreVerified: 0x40,
    dataStale: 0x80,
  },
};

module.exports = PROTOCOL;
