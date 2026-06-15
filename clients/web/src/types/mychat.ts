export type Profile = {
  uid: string;
  account: string;
  nickname: string;
  avatar?: string;
  gender?: number;
  signature?: string;
  create_time?: number;
  last_login?: number;
  online?: boolean;
};

export type AuthSession = {
  profile: Profile;
  accessToken: string;
  refreshToken: string;
  deviceId: string;
  platform: string;
};

export type Settings = {
  httpBaseUrl: string;
  wsUrl: string;
  sendShortcut: 'enter' | 'ctrlEnter';
};

export type DirectMessage = {
  msg_id: number;
  sender_uid: string;
  receiver_uid: string;
  content: string;
  msg_type: number;
  status: number;
  create_time: number;
  delivered_time: number;
  read_time: number;
};

export type FriendInfo = {
  friend_id: number;
  friend_uid: string;
  nickname: string;
  status: number;
  created_at: number;
};

export type GroupInfo = {
  group_id: number;
  name: string;
  creator_uid: string;
  created_at: number;
  member_count: number;
};

export type GroupSearchResult = GroupInfo & {
  joined?: boolean;
};

export type GroupInfoDetail = {
  group: GroupInfo;
  joined: boolean;
  members: GroupMember[];
};

export type GroupMember = {
  id: number;
  user_uid: string;
  nickname: string;
  role: number;
  joined_at: number;
};

export type GroupMessage = {
  msg_id: number;
  sender_uid: string;
  sender_nickname: string;
  content: string;
  created_at: number;
};

export type HttpEvent = {
  id: string;
  at: number;
  method: string;
  path: string;
  status?: number;
  ok: boolean;
  body?: unknown;
  error?: string;
};

export type WsEvent = {
  id: string;
  at: number;
  direction: 'in' | 'out' | 'state' | 'error';
  kind: string;
  detail: string;
};

export type ViewMode = 'chats' | 'friends' | 'groups' | 'profile' | 'debug' | 'settings';

export type Conversation =
  | { type: 'direct'; id: string; title: string }
  | { type: 'group'; id: string; title: string; groupId: number };
