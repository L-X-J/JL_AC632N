const PROTOCOL = require('../types/protocol');

function asBytes(input) {
  if (input instanceof ArrayBuffer) return new Uint8Array(input);
  if (input && input.buffer instanceof ArrayBuffer) {
    return new Uint8Array(input.buffer, input.byteOffset || 0, input.byteLength);
  }
  throw new TypeError('调试帧不是 ArrayBuffer');
}

function readU16(bytes, offset) {
  return bytes[offset] | (bytes[offset + 1] << 8);
}

function readI16(bytes, offset) {
  const value = readU16(bytes, offset);
  return value & 0x8000 ? value - 0x10000 : value;
}

function readU32(bytes, offset) {
  return (bytes[offset] |
    (bytes[offset + 1] << 8) |
    (bytes[offset + 2] << 16) |
    (bytes[offset + 3] << 24)) >>> 0;
}

function temperature(bytes, offset, valid) {
  const value = readI16(bytes, offset);
  return valid && value !== PROTOCOL.temperatureUnavailable ? value / 100 : null;
}

function optionalI16(bytes, offset) {
  const value = readI16(bytes, offset);
  return value === PROTOCOL.temperatureUnavailable ? null : value / 100;
}

function hex(bytes) {
  let result = '';
  for (let i = 0; i < bytes.length; i += 1) {
    result += (bytes[i] < 16 ? '0' : '') + bytes[i].toString(16);
  }
  return result.toUpperCase();
}

/** Decode the fixed 41-byte Rider debug snapshot without mutating the input. */
function decodeDebugSnapshot(input) {
  const bytes = asBytes(input);
  if (bytes.length !== PROTOCOL.debugFrameSize) {
    throw new Error('调试帧长度错误: ' + bytes.length + '，期望 ' + PROTOCOL.debugFrameSize);
  }
  const version = bytes[0];
  if (version !== PROTOCOL.debugFrameVersion) {
    throw new Error('不支持的调试帧版本: ' + version);
  }
  const flags = bytes[1];
  const has = (flag) => (flags & flag) !== 0;
  return {
    version,
    flags,
    sequence: readU32(bytes, 2),
    sensorC: temperature(bytes, 6, has(PROTOCOL.flags.sensorValid)),
    contactC: temperature(bytes, 8, has(PROTOCOL.flags.contactValid)),
    skinC: temperature(bytes, 10, has(PROTOCOL.flags.skinValid)),
    coreCandidateC: temperature(bytes, 12, has(PROTOCOL.flags.coreEstimate)),
    publishedCoreC: temperature(bytes, 14, has(PROTOCOL.flags.publishedCore)),
    slopeCPerMin: optionalI16(bytes, 16),
    skinBaselineC: temperature(bytes, 18, readI16(bytes, 18) !== PROTOCOL.temperatureUnavailable),
    skinDelta1mC: optionalI16(bytes, 20),
    skinDelta5mC: optionalI16(bytes, 22),
    heartRateDelta1m: readI16(bytes, 24),
    coreHistorySeconds: readU16(bytes, 26),
    contactSamples: readU16(bytes, 28),
    typicalSamples: bytes[30],
    heartRate: has(PROTOCOL.flags.heartRateValid) ? bytes[31] : null,
    quality: bytes[32],
    sensorStatus: bytes[33],
    temperatureState: bytes[34],
    coreState: bytes[35],
    freshness: bytes[36],
    confidence: bytes[37],
    modelMode: bytes[38],
    modelVersion: bytes[39],
    heartRateUsed: bytes[40] !== 0,
    rawHex: hex(bytes.slice(0, PROTOCOL.debugFrameSize)),
    receivedAt: Date.now(),
  };
}

module.exports = {
  asBytes,
  decodeDebugSnapshot,
  hex,
};
