import type { Settings, WsEvent } from '../types/mychat';
import { makeId } from '../utils/storage';
import { decodeEnvelopeFrame } from './protobufEnvelope';

type EventSink = (event: WsEvent) => void;

export class GatewayWsClient {
  private settings: Settings;
  private sink: EventSink;
  private socket: WebSocket | null = null;

  constructor(settings: Settings, sink: EventSink) {
    this.settings = settings;
    this.sink = sink;
  }

  updateSettings(settings: Settings): void {
    this.settings = settings;
  }

  updateSink(sink: EventSink): void {
    this.sink = sink;
  }

  connect(token?: string): void {
    this.disconnect();
    const url = withToken(this.settings.wsUrl, token);
    this.emit('state', 'connecting', url);
    const ws = new WebSocket(url);
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => this.emit('state', 'connected', this.settings.wsUrl);
    ws.onclose = (event) => {
      this.emit('state', 'closed', `${event.code} ${event.reason || ''}`.trim());
      if (this.socket === ws) this.socket = null;
    };
    ws.onerror = () => this.emit('error', 'error', 'WebSocket 连接错误');
    ws.onmessage = (event) => {
      if (typeof event.data === 'string') {
        this.emit('in', 'text', event.data);
        return;
      }
      if (event.data instanceof ArrayBuffer) {
        this.emit('in', 'binary', `${event.data.byteLength} bytes`);
        decodeEnvelopeFrame(event.data)
          .then((decoded) => {
            this.emit('in', decoded.typeName, JSON.stringify(decoded));
          })
          .catch((error) => {
            this.emit('error', 'decode-failed', error instanceof Error ? error.message : String(error));
          });
        return;
      }
      this.emit('in', 'unknown', Object.prototype.toString.call(event.data));
    };

    this.socket = ws;
  }

  disconnect(): void {
    if (!this.socket) return;
    this.socket.close();
    this.socket = null;
    this.emit('state', 'disconnect-requested', '');
  }

  sendTextForProbe(text: string): boolean {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
      this.emit('error', 'send-failed', 'WebSocket 尚未连接');
      return false;
    }
    this.socket.send(text);
    this.emit('out', 'text-probe', text);
    return true;
  }

  sendBinaryFrame(frame: Uint8Array, kind = 'protobuf'): boolean {
    if (!this.socket || this.socket.readyState !== WebSocket.OPEN) {
      this.emit('error', 'send-failed', 'WebSocket 尚未连接');
      return false;
    }
    this.socket.send(frame);
    this.emit('out', kind, `${frame.byteLength} bytes`);
    return true;
  }

  status(): string {
    if (!this.socket) return 'disconnected';
    switch (this.socket.readyState) {
      case WebSocket.CONNECTING:
        return 'connecting';
      case WebSocket.OPEN:
        return 'connected';
      case WebSocket.CLOSING:
        return 'closing';
      case WebSocket.CLOSED:
        return 'closed';
      default:
        return 'unknown';
    }
  }

  private emit(direction: WsEvent['direction'], kind: string, detail: string): void {
    this.sink({ id: makeId('ws'), at: Date.now(), direction, kind, detail });
  }
}

function withToken(wsUrl: string, token?: string): string {
  if (!token) return wsUrl;
  const url = new URL(wsUrl);
  url.searchParams.set('token', token);
  return url.toString();
}
