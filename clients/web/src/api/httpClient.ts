import type {
  AuthSession,
  DirectMessage,
  GroupInfoDetail,
  FriendInfo,
  GroupInfo,
  GroupSearchResult,
  GroupMember,
  GroupMessage,
  HttpEvent,
  Profile,
  Settings,
} from '../types/mychat';
import { makeId } from '../utils/storage';

type EventSink = (event: HttpEvent) => void;

type RequestOptions = {
  method?: 'GET' | 'POST';
  token?: string;
  body?: unknown;
};

export class GatewayHttpError extends Error {
  readonly status: number;
  readonly path: string;
  readonly rawMessage: string;

  constructor(status: number, path: string, rawMessage: string) {
    super(toUserFacingError(status, rawMessage));
    this.name = 'GatewayHttpError';
    this.status = status;
    this.path = path;
    this.rawMessage = rawMessage;
  }
}

export function isAuthExpiredError(error: unknown): boolean {
  if (!(error instanceof GatewayHttpError) || error.status !== 401) return false;
  const raw = error.rawMessage.toLowerCase();
  return raw.includes('expired')
    || raw.includes('invalid or expired access token')
    || raw.includes('invalid token')
    || raw.includes('missing or invalid authorization')
    || raw.includes('authorization header');
}

export class GatewayHttpClient {
  private settings: Settings;
  private sink: EventSink;

  constructor(settings: Settings, sink: EventSink) {
    this.settings = settings;
    this.sink = sink;
  }

  updateSettings(settings: Settings): void {
    this.settings = settings;
  }

  async register(payload: {
    account: string;
    password: string;
    nickname: string;
    device_id: string;
    platform: string;
  }): Promise<AuthSession> {
    const data = await this.request<{
      profile: Profile;
      access_token: string;
      refresh_token: string;
    }>('/api/v1/auth/register', { method: 'POST', body: payload });

    return {
      profile: data.profile,
      accessToken: data.access_token,
      refreshToken: data.refresh_token,
      deviceId: payload.device_id,
      platform: payload.platform,
    };
  }

  async login(payload: {
    account: string;
    password: string;
    device_id: string;
    platform: string;
  }): Promise<AuthSession> {
    const data = await this.request<{
      profile: Profile;
      access_token: string;
      refresh_token: string;
    }>('/api/v1/auth/login', { method: 'POST', body: payload });

    return {
      profile: data.profile,
      accessToken: data.access_token,
      refreshToken: data.refresh_token,
      deviceId: payload.device_id,
      platform: payload.platform,
    };
  }

  async profile(token: string): Promise<Profile> {
    const data = await this.request<{ profile: Profile }>('/api/v1/auth/info', { token });
    return data.profile;
  }

  async updateProfile(token: string, payload: {
    nickname: string;
    avatar?: string;
    gender?: number;
    signature?: string;
  }): Promise<Profile> {
    const data = await this.request<{ profile: Profile }>('/api/v1/auth/profile', {
      method: 'POST',
      token,
      body: payload,
    });
    return data.profile;
  }

  async searchUser(token: string, keyword: string): Promise<Profile> {
    const users = await this.searchUsers(token, keyword);
    return users[0];
  }

  async searchUsers(token: string, keyword: string): Promise<Profile[]> {
    const query = new URLSearchParams({ q: keyword });
    const data = await this.request<{ users?: Profile[]; profile?: Profile }>(
      `/api/v1/users/search?${query.toString()}`,
      { token },
    );
    if (Array.isArray(data.users)) return data.users;
    return data.profile ? [data.profile] : [];
  }

  async sendDirectMessage(token: string, receiverUid: string, content: string): Promise<DirectMessage> {
    const data = await this.request<{ message: DirectMessage }>('/api/v1/messages/send', {
      method: 'POST',
      token,
      body: { receiver_uid: receiverUid, content },
    });
    return data.message;
  }

  async directHistory(token: string, peerUid: string, limit = 50): Promise<DirectMessage[]> {
    const query = new URLSearchParams({
      peer_uid: peerUid,
      before_time: String(Number.MAX_SAFE_INTEGER),
      limit: String(limit),
    });
    const data = await this.request<{ messages: DirectMessage[] }>(
      `/api/v1/messages/history?${query.toString()}`,
      { token },
    );
    return data.messages;
  }

  async offlineMessages(token: string, limit = 50): Promise<DirectMessage[]> {
    const query = new URLSearchParams({
      before_time: String(Number.MAX_SAFE_INTEGER),
      limit: String(limit),
    });
    const data = await this.request<{ messages: DirectMessage[] }>(
      `/api/v1/messages/offline?${query.toString()}`,
      { token },
    );
    return data.messages;
  }

  async sendFriendRequest(token: string, targetUid: string): Promise<unknown> {
    return this.request('/api/v1/friends/request', {
      method: 'POST',
      token,
      body: { target_uid: targetUid },
    });
  }

  async respondFriendRequest(token: string, friendId: number, accept: boolean): Promise<unknown> {
    return this.request('/api/v1/friends/respond', {
      method: 'POST',
      token,
      body: { friend_id: friendId, accept },
    });
  }

  async friends(token: string): Promise<FriendInfo[]> {
    const data = await this.request<{ friends: FriendInfo[] }>('/api/v1/friends', { token });
    return data.friends;
  }

  async pendingFriends(token: string): Promise<FriendInfo[]> {
    const data = await this.request<{ pending_requests: FriendInfo[] }>(
      '/api/v1/friends/pending',
      { token },
    );
    return data.pending_requests;
  }

  async createGroup(token: string, name: string): Promise<{ group_id: number; message: string }> {
    return this.request('/api/v1/groups', { method: 'POST', token, body: { name } });
  }

  async joinGroup(token: string, groupId: number): Promise<unknown> {
    return this.request('/api/v1/groups/join', {
      method: 'POST',
      token,
      body: { group_id: groupId },
    });
  }

  async leaveGroup(token: string, groupId: number): Promise<unknown> {
    return this.request('/api/v1/groups/leave', {
      method: 'POST',
      token,
      body: { group_id: groupId },
    });
  }

  async groups(token: string): Promise<GroupInfo[]> {
    const data = await this.request<{ groups: GroupInfo[] }>('/api/v1/groups', { token });
    return data.groups;
  }

  async searchGroups(token: string, keyword: string): Promise<GroupSearchResult[]> {
    const query = new URLSearchParams({ q: keyword });
    const data = await this.request<{ groups: GroupSearchResult[] }>(
      `/api/v1/groups/search?${query.toString()}`,
      { token },
    );
    return data.groups;
  }

  async groupInfo(token: string, groupId: number): Promise<GroupInfoDetail> {
    const query = new URLSearchParams({ group_id: String(groupId) });
    return this.request<GroupInfoDetail>(`/api/v1/groups/info?${query.toString()}`, { token });
  }

  async groupMembers(token: string, groupId: number): Promise<GroupMember[]> {
    const data = await this.request<{ members: GroupMember[] }>(
      `/api/v1/groups/members?group_id=${encodeURIComponent(String(groupId))}`,
      { token },
    );
    return data.members;
  }

  async sendGroupMessage(token: string, groupId: number, content: string): Promise<{ msg_id: number; message: string }> {
    return this.request('/api/v1/groups/messages/send', {
      method: 'POST',
      token,
      body: { group_id: groupId, content },
    });
  }

  async groupHistory(token: string, groupId: number, limit = 50): Promise<GroupMessage[]> {
    const query = new URLSearchParams({
      group_id: String(groupId),
      before: String(Date.now()),
      limit: String(limit),
    });
    const data = await this.request<{ messages: GroupMessage[] }>(
      `/api/v1/groups/messages/history?${query.toString()}`,
      { token },
    );
    return data.messages;
  }

  private async request<T>(path: string, options: RequestOptions = {}): Promise<T> {
    const method = options.method ?? 'GET';
    const url = `${this.settings.httpBaseUrl.replace(/\/$/, '')}${path}`;
    const headers: Record<string, string> = {};
    if (options.body !== undefined) headers['Content-Type'] = 'application/json';
    if (options.token) headers.Authorization = `Bearer ${options.token}`;

    try {
      const response = await fetch(url, {
        method,
        headers,
        body: options.body === undefined ? undefined : JSON.stringify(options.body),
      });
      const text = await response.text();
      const body = text ? (JSON.parse(text) as unknown) : null;
      this.sink({
        id: makeId('http'),
        at: Date.now(),
        method,
        path,
        status: response.status,
        ok: response.ok,
        body,
      });

      if (!response.ok) {
        throw new GatewayHttpError(response.status, path, extractError(body));
      }
      return body as T;
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      this.sink({
        id: makeId('http'),
        at: Date.now(),
        method,
        path,
        ok: false,
        error: message,
      });
      throw error;
    }
  }
}

function extractError(body: unknown): string {
  if (!body || typeof body !== 'object') return '';
  const record = body as Record<string, unknown>;
  for (const key of ['message', 'error', 'err_msg', 'msg']) {
    if (typeof record[key] === 'string') return record[key];
  }
  return '';
}

function toUserFacingError(status: number, rawMessage: string): string {
  const lower = rawMessage.toLowerCase();
  if (status === 400) return rawMessage || '请求内容不完整，请检查输入。';
  if (status === 401) {
    if (lower.includes('password') || lower.includes('credential') || lower.includes('account')) {
      return '账号或密码不正确。';
    }
    return '登录状态已过期，请重新登录。';
  }
  if (status === 403) return '没有权限执行这个操作。';
  if (status === 404) return '没有找到对应的数据。';
  if (status === 409) return rawMessage || '数据已存在，请不要重复操作。';
  if (status === 422) return rawMessage || '输入格式不正确，请检查后重试。';
  if (status >= 500) return '后端服务暂时不可用，请稍后重试。';
  return rawMessage || '请求失败，请稍后重试。';
}
