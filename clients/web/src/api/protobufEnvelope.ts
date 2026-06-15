import { Reader, Writer } from 'protobufjs/minimal';
import type { AuthSession } from '../types/mychat';

export const CMD_SEND_MESSAGE = 2001;
export const CMD_PUSH_MESSAGE = 5001;

type ImHeader = {
  version?: string;
  seq?: number;
  cmdId?: number;
  fromUid?: string;
  toUid?: string;
  timestamp?: number | string;
  token?: string;
  deviceId?: string;
  platform?: string;
};

type BaseResponse = {
  errorCode?: number;
  errorMessage?: string;
};

type MessageBody = {
  messageId?: string;
  type?: number;
  content?: string;
  isRecalled?: boolean;
  isRead?: boolean;
  atUids?: string;
  ext?: string;
  senderUid?: string;
  receiverUid?: string;
  createTime?: number | string;
  deliveredTime?: number | string;
  readTime?: number | string;
};

type PushMessageBody = {
  pushId?: string;
  type?: number;
  content?: string;
  relatedMessageId?: string;
  ext?: string;
};

export type DecodedEnvelope = {
  header: ImHeader;
  typeName: string;
  payload: Record<string, unknown>;
};

let seq = 1;

export async function encodeSendMessageFrame(params: {
  session: AuthSession;
  receiverUid: string;
  content: string;
}): Promise<Uint8Array> {
  const header: ImHeader = {
    version: '1.0',
    seq: seq++,
    cmdId: CMD_SEND_MESSAGE,
    fromUid: params.session.profile.uid,
    toUid: params.receiverUid,
    timestamp: Date.now(),
    token: params.session.accessToken,
    deviceId: params.session.deviceId,
    platform: params.session.platform,
  };

  const body: MessageBody = {
    type: 0,
    content: params.content,
    senderUid: params.session.profile.uid,
    receiverUid: params.receiverUid,
    createTime: Date.now(),
  };

  const messageBytes = encodeSendMessageRequest({ header, body });
  return encodeEnvelope(encodeImHeader(header), 'im.message.SendMessageRequest', messageBytes);
}

export async function decodeEnvelopeFrame(frame: ArrayBuffer): Promise<DecodedEnvelope> {
  const bytes = new Uint8Array(frame);
  const withoutCrc = verifyAndStripCrc(bytes);
  let offset = 0;
  const headerSize = readVarint32(withoutCrc, offset);
  offset = headerSize.nextOffset;
  const typeNameSize = readVarint32(withoutCrc, offset);
  offset = typeNameSize.nextOffset;

  const typeName = new TextDecoder().decode(withoutCrc.slice(offset, offset + typeNameSize.value));
  offset += typeNameSize.value;
  const headerBytes = withoutCrc.slice(offset, offset + headerSize.value);
  offset += headerSize.value;
  const messageBytes = withoutCrc.slice(offset);

  const header = decodeImHeader(headerBytes);
  return { header, typeName, payload: decodePayload(typeName, messageBytes) };
}

function decodePayload(typeName: string, bytes: Uint8Array): Record<string, unknown> {
  switch (typeName) {
    case 'im.message.SendMessageResponse':
      return decodeSendMessageResponse(bytes) as Record<string, unknown>;
    case 'im.message.SendMessageRequest':
      return decodeSendMessageRequest(bytes) as Record<string, unknown>;
    case 'im.push.PushRequest':
      return decodePushRequest(bytes) as Record<string, unknown>;
    case 'im.base.BaseResponse':
      return decodeBaseResponse(bytes) as Record<string, unknown>;
    default:
      return { rawBytes: bytes.length };
  }
}

function encodeEnvelope(headerBytes: Uint8Array, typeName: string, messageBytes: Uint8Array): Uint8Array {
  const typeBytes = new TextEncoder().encode(typeName);
  const headerSize = writeVarint32(headerBytes.length);
  const typeSize = writeVarint32(typeBytes.length);
  const bodyLength = headerSize.length + typeSize.length + typeBytes.length + headerBytes.length + messageBytes.length;
  const body = new Uint8Array(bodyLength);
  let offset = 0;
  body.set(headerSize, offset);
  offset += headerSize.length;
  body.set(typeSize, offset);
  offset += typeSize.length;
  body.set(typeBytes, offset);
  offset += typeBytes.length;
  body.set(headerBytes, offset);
  offset += headerBytes.length;
  body.set(messageBytes, offset);

  const crc = crc32(body);
  const output = new Uint8Array(body.length + 4);
  output.set(body, 0);
  output[body.length] = crc & 0xff;
  output[body.length + 1] = (crc >>> 8) & 0xff;
  output[body.length + 2] = (crc >>> 16) & 0xff;
  output[body.length + 3] = (crc >>> 24) & 0xff;
  return output;
}

function encodeImHeader(value: ImHeader): Uint8Array {
  const writer = Writer.create();
  if (value.version) writer.uint32(10).string(value.version);
  if (value.seq !== undefined) writer.uint32(16).uint32(value.seq);
  if (value.cmdId !== undefined) writer.uint32(24).uint32(value.cmdId);
  if (value.fromUid) writer.uint32(34).string(value.fromUid);
  if (value.toUid) writer.uint32(42).string(value.toUid);
  if (value.timestamp !== undefined) writer.uint32(48).uint64(value.timestamp);
  if (value.token) writer.uint32(58).string(value.token);
  if (value.deviceId) writer.uint32(66).string(value.deviceId);
  if (value.platform) writer.uint32(74).string(value.platform);
  return writer.finish();
}

function decodeImHeader(bytes: Uint8Array): ImHeader {
  const reader = Reader.create(bytes);
  const end = reader.len;
  const out: ImHeader = {};
  while (reader.pos < end) {
    const tag = reader.uint32();
    switch (tag >>> 3) {
      case 1: out.version = reader.string(); break;
      case 2: out.seq = reader.uint32(); break;
      case 3: out.cmdId = reader.uint32(); break;
      case 4: out.fromUid = reader.string(); break;
      case 5: out.toUid = reader.string(); break;
      case 6: out.timestamp = reader.uint64().toString(); break;
      case 7: out.token = reader.string(); break;
      case 8: out.deviceId = reader.string(); break;
      case 9: out.platform = reader.string(); break;
      default: reader.skipType(tag & 7); break;
    }
  }
  return out;
}

function encodeMessageBody(value: MessageBody): Uint8Array {
  const writer = Writer.create();
  if (value.messageId) writer.uint32(10).string(value.messageId);
  if (value.type !== undefined) writer.uint32(16).int32(value.type);
  if (value.content) writer.uint32(26).string(value.content);
  if (value.isRecalled !== undefined) writer.uint32(32).bool(value.isRecalled);
  if (value.isRead !== undefined) writer.uint32(40).bool(value.isRead);
  if (value.atUids) writer.uint32(50).string(value.atUids);
  if (value.ext) writer.uint32(58).string(value.ext);
  if (value.senderUid) writer.uint32(66).string(value.senderUid);
  if (value.receiverUid) writer.uint32(74).string(value.receiverUid);
  if (value.createTime !== undefined) writer.uint32(80).int64(value.createTime);
  if (value.deliveredTime !== undefined) writer.uint32(88).int64(value.deliveredTime);
  if (value.readTime !== undefined) writer.uint32(96).int64(value.readTime);
  return writer.finish();
}

function decodeMessageBody(bytes: Uint8Array): MessageBody {
  const reader = Reader.create(bytes);
  const out: MessageBody = {};
  while (reader.pos < reader.len) {
    const tag = reader.uint32();
    switch (tag >>> 3) {
      case 1: out.messageId = reader.string(); break;
      case 2: out.type = reader.int32(); break;
      case 3: out.content = reader.string(); break;
      case 4: out.isRecalled = reader.bool(); break;
      case 5: out.isRead = reader.bool(); break;
      case 6: out.atUids = reader.string(); break;
      case 7: out.ext = reader.string(); break;
      case 8: out.senderUid = reader.string(); break;
      case 9: out.receiverUid = reader.string(); break;
      case 10: out.createTime = reader.int64().toString(); break;
      case 11: out.deliveredTime = reader.int64().toString(); break;
      case 12: out.readTime = reader.int64().toString(); break;
      default: reader.skipType(tag & 7); break;
    }
  }
  return out;
}

function encodeSendMessageRequest(value: { header: ImHeader; body: MessageBody }): Uint8Array {
  const writer = Writer.create();
  writer.uint32(10).bytes(encodeImHeader(value.header));
  writer.uint32(18).bytes(encodeMessageBody(value.body));
  return writer.finish();
}

function decodeSendMessageRequest(bytes: Uint8Array): { header?: ImHeader; body?: MessageBody } {
  const reader = Reader.create(bytes);
  const out: { header?: ImHeader; body?: MessageBody } = {};
  while (reader.pos < reader.len) {
    const tag = reader.uint32();
    const field = tag >>> 3;
    if (field === 1) out.header = decodeImHeader(reader.bytes());
    else if (field === 2) out.body = decodeMessageBody(reader.bytes());
    else reader.skipType(tag & 7);
  }
  return out;
}

function decodeBaseResponse(bytes: Uint8Array): BaseResponse {
  const reader = Reader.create(bytes);
  const out: BaseResponse = {};
  while (reader.pos < reader.len) {
    const tag = reader.uint32();
    switch (tag >>> 3) {
      case 1: out.errorCode = reader.int32(); break;
      case 2: out.errorMessage = reader.string(); break;
      case 3: reader.bytes(); break;
      default: reader.skipType(tag & 7); break;
    }
  }
  return out;
}

function decodeSendMessageResponse(bytes: Uint8Array): { base?: BaseResponse; message?: MessageBody } {
  const reader = Reader.create(bytes);
  const out: { base?: BaseResponse; message?: MessageBody } = {};
  while (reader.pos < reader.len) {
    const tag = reader.uint32();
    const field = tag >>> 3;
    if (field === 1) out.base = decodeBaseResponse(reader.bytes());
    else if (field === 2) out.message = decodeMessageBody(reader.bytes());
    else reader.skipType(tag & 7);
  }
  return out;
}

function decodePushMessageBody(bytes: Uint8Array): PushMessageBody {
  const reader = Reader.create(bytes);
  const out: PushMessageBody = {};
  while (reader.pos < reader.len) {
    const tag = reader.uint32();
    switch (tag >>> 3) {
      case 1: out.pushId = reader.string(); break;
      case 2: out.type = reader.int32(); break;
      case 3: out.content = reader.string(); break;
      case 4: out.relatedMessageId = reader.string(); break;
      case 5: out.ext = reader.string(); break;
      default: reader.skipType(tag & 7); break;
    }
  }
  return out;
}

function decodePushRequest(bytes: Uint8Array): { header?: ImHeader; body?: PushMessageBody } {
  const reader = Reader.create(bytes);
  const out: { header?: ImHeader; body?: PushMessageBody } = {};
  while (reader.pos < reader.len) {
    const tag = reader.uint32();
    const field = tag >>> 3;
    if (field === 1) out.header = decodeImHeader(reader.bytes());
    else if (field === 2) out.body = decodePushMessageBody(reader.bytes());
    else reader.skipType(tag & 7);
  }
  return out;
}

function verifyAndStripCrc(bytes: Uint8Array): Uint8Array {
  if (bytes.length < 4) throw new Error('Frame too small');
  const body = bytes.slice(0, bytes.length - 4);
  const received =
    bytes[bytes.length - 4] |
    (bytes[bytes.length - 3] << 8) |
    (bytes[bytes.length - 2] << 16) |
    (bytes[bytes.length - 1] << 24);
  const calculated = crc32(body) | 0;
  if (received !== calculated) {
    throw new Error(`CRC mismatch: received=${received >>> 0} calculated=${calculated >>> 0}`);
  }
  return body;
}

function writeVarint32(value: number): Uint8Array {
  const out: number[] = [];
  let current = value >>> 0;
  while (current > 127) {
    out.push((current & 0x7f) | 0x80);
    current >>>= 7;
  }
  out.push(current);
  return new Uint8Array(out);
}

function readVarint32(bytes: Uint8Array, startOffset: number): { value: number; nextOffset: number } {
  let result = 0;
  let shift = 0;
  let offset = startOffset;
  while (offset < bytes.length) {
    const byte = bytes[offset++];
    result |= (byte & 0x7f) << shift;
    if ((byte & 0x80) === 0) return { value: result >>> 0, nextOffset: offset };
    shift += 7;
    if (shift > 28) throw new Error('Invalid varint32');
  }
  throw new Error('Unexpected end of varint32');
}

const crcTable = new Uint32Array(256);
for (let i = 0; i < 256; i += 1) {
  let crc = i;
  for (let j = 0; j < 8; j += 1) {
    crc = (crc & 1) ? (0xedb88320 ^ (crc >>> 1)) : (crc >>> 1);
  }
  crcTable[i] = crc >>> 0;
}

function crc32(bytes: Uint8Array): number {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  }
  return (crc ^ 0xffffffff) >>> 0;
}
