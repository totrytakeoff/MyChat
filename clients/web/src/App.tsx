import { useEffect, useMemo, useRef, useState } from 'react';
import {
  Bug,
  ChevronRight,
  Check,
  Circle,
  Contact,
  Edit3,
  LogOut,
  MessageCircle,
  PanelRightOpen,
  Plus,
  RefreshCw,
  Search,
  Send,
  Settings as SettingsIcon,
  Share2,
  Trash2,
  UserPlus,
  Users,
  Wifi,
  WifiOff,
  X,
} from 'lucide-react';
import { GatewayHttpClient, isAuthExpiredError } from './api/httpClient';
import { encodeSendMessageFrame } from './api/protobufEnvelope';
import { GatewayWsClient } from './api/wsClient';
import type {
  AuthSession,
  Conversation,
  DirectMessage,
  FriendInfo,
  GroupInfoDetail,
  GroupInfo,
  GroupMember,
  GroupMessage,
  GroupSearchResult,
  HttpEvent,
  Profile,
  Settings,
  ViewMode,
  WsEvent,
} from './types/mychat';
import {
  loadJson,
  loadSessionJson,
  loadSessionOnlyJson,
  removeSessionItem,
  saveJson,
  saveSessionJson,
} from './utils/storage';

function defaultSettings(): Settings {
  return {
    httpBaseUrl: '',
    wsUrl: `${window.location.protocol === 'https:' ? 'wss' : 'ws'}://${window.location.host}/ws`,
    sendShortcut: 'enter',
  };
}

function normalizeSettings(value: Settings): Settings {
  return {
    ...defaultSettings(),
    ...value,
    sendShortcut: value.sendShortcut === 'ctrlEnter' ? 'ctrlEnter' : 'enter',
  };
}

function createDeviceId(): string {
  return `web-${crypto.randomUUID?.() ?? Math.random().toString(16).slice(2)}`;
}

type Notice = { type: 'ok' | 'error' | 'info'; text: string };
type AuthMode = 'login' | 'register';
type TransportMode = 'http' | 'ws';

type AuthFormState = {
  account: string;
  password: string;
  nickname: string;
};

type ConversationListItem = {
  key: string;
  type: Conversation['type'];
  id: string;
  title: string;
  subtitle: string;
  unread: number;
  conversation: Conversation;
};

type FriendListItem = {
  key: string;
  title: string;
  subtitle: string;
  friend: FriendInfo;
};

type GroupListItem = {
  key: string;
  title: string;
  subtitle: string;
  group: GroupInfo;
};

type ProfileFormState = {
  nickname: string;
  avatar: string;
  gender: number;
  signature: string;
};

export default function App() {
  const [settings, setSettings] = useState<Settings>(() => normalizeSettings(loadJson('settings', defaultSettings())));
  const [deviceId] = useState(() => {
    const saved = loadSessionOnlyJson('deviceId', '');
    if (saved) return saved;
    const next = createDeviceId();
    saveSessionJson('deviceId', next);
    return next;
  });
  const [session, setSession] = useState<AuthSession | null>(() => loadSessionJson<AuthSession | null>('session', null));
  const [view, setView] = useState<ViewMode>('chats');
  const [notice, setNotice] = useState<Notice>({ type: 'info', text: '就绪' });
  const [httpEvents, setHttpEvents] = useState<HttpEvent[]>([]);
  const [wsEvents, setWsEvents] = useState<WsEvent[]>([]);
  const [friends, setFriends] = useState<FriendInfo[]>([]);
  const [pendingFriends, setPendingFriends] = useState<FriendInfo[]>([]);
  const [groups, setGroups] = useState<GroupInfo[]>([]);
  const [members, setMembers] = useState<GroupMember[]>([]);
  const [directMessages, setDirectMessages] = useState<DirectMessage[]>([]);
  const [groupMessagesById, setGroupMessagesById] = useState<Record<number, GroupMessage[]>>({});
  const [directConversations, setDirectConversations] = useState<Conversation[]>([]);
  const [activeConversation, setActiveConversation] = useState<Conversation | null>(null);
  const [wsStatus, setWsStatus] = useState('disconnected');
  const [debugOpen, setDebugOpen] = useState(false);

  const [authMode, setAuthMode] = useState<AuthMode>('login');
  const [authForm, setAuthForm] = useState<AuthFormState>({
    account: '',
    password: '',
    nickname: '',
  });
  const [conversationSearch, setConversationSearch] = useState('');
  const [friendSearch, setFriendSearch] = useState('');
  const [groupSearch, setGroupSearch] = useState('');
  const [peerUid, setPeerUid] = useState('');
  const [messageDraft, setMessageDraft] = useState('');
  const [targetUid, setTargetUid] = useState('');
  const [searchedUsers, setSearchedUsers] = useState<Profile[]>([]);
  const [selectedProfile, setSelectedProfile] = useState<Profile | null>(null);
  const [selectedFriendUid, setSelectedFriendUid] = useState('');
  const [selectedGroupId, setSelectedGroupId] = useState<number | null>(null);
  const [groupName, setGroupName] = useState('');
  const [groupSearchKeyword, setGroupSearchKeyword] = useState('');
  const [searchedGroups, setSearchedGroups] = useState<GroupSearchResult[]>([]);
  const [selectedGroupDetail, setSelectedGroupDetail] = useState<GroupInfoDetail | null>(null);
  const [groupDraft, setGroupDraft] = useState('');
  const [unreadByConversation, setUnreadByConversation] = useState<Record<string, number>>({});
  const [transport, setTransport] = useState<TransportMode>('http');
  const [wsProbe, setWsProbe] = useState('probe');

  const httpClientRef = useRef<GatewayHttpClient | null>(null);
  const wsClientRef = useRef<GatewayWsClient | null>(null);
  const sessionRef = useRef<AuthSession | null>(session);
  const activeConversationRef = useRef<Conversation | null>(activeConversation);
  sessionRef.current = session;
  activeConversationRef.current = activeConversation;

  const appendHttpEvent = (event: HttpEvent) => {
    setHttpEvents((events) => [event, ...events].slice(0, 60));
  };

  const appendWsEvent = (event: WsEvent) => {
    setWsEvents((events) => [event, ...events].slice(0, 80));
    if (event.direction === 'state') setWsStatus(event.kind);
    if (event.direction === 'in') applyDecodedWsPayload(event);
  };

  const markUnread = (conversation: Conversation) => {
    const active = activeConversationRef.current;
    if (active && isSameConversation(active, conversation)) return;
    setUnreadByConversation((current) => {
      const key = conversationKey(conversation);
      return { ...current, [key]: (current[key] ?? 0) + 1 };
    });
  };

  const openConversation = (conversation: Conversation) => {
    setActiveConversation(conversation);
    setUnreadByConversation((current) => {
      const key = conversationKey(conversation);
      if (!current[key]) return current;
      const next = { ...current };
      delete next[key];
      return next;
    });
    setView('chats');
  };

  const applyDecodedWsPayload = (event: WsEvent) => {
    if (event.kind !== 'im.message.SendMessageResponse' && event.kind !== 'im.push.PushRequest') {
      return;
    }
    try {
      const decoded = JSON.parse(event.detail) as {
        header?: Record<string, unknown>;
        payload?: Record<string, unknown>;
      };
      if (event.kind === 'im.message.SendMessageResponse') {
        const message = decoded.payload?.message as Record<string, unknown> | undefined;
        if (!message) return;
        const current = sessionRef.current;
        const active = activeConversationRef.current;
        const peerUid = active?.type === 'direct'
          ? active.id
          : String(message.receiverUid ?? decoded.header?.fromUid ?? '');
        if (!current || !peerUid) return;
        addDirectConversation(peerUid);
        setDirectMessages((messages) => mergeDirectMessages(
          messages,
          [{
            msg_id: Number(message.messageId ?? 0),
            sender_uid: current.profile.uid,
            receiver_uid: peerUid,
            content: String(message.content ?? ''),
            msg_type: 0,
            status: 0,
            create_time: Date.now(),
            delivered_time: 0,
            read_time: 0,
          }],
        ));
        return;
      }

      const body = decoded.payload?.body as Record<string, unknown> | undefined;
      if (!body) return;
      const current = sessionRef.current;
      if (!current) {
        setNotice({ type: 'info', text: '收到新消息，请重新登录后查看。' });
        return;
      }

      const pushContext = parsePushContext(body, decoded.header);
      if (pushContext.conversationType === 'group' && pushContext.conversationId) {
        const groupId = Number(pushContext.conversationId);
        if (!Number.isFinite(groupId) || groupId <= 0) return;
        const group = groups.find((item) => item.group_id === groupId);
        markUnread({
          type: 'group',
          id: String(groupId),
          title: group?.name || groupTitle(groupId),
          groupId,
        });
        http.groupHistory(current.accessToken, groupId)
          .then((messages) => {
            setGroupMessagesById((currentMessages) => ({
              ...currentMessages,
              [groupId]: sortGroupMessages(messages),
            }));
            http.groups(current.accessToken)
              .then(setGroups)
              .catch(() => undefined);
            setNotice({ type: 'info', text: '收到新的群消息。' });
          })
          .catch(() => {
            setNotice({ type: 'info', text: '收到群消息，请重新打开群聊。' });
          });
        return;
      }

      const peerUid = pushContext.conversationType === 'direct'
        ? pushContext.conversationId || pushContext.senderUid
        : pushContext.senderUid;
      if (peerUid) {
        const friend = friends.find((item) => item.friend_uid === peerUid);
        markUnread({
          type: 'direct',
          id: peerUid,
          title: friend?.nickname || '联系人',
        });
        http.directHistory(current.accessToken, peerUid)
          .then((messages) => {
            setDirectMessages((currentMessages) => replaceDirectMessagesForPeer(
              currentMessages,
              messages,
              peerUid,
            ));
            addDirectConversation(peerUid, friend?.nickname || '联系人');
            setNotice({ type: 'info', text: '收到新的消息。' });
          })
          .catch(() => {
            setNotice({ type: 'info', text: '收到新消息，请重新打开当前会话。' });
          });
        return;
      }

      http.offlineMessages(current.accessToken)
        .then((messages) => {
          setDirectMessages((currentMessages) => mergeDirectMessages(currentMessages, messages));
          messages.forEach((message) => {
            const peer = message.sender_uid === current.profile.uid ? message.receiver_uid : message.sender_uid;
            const friend = friends.find((item) => item.friend_uid === peer);
            markUnread({
              type: 'direct',
              id: peer,
              title: friend?.nickname || '联系人',
            });
            addDirectConversation(peer, friend?.nickname || '联系人');
          });
          setNotice({ type: 'info', text: '收到新消息。' });
        })
        .catch(() => {
          setNotice({ type: 'info', text: '收到新消息，请打开对应会话查看。' });
        });
    } catch {
      // Raw decode details remain visible in the debug drawer.
    }
  };

  const http = useMemo(() => {
    const client = httpClientRef.current ?? new GatewayHttpClient(settings, appendHttpEvent);
    client.updateSettings(settings);
    httpClientRef.current = client;
    return client;
  }, [settings]);

  const ws = useMemo(() => {
    const client = wsClientRef.current ?? new GatewayWsClient(settings, appendWsEvent);
    client.updateSettings(settings);
    wsClientRef.current = client;
    setWsStatus(client.status());
    return client;
  }, [settings]);

  useEffect(() => {
    ws.updateSink(appendWsEvent);
  });

  const token = session?.accessToken ?? '';
  const ownUid = session?.profile.uid ?? '';

  const conversationItems = useMemo(() => {
    const directItems: ConversationListItem[] = directConversations.map((conversation) => {
      const friend = friends.find((item) => item.friend_uid === conversation.id);
      return {
        key: `direct-${conversation.id}`,
        type: 'direct',
        id: conversation.id,
        title: friend?.nickname || conversation.title,
        subtitle: friend ? '好友' : '最近会话',
        unread: unreadByConversation[conversationKey(conversation)] ?? 0,
        conversation: { ...conversation, title: friend?.nickname || conversation.title },
      };
    });

    const friendItems = friends
      .filter((friend) => !directConversations.some((conversation) => conversation.id === friend.friend_uid))
      .map<ConversationListItem>((friend) => ({
        key: `friend-${friend.friend_uid}`,
        type: 'direct',
        id: friend.friend_uid,
        title: friend.nickname || '联系人',
        subtitle: '好友',
        unread: 0,
        conversation: {
          type: 'direct',
          id: friend.friend_uid,
          title: friend.nickname || '联系人',
        },
      }));

    const groupItems = groups.map<ConversationListItem>((group) => ({
      key: `group-${group.group_id}`,
      type: 'group',
      id: String(group.group_id),
      title: group.name || groupTitle(group.group_id),
      subtitle: `${group.member_count} 位成员`,
      unread: unreadByConversation[conversationKey({
        type: 'group',
        id: String(group.group_id),
        title: group.name || groupTitle(group.group_id),
        groupId: group.group_id,
      })] ?? 0,
      conversation: {
        type: 'group',
        id: String(group.group_id),
        title: group.name || groupTitle(group.group_id),
        groupId: group.group_id,
      },
    }));

    const query = conversationSearch.trim().toLowerCase();
    return [...directItems, ...friendItems, ...groupItems].filter((item) => {
      if (!query) return true;
      return `${item.title} ${item.subtitle} ${item.id}`.toLowerCase().includes(query);
    });
  }, [directConversations, friends, groups, conversationSearch, unreadByConversation]);

  const friendItems = useMemo(() => {
    const query = friendSearch.trim().toLowerCase();
    return friends
      .map<FriendListItem>((friend) => ({
        key: `friend-${friend.friend_uid}`,
        title: friend.nickname || '联系人',
        subtitle: '好友',
        friend,
      }))
      .filter((item) => {
        if (!query) return true;
        return `${item.title} ${item.subtitle}`.toLowerCase().includes(query);
      });
  }, [friends, friendSearch]);

  const groupItems = useMemo(() => {
    const query = groupSearch.trim().toLowerCase();
    return groups
      .map<GroupListItem>((group) => ({
        key: `group-${group.group_id}`,
        title: group.name || groupTitle(group.group_id),
        subtitle: `${group.member_count} 位成员`,
        group,
      }))
      .filter((item) => {
        if (!query) return true;
        return `${item.title} ${item.subtitle}`.toLowerCase().includes(query);
      });
  }, [groups, groupSearch]);

  const selectedFriend = useMemo(
    () => friends.find((friend) => friend.friend_uid === selectedFriendUid) ?? null,
    [friends, selectedFriendUid],
  );

  const selectedPendingFriend = useMemo(
    () => pendingFriends.find((friend) => friend.friend_uid === selectedFriendUid) ?? null,
    [pendingFriends, selectedFriendUid],
  );

  const selectedGroup = useMemo(
    () => groups.find((group) => group.group_id === selectedGroupId) ?? null,
    [groups, selectedGroupId],
  );

  const visibleDirectMessages = useMemo(() => {
    if (activeConversation?.type !== 'direct') return [];
    const peer = activeConversation.id;
    return directMessages.filter((message) => {
      if (!ownUid) return message.sender_uid === peer || message.receiver_uid === peer;
      return (message.sender_uid === ownUid && message.receiver_uid === peer)
        || (message.sender_uid === peer && message.receiver_uid === ownUid);
    });
  }, [activeConversation, directMessages, ownUid]);

  const visibleGroupMessages = useMemo(() => {
    if (activeConversation?.type !== 'group') return [];
    return groupMessagesById[activeConversation.groupId] ?? [];
  }, [activeConversation, groupMessagesById]);

  const requireSession = (): AuthSession => {
    if (!session) throw new Error('请先登录');
    return session;
  };

  const run = async (label: string, action: () => Promise<void>): Promise<boolean> => {
    setNotice({ type: 'info', text: `${label}...` });
    try {
      await action();
      setNotice({ type: 'ok', text: `${label}成功` });
      return true;
    } catch (error) {
      handleAsyncError(error);
      return false;
    }
  };

  const persistSettings = (next: Settings) => {
    const normalized = normalizeSettings(next);
    setSettings(normalized);
    saveJson('settings', normalized);
  };

  const persistSession = (next: AuthSession | null) => {
    setSession(next);
    if (next) saveSessionJson('session', next);
    else {
      removeSessionItem('session');
      ws.disconnect();
      setActiveConversation(null);
      setView('chats');
    }
  };

  const expireSession = () => {
    persistSession(null);
    setNotice({ type: 'error', text: '登录状态已过期，请重新登录。' });
  };

  const handleAsyncError = (error: unknown, cancelled = false) => {
    if (cancelled) return;
    if (sessionRef.current && isAuthExpiredError(error)) {
      expireSession();
      return;
    }
    setNotice({ type: 'error', text: explainError(error) });
  };

  const addDirectConversation = (uid: string, title = '联系人') => {
    if (!uid.trim()) return;
    const normalized = uid.trim();
    setDirectConversations((items) => {
      if (items.some((item) => item.type === 'direct' && item.id === normalized)) return items;
      return [{ type: 'direct', id: normalized, title }, ...items];
    });
  };

  const syncFriends = async (current = requireSession()) => {
    const [friendList, pending] = await Promise.all([
      http.friends(current.accessToken),
      http.pendingFriends(current.accessToken),
    ]);
    setFriends(friendList);
    setPendingFriends(pending);
    if (selectedFriendUid && !friendList.some((friend) => friend.friend_uid === selectedFriendUid)) {
      setSelectedFriendUid('');
    }
  };

  const syncGroups = async (current = requireSession()) => {
    const groupList = await http.groups(current.accessToken);
    setGroups(groupList);
    if (selectedGroupId && !groupList.some((group) => group.group_id === selectedGroupId)) {
      setSelectedGroupId(null);
    }
    if (selectedGroupDetail) {
      const refreshed = groupList.find((group) => group.group_id === selectedGroupDetail.group.group_id);
      if (refreshed) {
        setSelectedGroupDetail((detail) => detail ? { ...detail, group: refreshed, joined: true } : detail);
      }
    }
  };

  const syncOffline = async (current = requireSession()) => {
    const messages = await http.offlineMessages(current.accessToken);
    setDirectMessages((currentMessages) => mergeDirectMessages(currentMessages, messages));
    messages.forEach((message) => {
      const peer = message.sender_uid === current.profile.uid ? message.receiver_uid : message.sender_uid;
      addDirectConversation(peer);
    });
  };

  const syncDirectHistory = async (peerUid: string, title?: string, current = requireSession()) => {
    const messages = await http.directHistory(current.accessToken, peerUid);
    setDirectMessages((currentMessages) => replaceDirectMessagesForPeer(
      currentMessages,
      messages,
      peerUid,
    ));
    addDirectConversation(peerUid, title);
  };

  const syncGroupHistory = async (groupId: number, current = requireSession()) => {
    const messages = await http.groupHistory(current.accessToken, groupId);
    setGroupMessagesById((currentMessages) => ({ ...currentMessages, [groupId]: sortGroupMessages(messages) }));
  };

  const register = () => run('注册', async () => {
    if (!authForm.account.trim()) throw new Error('请输入账号');
    if (!authForm.password.trim()) throw new Error('请输入密码');
    const next = await http.register({
      account: authForm.account.trim(),
      password: authForm.password,
      nickname: authForm.nickname.trim() || authForm.account.trim(),
      device_id: deviceId,
      platform: 'web',
    });
    persistSession(next);
    setView('chats');
  });

  const login = () => run('登录', async () => {
    if (!authForm.account.trim()) throw new Error('请输入账号');
    if (!authForm.password.trim()) throw new Error('请输入密码');
    const next = await http.login({
      account: authForm.account.trim(),
      password: authForm.password,
      device_id: deviceId,
      platform: 'web',
    });
    persistSession(next);
    setView('chats');
  });

  const loadProfile = () => run('刷新资料', async () => {
    const current = requireSession();
    const profile = await http.profile(current.accessToken);
    persistSession({ ...current, profile });
  });

  const updateProfile = (form: ProfileFormState) => run('保存资料', async () => {
    const current = requireSession();
    if (!form.nickname.trim()) throw new Error('请输入昵称');
    const profile = await http.updateProfile(current.accessToken, {
      nickname: form.nickname.trim(),
      avatar: form.avatar.trim(),
      gender: form.gender,
      signature: form.signature.trim(),
    });
    persistSession({ ...current, profile });
  });

  const openDirect = (uid = peerUid) => {
    const normalized = uid.trim();
    if (!normalized) return;
    const friend = friends.find((item) => item.friend_uid === normalized);
    const conversation: Conversation = {
      type: 'direct',
      id: normalized,
      title: friend?.nickname || '联系人',
    };
    addDirectConversation(normalized, conversation.title);
    openConversation(conversation);
    setPeerUid('');
  };

  const loadDirectHistory = () => run('刷新会话', async () => {
    const current = requireSession();
    const peer = activeConversation?.type === 'direct' ? activeConversation.id : peerUid.trim();
    if (!peer) throw new Error('请先选择联系人');
    const messages = await http.directHistory(current.accessToken, peer);
    setDirectMessages((currentMessages) => replaceDirectMessagesForPeer(currentMessages, messages, peer));
    openDirect(peer);
  });

  const pullOffline = () => run('同步收件箱', async () => {
    const current = requireSession();
    const messages = await http.offlineMessages(current.accessToken);
    setDirectMessages((currentMessages) => mergeDirectMessages(currentMessages, messages));
    messages.forEach((message) => {
      const peer = message.sender_uid === ownUid ? message.receiver_uid : message.sender_uid;
      addDirectConversation(peer);
    });
  });

  const sendDirect = () => run('发送消息', async () => {
    const current = requireSession();
    const peer = activeConversation?.type === 'direct' ? activeConversation.id : peerUid.trim();
    if (!peer) throw new Error('请先选择联系人');
    if (!messageDraft.trim()) throw new Error('请输入消息内容');
    const message = await http.sendDirectMessage(current.accessToken, peer, messageDraft.trim());
    setDirectMessages((messages) => mergeDirectMessages(messages, [message]));
    setMessageDraft('');
    openDirect(peer);
  });

  const sendDirectOverWs = () => run('发送实时消息', async () => {
    const current = requireSession();
    const peer = activeConversation?.type === 'direct' ? activeConversation.id : peerUid.trim();
    if (!peer) throw new Error('请先选择联系人');
    if (!messageDraft.trim()) throw new Error('请输入消息内容');
    const frame = await encodeSendMessageFrame({
      session: current,
      receiverUid: peer,
      content: messageDraft.trim(),
    });
    if (!ws.sendBinaryFrame(frame, 'CMD_SEND_MESSAGE')) {
      throw new Error('请先在设置中上线，再发送实时消息。');
    }
    setMessageDraft('');
    openDirect(peer);
  });

  const refreshFriends = () => run('刷新联系人', async () => {
    const current = requireSession();
    await syncFriends(current);
  });

  const searchFriendTarget = () => run('搜索用户', async () => {
    const current = requireSession();
    const keyword = targetUid.trim();
    if (!keyword) throw new Error('请输入账号或昵称');
    const profiles = await http.searchUsers(current.accessToken, keyword);
    setSearchedUsers(profiles);
    setSelectedProfile(profiles[0] ?? null);
  });

  const sendFriendRequest = () => run('发送好友申请', async () => {
    const current = requireSession();
    if (!selectedProfile) throw new Error('请先搜索并选择一个用户');
    if (selectedProfile.uid === current.profile.uid) throw new Error('不能添加自己为好友');
    if (friends.some((friend) => friend.friend_uid === selectedProfile.uid)) throw new Error('你们已经是好友');
    await http.sendFriendRequest(current.accessToken, selectedProfile.uid);
    setTargetUid('');
    setSearchedUsers([]);
    setSelectedProfile(null);
    await syncFriends(current);
  });

  const respondFriend = (accept: boolean, friendId: number) => run(accept ? '同意好友申请' : '拒绝好友申请', async () => {
    const current = requireSession();
    if (!friendId) throw new Error('请选择一条好友申请');
    await http.respondFriendRequest(current.accessToken, friendId, accept);
    await syncFriends(current);
  });

  const refreshGroups = () => run('刷新群聊', async () => {
    const current = requireSession();
    await syncGroups(current);
  });

  const createGroup = () => run('创建群聊', async () => {
    const current = requireSession();
    if (!groupName.trim()) throw new Error('请输入群聊名称');
    const created = await http.createGroup(current.accessToken, groupName.trim());
    setGroupName('');
    await syncGroups(current);
    await selectGroupInfo(created.group_id, current);
  });

  const searchGroupTarget = () => run('搜索群聊', async () => {
    const current = requireSession();
    const keyword = groupSearchKeyword.trim();
    if (!keyword) throw new Error('请输入群名称或群号');
    const results = await http.searchGroups(current.accessToken, keyword);
    setSearchedGroups(results);
    setSelectedGroupDetail(null);
    if (results.length > 0) {
      await selectGroupInfo(results[0].group_id, current);
    }
  });

  const selectGroupInfo = async (groupId: number, current = requireSession()) => {
    const detail = await http.groupInfo(current.accessToken, groupId);
    setSelectedGroupId(detail.joined ? detail.group.group_id : null);
    setSelectedGroupDetail(detail);
    setMembers(detail.members);
  };

  const joinSelectedGroup = () => run('加入群聊', async () => {
    const current = requireSession();
    const id = selectedGroupDetail?.group.group_id;
    if (!id) throw new Error('请先搜索并选择一个群聊');
    await http.joinGroup(current.accessToken, id);
    await syncGroups(current);
    await selectGroupInfo(id, current);
  });

  const leaveSelectedGroup = () => run('退出群聊', async () => {
    const current = requireSession();
    const id = selectedGroupDetail?.group.group_id;
    if (!id || !selectedGroupDetail?.joined) throw new Error('请先打开已加入群聊的资料');
    await http.leaveGroup(current.accessToken, id);
    await syncGroups(current);
    await selectGroupInfo(id, current);
  });

  const openGroup = (group: GroupInfo) => {
    openConversation({
      type: 'group',
      id: String(group.group_id),
      title: group.name || groupTitle(group.group_id),
      groupId: group.group_id,
    });
  };

  const loadGroupHistory = () => run('刷新群消息', async () => {
    const current = requireSession();
    const id = activeConversation?.type === 'group'
      ? activeConversation.groupId
      : selectedGroupDetail?.group.group_id;
    if (!id) throw new Error('请先选择群聊');
    const messages = await http.groupHistory(current.accessToken, id);
    setGroupMessagesById((currentMessages) => ({ ...currentMessages, [id]: sortGroupMessages(messages) }));
  });

  const sendGroupMessage = () => run('发送群消息', async () => {
    const current = requireSession();
    const id = activeConversation?.type === 'group'
      ? activeConversation.groupId
      : selectedGroupDetail?.group.group_id;
    if (!id) throw new Error('请先选择群聊');
    if (!groupDraft.trim()) throw new Error('请输入消息内容');
    await http.sendGroupMessage(current.accessToken, id, groupDraft.trim());
    setGroupDraft('');
    const messages = await http.groupHistory(current.accessToken, id);
    setGroupMessagesById((currentMessages) => ({ ...currentMessages, [id]: sortGroupMessages(messages) }));
  });

  useEffect(() => {
    if (!session) return;
    let cancelled = false;
    const current = session;

    Promise.all([
      http.friends(current.accessToken),
      http.pendingFriends(current.accessToken),
      http.groups(current.accessToken),
      http.offlineMessages(current.accessToken),
    ])
      .then(([friendList, pending, groupList, offline]) => {
        if (cancelled) return;
        setFriends(friendList);
        setPendingFriends(pending);
        setGroups(groupList);
        setDirectMessages((messages) => mergeDirectMessages(messages, offline));
        offline.forEach((message) => {
          const peer = message.sender_uid === current.profile.uid ? message.receiver_uid : message.sender_uid;
          addDirectConversation(peer);
        });
      })
      .catch((error) => {
        handleAsyncError(error, cancelled);
      });

    const status = ws.status();
    if (status !== 'connected' && status !== 'connecting') {
      ws.connect(current.accessToken);
    }

    return () => {
      cancelled = true;
    };
  }, [session?.accessToken, http, ws]);

  useEffect(() => {
    if (!session || view !== 'friends') return;
    let cancelled = false;
    const current = session;
    const sync = () => {
      Promise.all([
        http.friends(current.accessToken),
        http.pendingFriends(current.accessToken),
      ])
        .then(([friendList, pending]) => {
          if (cancelled) return;
          setFriends(friendList);
          setPendingFriends(pending);
        })
        .catch((error) => {
          handleAsyncError(error, cancelled);
        });
    };
    sync();
    const timer = window.setInterval(sync, 3000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [session?.accessToken, view, http]);

  useEffect(() => {
    if (!session || view !== 'groups') return;
    let cancelled = false;
    const current = session;
    const sync = () => {
      http.groups(current.accessToken)
        .then((groupList) => {
          if (!cancelled) setGroups(groupList);
        })
        .catch((error) => {
          handleAsyncError(error, cancelled);
        });
    };
    sync();
    const timer = window.setInterval(sync, 5000);
    return () => {
      cancelled = true;
      window.clearInterval(timer);
    };
  }, [session?.accessToken, view, http]);

  useEffect(() => {
    if (!session || view !== 'chats' || !activeConversation) return;
    let cancelled = false;
    const current = session;

    if (activeConversation.type === 'direct') {
      const peer = activeConversation.id;
      http.directHistory(current.accessToken, peer)
        .then((messages) => {
          if (cancelled) return;
          setDirectMessages((currentMessages) => replaceDirectMessagesForPeer(
            currentMessages,
            messages,
            peer,
          ));
          addDirectConversation(peer, activeConversation.title);
        })
        .catch((error) => {
          handleAsyncError(error, cancelled);
        });
    } else {
      const groupId = activeConversation.groupId;
      Promise.all([
        http.groupHistory(current.accessToken, groupId),
        http.groupMembers(current.accessToken, groupId).catch(() => [] as GroupMember[]),
      ])
        .then(([messages, groupMembers]) => {
          if (cancelled) return;
          setGroupMessagesById((currentMessages) => ({
            ...currentMessages,
            [groupId]: sortGroupMessages(messages),
          }));
          setMembers(groupMembers);
        })
        .catch((error) => {
          handleAsyncError(error, cancelled);
        });
    }

    return () => {
      cancelled = true;
    };
  }, [session?.accessToken, view, activeConversation, http]);

  if (!session) {
    return (
      <AuthScreen
        mode={authMode}
        form={authForm}
        notice={notice}
        setMode={setAuthMode}
        setForm={setAuthForm}
        onLogin={login}
        onRegister={register}
      />
    );
  }

  const sendCurrentMessage = activeConversation?.type === 'group'
    ? sendGroupMessage
    : transport === 'ws'
      ? sendDirectOverWs
      : sendDirect;
  const headerConversation = view === 'chats' ? activeConversation : null;
  const totalUnread = Object.values(unreadByConversation).reduce((sum, count) => sum + count, 0);

  return (
    <div className={debugOpen ? 'appShell debugOpen' : 'appShell'}>
      <Rail
        view={view}
        session={session}
        wsStatus={wsStatus}
        unreadCount={totalUnread}
        setView={setView}
        onOpenProfile={() => setView('profile')}
        onLogout={() => persistSession(null)}
      />

      {view === 'chats' && (
        <ConversationSidebar
          searchText={conversationSearch}
          items={conversationItems}
          activeConversation={activeConversation}
          friendsCount={friends.length}
          groupsCount={groups.length}
          setSearchText={setConversationSearch}
          onOpenConversation={openConversation}
          onPullOffline={pullOffline}
        />
      )}

      {view === 'friends' && (
        <FriendsSidebar
          searchText={friendSearch}
          items={friendItems}
          pending={pendingFriends}
          selectedUid={selectedFriendUid}
          setSearchText={setFriendSearch}
          targetUid={targetUid}
          setTargetUid={setTargetUid}
          searchedUsers={searchedUsers}
          selectedProfile={selectedProfile}
          onSearch={searchFriendTarget}
          onClearSearch={() => {
            setSearchedUsers([]);
            setSelectedProfile(null);
          }}
          onSelectProfile={(profile) => {
            setSelectedProfile(profile);
            setSelectedFriendUid('');
          }}
          onSelectFriend={(uid) => {
            setSelectedFriendUid(uid);
            setSelectedProfile(null);
          }}
          onRefresh={refreshFriends}
        />
      )}

      {view === 'groups' && (
        <GroupsSidebar
          searchText={groupSearch}
          items={groupItems}
          selectedGroupId={selectedGroupId}
          setSearchText={setGroupSearch}
          groupName={groupName}
          setGroupName={setGroupName}
          groupSearchKeyword={groupSearchKeyword}
          setGroupSearchKeyword={setGroupSearchKeyword}
          searchedGroups={searchedGroups}
          selectedGroupDetail={selectedGroupDetail}
          onCreate={createGroup}
          onSearch={searchGroupTarget}
          onSelectGroup={(groupId) => {
            const current = session;
            if (!current) return;
            selectGroupInfo(groupId, current).catch((error) => {
              handleAsyncError(error);
            });
          }}
          onRefresh={refreshGroups}
        />
      )}

      {(view === 'profile' || view === 'settings' || view === 'debug') && (
        <AccountSidebar
          session={session}
          wsStatus={wsStatus}
          httpCount={httpEvents.length}
          wsCount={wsEvents.length}
        />
      )}

      <main className="mainPane">
        <header className="topBar">
          <div className="chatIdentity">
            <span className={headerConversation ? 'presenceDot' : 'presenceDot neutral'} />
            <div>
              <h1>{headerConversation?.title ?? titleForView(view)}</h1>
              <p>{subtitleForView(view, headerConversation, session)}</p>
            </div>
          </div>
          <div className="topActions">
            <NoticePill notice={notice} />
            <IconButton
              label={debugOpen ? '收起调试面板' : '打开调试面板'}
              active={debugOpen}
              onClick={() => setDebugOpen((open) => !open)}
            >
              <PanelRightOpen size={18} />
            </IconButton>
          </div>
        </header>

        {view === 'chats' && (
          <ChatPanel
            session={session}
            conversation={activeConversation}
            directMessages={visibleDirectMessages}
            groupMessages={visibleGroupMessages}
            messageDraft={messageDraft}
            groupDraft={groupDraft}
            sendShortcut={settings.sendShortcut}
            setMessageDraft={setMessageDraft}
            setGroupDraft={setGroupDraft}
            onSend={sendCurrentMessage}
            onLoadDirect={loadDirectHistory}
            onPullOffline={pullOffline}
            onLoadGroup={loadGroupHistory}
          />
        )}

        {view === 'friends' && (
          <FriendsPanel
            session={session}
            selectedProfile={selectedProfile}
            selectedFriend={selectedFriend}
            selectedPending={selectedPendingFriend}
            friends={friends}
            onRefresh={refreshFriends}
            onRequest={sendFriendRequest}
            onAccept={(id) => respondFriend(true, id)}
            onReject={(id) => respondFriend(false, id)}
            openDirect={openDirect}
          />
        )}

        {view === 'groups' && (
          <GroupsPanel
            members={members}
            selectedGroupDetail={selectedGroupDetail}
            onRefresh={refreshGroups}
            onJoin={joinSelectedGroup}
            onLeave={leaveSelectedGroup}
            openGroup={openGroup}
          />
        )}

        {view === 'profile' && (
          <ProfilePanel
            session={session}
            wsStatus={wsStatus}
            onProfile={loadProfile}
            onSave={updateProfile}
          />
        )}

        {view === 'settings' && (
          <SettingsPanel
            session={session}
            settings={settings}
            wsStatus={wsStatus}
            setSettings={persistSettings}
            onProfile={loadProfile}
            onConnect={() => ws.connect(token)}
            onDisconnect={() => ws.disconnect()}
          />
        )}

        {view === 'debug' && (
          <DebugPanel
            settings={settings}
            setSettings={persistSettings}
            session={session}
            httpEvents={httpEvents}
            wsEvents={wsEvents}
            wsStatus={wsStatus}
            transport={transport}
            wsProbe={wsProbe}
            setTransport={setTransport}
            setWsProbe={setWsProbe}
            onConnect={() => ws.connect(token)}
            onDisconnect={() => ws.disconnect()}
            onProbe={() => ws.sendTextForProbe(wsProbe)}
          />
        )}
      </main>

      {debugOpen && (
        <aside className="debugDrawer">
          <DebugPanel
            compact
            settings={settings}
            setSettings={persistSettings}
            session={session}
            httpEvents={httpEvents}
            wsEvents={wsEvents}
            wsStatus={wsStatus}
            transport={transport}
            wsProbe={wsProbe}
            setTransport={setTransport}
            setWsProbe={setWsProbe}
            onConnect={() => ws.connect(token)}
            onDisconnect={() => ws.disconnect()}
            onProbe={() => ws.sendTextForProbe(wsProbe)}
          />
        </aside>
      )}
    </div>
  );
}

function AuthScreen(props: {
  mode: AuthMode;
  form: AuthFormState;
  notice: Notice;
  setMode: (mode: AuthMode) => void;
  setForm: (form: AuthFormState) => void;
  onLogin: () => void;
  onRegister: () => void;
}) {
  const submit = props.mode === 'register' ? props.onRegister : props.onLogin;

  return (
    <main className="authShell">
      <section className="authBox">
        <div className="authBrand">
          <div className="brandMark">
            <MessageCircle size={30} />
          </div>
          <h1>MyChat</h1>
          <p>登录后继续使用 MyChat</p>
        </div>
        <div className="authTabs" role="tablist" aria-label="认证模式">
          <button className={props.mode === 'login' ? 'active' : ''} onClick={() => props.setMode('login')} type="button">
            登录
          </button>
          <button className={props.mode === 'register' ? 'active' : ''} onClick={() => props.setMode('register')} type="button">
            注册
          </button>
        </div>
        <form className="authForm" onSubmit={(event) => {
          event.preventDefault();
          submit();
        }}>
          <label>
            账号
            <input
              autoComplete="username"
              placeholder="请输入账号"
              value={props.form.account}
              onChange={(event) => props.setForm({ ...props.form, account: event.target.value })}
            />
          </label>
          <label>
            密码
            <input
              autoComplete={props.mode === 'register' ? 'new-password' : 'current-password'}
              placeholder="请输入密码"
              type="password"
              value={props.form.password}
              onChange={(event) => props.setForm({ ...props.form, password: event.target.value })}
            />
          </label>
          {props.mode === 'register' && (
            <label>
              昵称
              <input
                placeholder="请输入昵称"
                value={props.form.nickname}
                onChange={(event) => props.setForm({ ...props.form, nickname: event.target.value })}
              />
            </label>
          )}
          <button className="primaryButton" type="submit">
            {props.mode === 'register' ? '创建账号' : '登录'}
          </button>
        </form>
        <NoticePill notice={props.notice} />
      </section>
    </main>
  );
}

function Rail({ view, session, wsStatus, unreadCount, setView, onOpenProfile, onLogout }: {
  view: ViewMode;
  session: AuthSession;
  wsStatus: string;
  unreadCount: number;
  setView: (view: ViewMode) => void;
  onOpenProfile: () => void;
  onLogout: () => void;
}) {
  const items: Array<{ key: ViewMode; label: string; icon: React.ReactNode }> = [
    { key: 'chats', label: '消息', icon: <MessageCircle size={21} /> },
    { key: 'friends', label: '联系人', icon: <Contact size={21} /> },
    { key: 'groups', label: '群聊', icon: <Users size={21} /> },
    { key: 'settings', label: '设置', icon: <SettingsIcon size={21} /> },
    { key: 'debug', label: '开发', icon: <Bug size={21} /> },
  ];

  return (
    <nav className="rail" aria-label="主导航">
      <button className="avatarButton" title="个人主页" type="button" onClick={onOpenProfile}>
        <UserAvatar profile={session.profile} large />
      </button>
      <div className="railItems">
        {items.map((item) => (
          <button
            key={item.key}
            className={view === item.key ? 'active railButton' : 'railButton'}
            title={item.label}
            onClick={() => setView(item.key)}
            type="button"
          >
            {item.icon}
            <span>{item.label}</span>
            {item.key === 'chats' && unreadCount > 0 && (
              <i className="railUnread">{unreadCount > 99 ? '99+' : unreadCount}</i>
            )}
          </button>
        ))}
      </div>
      <div className={wsStatus === 'connected' ? 'wsBadge connected' : 'wsBadge'} title={wsStatus}>
        {wsStatus === 'connected' ? <Wifi size={18} /> : <WifiOff size={18} />}
      </div>
      <button className="logoutButton" title="退出登录" onClick={onLogout} type="button">
        <LogOut size={20} />
      </button>
    </nav>
  );
}

function ConversationSidebar(props: {
  searchText: string;
  items: ConversationListItem[];
  activeConversation: Conversation | null;
  friendsCount: number;
  groupsCount: number;
  setSearchText: (value: string) => void;
  onOpenConversation: (conversation: Conversation) => void;
  onPullOffline: () => void;
}) {
  return (
    <aside className="sidePane">
      <header className="sideHeader">
        <div>
          <h2>消息</h2>
          <p>{props.friendsCount} 位联系人 / {props.groupsCount} 个群聊</p>
        </div>
        <IconButton label="同步收件箱" onClick={props.onPullOffline}>
          <RefreshCw size={18} />
        </IconButton>
      </header>
      <div className="searchBox">
        <Search size={17} />
        <input
          placeholder="搜索"
          value={props.searchText}
          onChange={(event) => props.setSearchText(event.target.value)}
        />
      </div>
      <div className="conversationList">
        {props.items.map((item) => (
          <button
            key={item.key}
            className={isSameConversation(props.activeConversation, item.conversation) ? 'selected' : ''}
            onClick={() => props.onOpenConversation(item.conversation)}
            type="button"
          >
            <div className={item.type === 'group' ? 'conversationIcon group' : 'conversationIcon'}>
              {item.type === 'group' ? <Users size={17} /> : <MessageCircle size={17} />}
            </div>
            <div>
              <strong>{item.title}</strong>
              <span>{item.subtitle}</span>
            </div>
            {item.unread > 0 && (
              <i className="unreadBadge">{item.unread > 99 ? '99+' : item.unread}</i>
            )}
          </button>
        ))}
        {!props.items.length && <div className="emptyList">暂无会话</div>}
      </div>
    </aside>
  );
}

function FriendsSidebar(props: {
  searchText: string;
  items: FriendListItem[];
  pending: FriendInfo[];
  selectedUid: string;
  setSearchText: (value: string) => void;
  targetUid: string;
  setTargetUid: (value: string) => void;
  searchedUsers: Profile[];
  selectedProfile: Profile | null;
  onSearch: () => void;
  onClearSearch: () => void;
  onSelectProfile: (profile: Profile) => void;
  onSelectFriend: (uid: string) => void;
  onRefresh: () => void;
}) {
  return (
    <aside className="sidePane">
      <header className="sideHeader">
        <div>
          <h2>联系人</h2>
          <p>{props.items.length} 位好友 / {props.pending.length} 个申请</p>
        </div>
        <IconButton label="刷新联系人" onClick={props.onRefresh}>
          <RefreshCw size={18} />
        </IconButton>
      </header>
      <div className="searchBox">
        <Search size={17} />
        <input
          placeholder="搜索联系人"
          value={props.searchText}
          onChange={(event) => props.setSearchText(event.target.value)}
        />
      </div>
      <div className="conversationList">
        <form className="sideSearchForm" onSubmit={(event) => {
          event.preventDefault();
          props.onSearch();
        }}>
          <input
            placeholder="搜索账号或昵称"
            value={props.targetUid}
            onChange={(event) => {
              props.setTargetUid(event.target.value);
              props.onClearSearch();
            }}
          />
          <button type="submit" aria-label="搜索用户" title="搜索用户">
            <Search size={16} />
          </button>
        </form>
        {props.searchedUsers.length > 0 && (
          <div className="listSectionLabel">搜索结果</div>
        )}
        {props.searchedUsers.map((profile) => (
          <button
            key={`profile-${profile.uid}`}
            className={props.selectedProfile?.uid === profile.uid ? 'selected' : ''}
            type="button"
            onClick={() => props.onSelectProfile(profile)}
          >
            <UserAvatar profile={profile} />
            <div>
              <strong>{profile.nickname || profile.account}</strong>
              <span>@{profile.account}</span>
            </div>
          </button>
        ))}
        {props.pending.length > 0 && (
          <div className="listSectionLabel">好友申请</div>
        )}
        {props.pending.map((item) => (
          <button
            key={`pending-${item.friend_id}`}
            type="button"
            onClick={() => props.onSelectFriend(item.friend_uid)}
          >
            <div className="conversationIcon pending">
              <UserPlus size={17} />
            </div>
            <div>
              <strong>{item.nickname || '新的好友申请'}</strong>
              <span>等待处理</span>
            </div>
          </button>
        ))}
        {props.items.length > 0 && <div className="listSectionLabel">我的好友</div>}
        {props.items.map((item) => (
          <button
            key={item.key}
            className={props.selectedUid === item.friend.friend_uid ? 'selected' : ''}
            onClick={() => props.onSelectFriend(item.friend.friend_uid)}
            type="button"
          >
            <div className="conversationIcon">
              <Contact size={17} />
            </div>
            <div>
              <strong>{item.title}</strong>
              <span>{item.subtitle}</span>
            </div>
          </button>
        ))}
        {!props.items.length && !props.pending.length && <div className="emptyList">暂无联系人</div>}
      </div>
    </aside>
  );
}

function GroupsSidebar(props: {
  searchText: string;
  items: GroupListItem[];
  selectedGroupId: number | null;
  setSearchText: (value: string) => void;
  groupName: string;
  setGroupName: (value: string) => void;
  groupSearchKeyword: string;
  setGroupSearchKeyword: (value: string) => void;
  searchedGroups: GroupSearchResult[];
  selectedGroupDetail: GroupInfoDetail | null;
  onCreate: () => void;
  onSearch: () => void;
  onSelectGroup: (groupId: number) => void;
  onRefresh: () => void;
}) {
  return (
    <aside className="sidePane">
      <header className="sideHeader">
        <div>
          <h2>群聊</h2>
          <p>{props.items.length} 个群聊</p>
        </div>
        <IconButton label="刷新群聊" onClick={props.onRefresh}>
          <RefreshCw size={18} />
        </IconButton>
      </header>
      <div className="searchBox">
        <Search size={17} />
        <input
          placeholder="搜索群聊"
          value={props.searchText}
          onChange={(event) => props.setSearchText(event.target.value)}
        />
      </div>
      <div className="conversationList">
        <form className="sideSearchForm" onSubmit={(event) => {
          event.preventDefault();
          props.onSearch();
        }}>
          <input
            placeholder="搜索群名称或群号"
            value={props.groupSearchKeyword}
            onChange={(event) => props.setGroupSearchKeyword(event.target.value)}
          />
          <button type="submit" aria-label="搜索群聊" title="搜索群聊">
            <Search size={16} />
          </button>
        </form>
        {props.searchedGroups.length > 0 && (
          <div className="listSectionLabel">搜索结果</div>
        )}
        {props.searchedGroups.map((group) => (
          <button
            key={`search-group-${group.group_id}`}
            className={props.selectedGroupDetail?.group.group_id === group.group_id ? 'selected' : ''}
            onClick={() => props.onSelectGroup(group.group_id)}
            type="button"
          >
            <div className="conversationIcon group">
              <Users size={17} />
            </div>
            <div>
              <strong>{group.name || groupTitle(group.group_id)}</strong>
              <span>{group.joined ? '已加入' : `${group.member_count} 位成员`}</span>
            </div>
          </button>
        ))}
        <form className="sideSearchForm" onSubmit={(event) => {
          event.preventDefault();
          props.onCreate();
        }}>
          <input
            placeholder="创建群聊"
            value={props.groupName}
            onChange={(event) => props.setGroupName(event.target.value)}
          />
          <button type="submit" aria-label="创建群聊" title="创建群聊">
            <Plus size={16} />
          </button>
        </form>
        {props.items.length > 0 && <div className="listSectionLabel">我的群聊</div>}
        {props.items.map((item) => (
          <button
            key={item.key}
            className={props.selectedGroupId === item.group.group_id ? 'selected' : ''}
            onClick={() => props.onSelectGroup(item.group.group_id)}
            type="button"
          >
            <div className="conversationIcon group">
              <Users size={17} />
            </div>
            <div>
              <strong>{item.title}</strong>
              <span>{item.subtitle}</span>
            </div>
          </button>
        ))}
        {!props.items.length && <div className="emptyList">暂无群聊</div>}
      </div>
    </aside>
  );
}

function AccountSidebar(props: {
  session: AuthSession;
  wsStatus: string;
  httpCount: number;
  wsCount: number;
}) {
  return (
    <aside className="sidePane accountSidePane">
      <header className="sideHeader">
        <div>
          <h2>{props.session.profile.nickname || props.session.profile.account}</h2>
          <p>@{props.session.profile.account}</p>
        </div>
      </header>
      <div className="profilePanel compact">
        <UserAvatar profile={props.session.profile} />
        <div>
          <h3>{props.session.profile.nickname || props.session.profile.account}</h3>
          <p>@{props.session.profile.account}</p>
        </div>
      </div>
      <div className="settingsList compact">
        <article>
          <div>
            <strong>连接</strong>
            <span>{wsStatusLabel(props.wsStatus)}</span>
          </div>
          <div className={props.wsStatus === 'connected' ? 'statusLight online' : 'statusLight'} />
        </article>
        <article>
          <div>
            <strong>调试事件</strong>
            <span>HTTP {props.httpCount} / WS {props.wsCount}</span>
          </div>
        </article>
      </div>
    </aside>
  );
}

function ChatPanel(props: {
  session: AuthSession;
  conversation: Conversation | null;
  directMessages: DirectMessage[];
  groupMessages: GroupMessage[];
  messageDraft: string;
  groupDraft: string;
  sendShortcut: Settings['sendShortcut'];
  setMessageDraft: (value: string) => void;
  setGroupDraft: (value: string) => void;
  onSend: () => void;
  onLoadDirect: () => void;
  onPullOffline: () => void;
  onLoadGroup: () => void;
}) {
  const isGroup = props.conversation?.type === 'group';
  const draft = isGroup ? props.groupDraft : props.messageDraft;
  const setDraft = isGroup ? props.setGroupDraft : props.setMessageDraft;
  const messages = isGroup ? props.groupMessages : props.directMessages;

  return (
    <section className="chatPanel">
      <div className="chatToolbar">
        {props.conversation ? (
          <>
            <button type="button" onClick={isGroup ? props.onLoadGroup : props.onLoadDirect}>
              <RefreshCw size={16} />
              刷新
            </button>
            {!isGroup && (
              <button type="button" onClick={props.onPullOffline}>
                <Circle size={14} />
                收件箱
              </button>
            )}
          </>
        ) : (
          <span className="toolbarHint">从左侧选择会话，或发起一个新的聊天。</span>
        )}
      </div>
      <div className="messageList">
        {!props.conversation && (
          <div className="emptyChat">
            <MessageCircle size={38} />
            <h2>选择一个会话</h2>
            <p>选择联系人或群聊后开始聊天。</p>
          </div>
        )}
        {props.conversation && !messages.length && (
          <div className="emptyChat slim">
            <h2>还没有消息</h2>
            <p>发送一条消息，或刷新当前会话。</p>
          </div>
        )}
        {!isGroup && props.directMessages.map((message) => (
          <MessageBubble
            key={`${message.msg_id}-${message.create_time}-${message.sender_uid}`}
            own={message.sender_uid === props.session.profile.uid}
            title={message.sender_uid === props.session.profile.uid ? '我' : '对方'}
            content={message.content}
            meta={messageStatusLabel(message.status)}
          />
        ))}
        {isGroup && props.groupMessages.map((message) => (
          <MessageBubble
            key={`${message.msg_id}-${message.created_at}-${message.sender_uid}`}
            own={message.sender_uid === props.session.profile.uid}
            title={message.sender_nickname || '群成员'}
            content={message.content}
            meta="群消息"
          />
        ))}
      </div>
      <form className="composer" onSubmit={(event) => {
        event.preventDefault();
        if (props.conversation) props.onSend();
      }}>
        <textarea
          disabled={!props.conversation}
          value={draft}
          onChange={(event) => setDraft(event.target.value)}
          placeholder={props.conversation ? '输入消息' : '请先选择一个会话'}
          onKeyDown={(event) => {
            if (!props.conversation) return;
            const shouldSend = props.sendShortcut === 'enter'
              ? event.key === 'Enter' && !event.shiftKey && !event.ctrlKey && !event.metaKey
              : event.key === 'Enter' && (event.ctrlKey || event.metaKey);
            if (!shouldSend) return;
            event.preventDefault();
            if (draft.trim()) props.onSend();
          }}
        />
        <button className="sendButton" disabled={!props.conversation || !draft.trim()} title="发送" type="submit">
          <Send size={20} />
        </button>
      </form>
    </section>
  );
}

function MessageBubble(props: { own: boolean; title: string; content: string; meta: string }) {
  return (
    <article className={props.own ? 'messageBubble own' : 'messageBubble'}>
      {!props.own && <UserAvatar name={props.title} />}
      <div className="messageBody">
        <span>{props.title}</span>
        <p>{props.content}</p>
        <small>{props.meta}</small>
      </div>
      {props.own && <UserAvatar name={props.title} />}
    </article>
  );
}

function FriendsPanel(props: {
  session: AuthSession;
  selectedProfile: Profile | null;
  selectedFriend: FriendInfo | null;
  selectedPending: FriendInfo | null;
  friends: FriendInfo[];
  onRefresh: () => void;
  onRequest: () => void;
  onAccept: (id: number) => void;
  onReject: (id: number) => void;
  openDirect: (uid?: string) => void;
}) {
  const searchedIsSelf = props.selectedProfile?.uid === props.session.profile.uid;
  const searchedIsFriend = Boolean(
    props.selectedProfile && props.friends.some((friend) => friend.friend_uid === props.selectedProfile?.uid),
  );

  return (
    <section className="workPanel profileWorkPanel">
      <div className="sectionTitle">
        <div>
          <h2>{props.selectedProfile || props.selectedFriend ? '联系人资料' : '联系人'}</h2>
          <p>从中间栏选择好友、申请或搜索结果后查看资料。</p>
        </div>
        <button type="button" onClick={props.onRefresh}>
          <RefreshCw size={16} />
          刷新
        </button>
      </div>

      {props.selectedProfile && (
        <ProfileDetail
          profile={props.selectedProfile}
          badge={searchedIsSelf ? '这是你' : searchedIsFriend ? '已是好友' : '搜索结果'}
          actions={(
            <button
              disabled={searchedIsSelf || searchedIsFriend}
              type="button"
              onClick={props.onRequest}
            >
              <UserPlus size={16} />
              {searchedIsSelf ? '这是你' : searchedIsFriend ? '已是好友' : '添加好友'}
            </button>
          )}
        />
      )}

      {!props.selectedProfile && props.selectedPending && (
        <ProfileDetail
          name={props.selectedPending.nickname || '新的好友申请'}
          subtitle="等待你处理"
          description="对方请求添加你为好友"
          badge="好友申请"
          actions={(
            <>
              <button type="button" onClick={() => props.onAccept(props.selectedPending!.friend_id)}>
                <Check size={16} />
                同意
              </button>
              <button type="button" onClick={() => props.onReject(props.selectedPending!.friend_id)}>
                <X size={16} />
                拒绝
              </button>
            </>
          )}
        />
      )}

      {!props.selectedProfile && !props.selectedPending && props.selectedFriend && (
        <ProfileDetail
          name={props.selectedFriend.nickname || '联系人'}
          subtitle="已添加到联系人"
          description="可直接发起聊天"
          badge="好友"
          actions={(
            <button type="button" onClick={() => props.openDirect(props.selectedFriend?.friend_uid)}>
              <MessageCircle size={16} />
              发消息
            </button>
          )}
        />
      )}

      {!props.selectedProfile && !props.selectedPending && !props.selectedFriend && (
        <div className="emptyChat inline">
          <Contact size={34} />
          <h2>选择一个联系人</h2>
          <p>好友、好友申请和搜索结果都在中间栏，右侧只展示资料。</p>
        </div>
      )}
    </section>
  );
}

function GroupsPanel(props: {
  members: GroupMember[];
  selectedGroupDetail: GroupInfoDetail | null;
  onRefresh: () => void;
  onJoin: () => void;
  onLeave: () => void;
  openGroup: (group: GroupInfo) => void;
}) {
  const selectedGroup = props.selectedGroupDetail?.group ?? null;
  const joined = props.selectedGroupDetail?.joined ?? false;

  return (
    <section className="workPanel profileWorkPanel">
      <div className="sectionTitle">
        <div>
          <h2>{selectedGroup ? '群资料' : '群聊'}</h2>
          <p>从中间栏选择群聊或搜索结果后查看资料。</p>
        </div>
        <button type="button" onClick={props.onRefresh}>
          <RefreshCw size={16} />
          刷新
        </button>
      </div>

      {selectedGroup ? (
        <GroupDetail
          group={selectedGroup}
          joined={joined}
          members={props.members}
          onJoin={props.onJoin}
          onLeave={props.onLeave}
          openGroup={props.openGroup}
        />
      ) : (
        <div className="emptyChat inline">
          <Users size={34} />
          <h2>选择一个群聊</h2>
          <p>群列表、搜索和创建都在中间栏，右侧只展示群资料。</p>
        </div>
      )}
    </section>
  );
}

function ProfileDetail(props: {
  profile?: Profile;
  name?: string;
  subtitle?: string;
  description?: string;
  badge?: string;
  actions?: React.ReactNode;
}) {
  const name = props.profile?.nickname || props.profile?.account || props.name || '联系人';
  const subtitle = props.profile ? `@${props.profile.account}` : props.subtitle;
  const description = props.profile?.signature || props.description || '这个人还没有填写签名';

  return (
    <section className="profileCard">
      <div className="profileHero">
        <UserAvatar profile={props.profile} name={name} large />
        <div className="profileHeroText">
          <h2>{name}</h2>
          {subtitle && <p>{subtitle}</p>}
          <span>{props.badge || '资料'}</span>
        </div>
      </div>
      <div className="profileSummary">{description}</div>
      <div className="profileInfoList">
        {props.profile && (
          <>
            <InfoRow label="账号" value={`@${props.profile.account}`} />
            <InfoRow label="性别" value={genderLabel(props.profile.gender)} />
          </>
        )}
      </div>
      {props.actions && <div className="profileActions">{props.actions}</div>}
    </section>
  );
}

function GroupDetail(props: {
  group: GroupInfo;
  joined: boolean;
  members: GroupMember[];
  onJoin: () => void;
  onLeave: () => void;
  openGroup: (group: GroupInfo) => void;
}) {
  const groupName = props.group.name || groupTitle(props.group.group_id);
  const [myGroupNickname, setMyGroupNickname] = useState('');
  const [groupRemark, setGroupRemark] = useState('');
  const [pinned, setPinned] = useState(false);
  const [muted, setMuted] = useState(false);
  const [notifyMode, setNotifyMode] = useState('接收消息但不提醒');
  const previewMembers = props.members.slice(0, 15);

  return (
    <section className="profileCard imDetailPanel">
      <div className="groupProfileHeader">
        <GroupAvatar large />
        <div>
          <h2>{groupName}</h2>
          <p>{props.group.group_id}</p>
        </div>
        <button className="compactActionButton" type="button">
          <Share2 size={15} />
          分享
        </button>
      </div>

      <section className="imSection">
        <button className="sectionHeaderButton" type="button">
          <strong>群聊成员</strong>
          <span>查看{props.group.member_count}名群成员 <ChevronRight size={15} /></span>
        </button>
        {props.joined ? (
          <div className="memberGrid">
            {previewMembers.map((member) => (
              <article key={member.id}>
                <UserAvatar name={member.nickname || '群成员'} />
                <span>{member.nickname || '群成员'}</span>
              </article>
            ))}
            {!previewMembers.length && <div className="emptyList">暂无成员数据</div>}
          </div>
        ) : (
          <div className="profileHint">加入后可以查看群成员并进入群聊。</div>
        )}
      </section>

      <section className="imSection">
        <h3>群公告</h3>
        <button className="settingRow" type="button">
          <span>暂无群公告</span>
          <ChevronRight size={15} />
        </button>
      </section>

      <section className="imSection">
        <h3>我的本群昵称</h3>
        <input
          className="settingInput"
          value={myGroupNickname}
          onChange={(event) => setMyGroupNickname(event.target.value)}
          placeholder="填写本群昵称"
        />
      </section>

      <section className="imSection">
        <h3>群聊备注</h3>
        <input
          className="settingInput"
          value={groupRemark}
          onChange={(event) => setGroupRemark(event.target.value)}
          placeholder="填写备注"
        />
      </section>

      <section className="imSection">
        <SettingSwitchRow label="设为置顶" checked={pinned} onChange={setPinned} />
        <SettingSwitchRow label="消息免打扰" checked={muted} onChange={setMuted} />
      </section>

      <section className="imSection">
        <h3>群消息设置</h3>
        <select
          className="settingInput"
          value={notifyMode}
          onChange={(event) => setNotifyMode(event.target.value)}
        >
          <option>接收消息但不提醒</option>
          <option>接收并提醒</option>
          <option>屏蔽群消息</option>
        </select>
      </section>

      <div className="profileInfoList">
        <InfoRow label="群号" value={props.group.group_id} />
        <InfoRow label="群状态" value={props.joined ? '已加入群聊' : '搜索结果，尚未加入'} />
      </div>

      <div className="profileActions">
        {props.joined ? (
          <>
            <button type="button" onClick={() => props.openGroup(props.group)}>
              <MessageCircle size={16} />
              进入群聊
            </button>
            <button className="dangerButton" type="button" onClick={props.onLeave}>退出群聊</button>
          </>
        ) : (
          <button type="button" onClick={props.onJoin}>
            <UserPlus size={16} />
            加入群聊
          </button>
        )}
      </div>

      {props.joined && (
        <button className="plainDangerRow" type="button">
          <Trash2 size={15} />
          删除聊天记录
        </button>
      )}
    </section>
  );
}

function ProfilePanel(props: {
  session: AuthSession;
  wsStatus: string;
  onProfile: () => void;
  onSave: (form: ProfileFormState) => Promise<boolean>;
}) {
  const [editing, setEditing] = useState(false);
  const [form, setForm] = useState<ProfileFormState>(() => profileToForm(props.session.profile));

  useEffect(() => {
    setForm(profileToForm(props.session.profile));
  }, [props.session.profile]);

  const previewProfile: Profile = {
    ...props.session.profile,
    nickname: form.nickname.trim() || props.session.profile.nickname,
    avatar: form.avatar.trim(),
    gender: form.gender,
    signature: form.signature.trim(),
  };

  const save = async () => {
    const ok = await props.onSave(form);
    if (ok) setEditing(false);
  };

  const loadAvatarFile = (file: File | null) => {
    if (!file) return;
    if (!file.type.startsWith('image/')) return;
    const reader = new FileReader();
    reader.onload = () => {
      if (typeof reader.result === 'string') {
        setForm((current) => ({ ...current, avatar: reader.result as string }));
      }
    };
    reader.readAsDataURL(file);
  };

  return (
    <section className="workPanel profileWorkPanel">
      <div className="sectionTitle">
        <div>
          <h2>个人主页</h2>
          <p>{editing ? '编辑昵称、头像和个性签名。' : '查看账号资料和在线状态。'}</p>
        </div>
        {!editing && (
          <button type="button" onClick={props.onProfile}>
            <RefreshCw size={16} />
            刷新
          </button>
        )}
      </div>

      {editing ? (
        <section className="profileCard imDetailPanel">
          <div className="profileEditAvatar">
            <UserAvatar profile={previewProfile} large />
          </div>
          <div className="profileEditRows">
            <label className="editFieldRow">
              <span>昵称</span>
              <input
                maxLength={36}
                value={form.nickname}
                onChange={(event) => setForm({ ...form, nickname: event.target.value })}
                placeholder="输入昵称"
              />
              <small>{form.nickname.length}/36</small>
            </label>
            <label className="editFieldRow">
              <span>签名</span>
              <input
                maxLength={80}
                value={form.signature}
                onChange={(event) => setForm({ ...form, signature: event.target.value })}
                placeholder="填写签名"
              />
              <small>{form.signature.length}/80</small>
            </label>
            <label className="editFieldRow">
              <span>性别</span>
              <select
                value={form.gender}
                onChange={(event) => setForm({ ...form, gender: Number(event.target.value) })}
              >
                <option value={0}>保密</option>
                <option value={1}>男</option>
                <option value={2}>女</option>
                <option value={3}>其他</option>
              </select>
            </label>
            <label className="editFieldRow">
              <span>头像</span>
              <input
                value={form.avatar}
                onChange={(event) => setForm({ ...form, avatar: event.target.value })}
                placeholder="图片 URL 或 base64"
              />
            </label>
            <div className="avatarUploadRow">
              <label>
                选择图片
                <input
                  accept="image/*"
                  type="file"
                  onChange={(event) => loadAvatarFile(event.target.files?.[0] ?? null)}
                />
              </label>
              <button type="button" onClick={() => setForm({ ...form, avatar: '' })}>
                清除头像
              </button>
            </div>
          </div>
          <div className="profileActions">
            <button type="button" onClick={save}>
              <Check size={16} />
              保存
            </button>
            <button
              type="button"
              className="ghostButton"
              onClick={() => {
                setForm(profileToForm(props.session.profile));
                setEditing(false);
              }}
            >
              取消
            </button>
          </div>
        </section>
      ) : (
        <section className="profileCard">
          <div className="profileHero">
            <UserAvatar profile={props.session.profile} large />
            <div className="profileHeroText">
              <h2>{props.session.profile.nickname || props.session.profile.account}</h2>
              <p>@{props.session.profile.account}</p>
              <span>{props.wsStatus === 'connected' ? '在线' : '离线'}</span>
            </div>
          </div>
          <div className="profileSummary">
            {props.session.profile.signature || '这个人还没有填写签名'}
          </div>
          <div className="profileInfoList">
            <InfoRow label="账号" value={`@${props.session.profile.account}`} />
            <InfoRow label="性别" value={genderLabel(props.session.profile.gender)} />
            <InfoRow label="在线状态" value={props.wsStatus === 'connected' ? '在线' : '离线'} />
          </div>
          <div className="profileActions">
            <button type="button" onClick={() => setEditing(true)}>
              <Edit3 size={16} />
              编辑资料
            </button>
          </div>
        </section>
      )}
    </section>
  );
}

function InfoRow(props: { label: string; value: React.ReactNode }) {
  return (
    <article className="infoRow">
      <strong>{props.label}</strong>
      <span>{props.value}</span>
    </article>
  );
}

function SettingSwitchRow(props: {
  label: string;
  checked: boolean;
  onChange: (checked: boolean) => void;
}) {
  return (
    <label className="switchRow">
      <span>{props.label}</span>
      <input
        type="checkbox"
        checked={props.checked}
        onChange={(event) => props.onChange(event.target.checked)}
      />
      <i />
    </label>
  );
}

function SettingsPanel(props: {
  session: AuthSession;
  settings: Settings;
  wsStatus: string;
  setSettings: (settings: Settings) => void;
  onProfile: () => void;
  onConnect: () => void;
  onDisconnect: () => void;
}) {
  return (
    <section className="workPanel settingsPanel">
      <div className="sectionTitle">
        <div>
          <h2>设置</h2>
          <p>{props.session.profile.nickname || props.session.profile.account}</p>
        </div>
        <button type="button" onClick={props.onProfile}>
          <RefreshCw size={16} />
          刷新
        </button>
      </div>
      <div className="profilePanel">
        <UserAvatar profile={props.session.profile} large />
        <div>
          <h3>{props.session.profile.nickname || props.session.profile.account}</h3>
          <p>@{props.session.profile.account}</p>
          <span>{props.session.profile.signature || '这个人还没有填写签名'}</span>
        </div>
      </div>
      <div className="settingsList">
        <article>
          <div>
            <strong>发送快捷键</strong>
            <span>{props.settings.sendShortcut === 'enter' ? 'Enter 发送，Shift+Enter 换行' : 'Ctrl+Enter 发送，Enter 换行'}</span>
          </div>
          <div className="segmentedControl">
            <button
              className={props.settings.sendShortcut === 'enter' ? 'active' : ''}
              type="button"
              onClick={() => props.setSettings({ ...props.settings, sendShortcut: 'enter' })}
            >
              Enter
            </button>
            <button
              className={props.settings.sendShortcut === 'ctrlEnter' ? 'active' : ''}
              type="button"
              onClick={() => props.setSettings({ ...props.settings, sendShortcut: 'ctrlEnter' })}
            >
              Ctrl+Enter
            </button>
          </div>
        </article>
        <article>
          <div>
            <strong>连接状态</strong>
            <span>{props.wsStatus === 'connected' ? '在线' : '未连接'}</span>
          </div>
          <div className={props.wsStatus === 'connected' ? 'statusLight online' : 'statusLight'} />
        </article>
        <article>
          <div>
            <strong>账号</strong>
            <span>@{props.session.profile.account}</span>
          </div>
        </article>
      </div>
      <div className="buttonRow">
        <button type="button" onClick={props.onConnect}>上线</button>
        <button type="button" onClick={props.onDisconnect}>离线</button>
      </div>
    </section>
  );
}

function DebugPanel(props: {
  compact?: boolean;
  settings: Settings;
  setSettings: (settings: Settings) => void;
  session: AuthSession | null;
  httpEvents: HttpEvent[];
  wsEvents: WsEvent[];
  wsStatus: string;
  transport: TransportMode;
  wsProbe: string;
  setTransport: (value: TransportMode) => void;
  setWsProbe: (value: string) => void;
  onConnect: () => void;
  onDisconnect: () => void;
  onProbe: () => void;
}) {
  const [draft, setDraft] = useState(props.settings);
  const [developerOpen, setDeveloperOpen] = useState(false);

  return (
    <section className={props.compact ? 'debugPanel compact' : 'debugPanel'}>
      <div className="sectionTitle">
        <div>
          <h2>开发者</h2>
          <p>后端联调和运行诊断。</p>
        </div>
      </div>
      <dl>
        <dt>HTTP</dt><dd>{props.settings.httpBaseUrl || '同源 /api 代理'}</dd>
        <dt>WS</dt><dd>{props.settings.wsUrl}</dd>
        <dt>用户</dt><dd>{props.session ? `${props.session.profile.account} / ${props.session.profile.uid}` : '无'}</dd>
        <dt>状态</dt><dd>{wsStatusLabel(props.wsStatus)}</dd>
      </dl>
      <div className="buttonRow">
        <button type="button" onClick={props.onConnect}>连接 WS</button>
        <button type="button" onClick={props.onDisconnect}>断开</button>
      </div>
      <div className="developerSwitch" aria-label="消息发送通道">
        <button
          className={props.transport === 'http' ? 'active' : ''}
          type="button"
          onClick={() => props.setTransport('http')}
        >
          使用 HTTP 发送
        </button>
        <button
          className={props.transport === 'ws' ? 'active' : ''}
          type="button"
          onClick={() => props.setTransport('ws')}
        >
          使用 WS 发送
        </button>
      </div>
      <div className="formStrip compact">
        <input value={props.wsProbe} onChange={(event) => props.setWsProbe(event.target.value)} />
        <button type="button" onClick={props.onProbe}>探测</button>
      </div>
      <button className="developerToggle" type="button" onClick={() => setDeveloperOpen((open) => !open)}>
        开发端点
      </button>
      {developerOpen && (
        <div className="endpointPanel">
          <label>
            HTTP 基础地址
            <input value={draft.httpBaseUrl} onChange={(event) => setDraft({ ...draft, httpBaseUrl: event.target.value })} />
          </label>
          <label>
            WebSocket 地址
            <input value={draft.wsUrl} onChange={(event) => setDraft({ ...draft, wsUrl: event.target.value })} />
          </label>
          <button type="button" onClick={() => props.setSettings(draft)}>保存</button>
        </div>
      )}
      <h3>HTTP 事件</h3>
      <EventList events={props.httpEvents.map((event) => ({
        id: event.id,
        title: `${event.method} ${event.path}`,
        meta: `${event.status ?? '-'} ${event.ok ? '成功' : '失败'}`,
        detail: event.error || JSON.stringify(event.body),
      }))} />
      <h3>WebSocket 事件</h3>
      <EventList events={props.wsEvents.map((event) => ({
        id: event.id,
        title: `${wsDirectionLabel(event.direction)} ${wsKindLabel(event.kind)}`,
        meta: new Date(event.at).toLocaleTimeString(),
        detail: event.detail,
      }))} />
    </section>
  );
}

function EventList({ events }: { events: Array<{ id: string; title: string; meta: string; detail?: string }> }) {
  if (!events.length) return <div className="emptyList">暂无事件</div>;
  return (
    <div className="eventList">
      {events.map((event) => (
        <article key={event.id}>
          <div><strong>{event.title}</strong><span>{event.meta}</span></div>
          <p>{event.detail}</p>
        </article>
      ))}
    </div>
  );
}

function IconButton(props: {
  label: string;
  active?: boolean;
  children: React.ReactNode;
  onClick: () => void;
}) {
  return (
    <button className={props.active ? 'iconButton active' : 'iconButton'} title={props.label} type="button" onClick={props.onClick}>
      {props.children}
    </button>
  );
}

function UserAvatar(props: { profile?: Profile; name?: string; large?: boolean }) {
  const displayName = props.profile?.nickname || props.profile?.account || props.name || '?';
  const initial = displayName.slice(0, 1).toUpperCase();
  const className = props.large ? 'profileAvatar large' : 'profileAvatar';
  const [failed, setFailed] = useState(false);
  useEffect(() => {
    setFailed(false);
  }, [props.profile?.avatar]);

  if (props.profile?.avatar && !failed) {
    return (
      <img
        className={className}
        src={props.profile.avatar}
        alt={displayName}
        onError={() => setFailed(true)}
      />
    );
  }
  return <div className={className}>{initial}</div>;
}

function GroupAvatar(props: { large?: boolean }) {
  return (
    <div className={props.large ? 'profileAvatar groupAvatar large' : 'profileAvatar groupAvatar'}>
      <Users size={props.large ? 28 : 18} />
    </div>
  );
}

function NoticePill({ notice }: { notice: Notice }) {
  return <div className={`notice ${notice.type}`}>{notice.text}</div>;
}

function explainError(error: unknown): string {
  if (!(error instanceof Error)) return String(error);
  const message = error.message.trim();
  if (!message) return '操作失败，请稍后重试。';
  if (message.includes('Failed to fetch') || message.includes('NetworkError')) {
    return '无法连接后端服务，请确认服务已启动。';
  }
  if (message.includes('Unexpected token') || message.includes('JSON')) {
    return '服务返回内容异常，请稍后重试。';
  }
  if (/^HTTP\s+\d+/.test(message)) {
    return '请求失败，请稍后重试。';
  }
  return message;
}

function isSameConversation(left: Conversation | null, right: Conversation): boolean {
  if (!left) return false;
  if (left.type !== right.type) return false;
  return left.id === right.id;
}

function conversationKey(conversation: Conversation): string {
  return `${conversation.type}:${conversation.id}`;
}

function mergeDirectMessages(current: DirectMessage[], incoming: DirectMessage[]): DirectMessage[] {
  const seen = new Set(current.map((message) => `${message.msg_id}-${message.sender_uid}-${message.receiver_uid}`));
  const merged = [...current];
  incoming.forEach((message) => {
    const key = `${message.msg_id}-${message.sender_uid}-${message.receiver_uid}`;
    if (!seen.has(key)) merged.push(message);
  });
  return sortDirectMessages(merged);
}

function replaceDirectMessagesForPeer(
  current: DirectMessage[],
  incoming: DirectMessage[],
  peerUid: string,
): DirectMessage[] {
  const peer = peerUid.trim();
  if (!peer) return sortDirectMessages(current);
  const remaining = current.filter((message) => message.sender_uid !== peer && message.receiver_uid !== peer);
  return mergeDirectMessages(remaining, incoming);
}

function sortDirectMessages(messages: DirectMessage[]): DirectMessage[] {
  return [...messages].sort((left, right) => directMessageTime(left) - directMessageTime(right));
}

function sortGroupMessages(messages: GroupMessage[]): GroupMessage[] {
  return [...messages].sort((left, right) => (left.created_at || 0) - (right.created_at || 0));
}

function directMessageTime(message: DirectMessage): number {
  return message.create_time || message.delivered_time || message.read_time || message.msg_id || 0;
}

function parsePushContext(
  body: Record<string, unknown>,
  header?: Record<string, unknown>,
): { senderUid: string; conversationType: string; conversationId: string } {
  const ext = parseObjectExt(body.ext);
  const senderUid = stringField(ext.sender_uid) || stringField(header?.fromUid);
  const conversationType = stringField(ext.conversation_type) || 'direct';
  const conversationId = stringField(ext.conversation_id)
    || (conversationType === 'direct' ? senderUid : '');
  return { senderUid, conversationType, conversationId };
}

function parseObjectExt(value: unknown): Record<string, unknown> {
  if (typeof value !== 'string' || !value.trim()) return {};
  try {
    const parsed = JSON.parse(value) as unknown;
    return parsed && typeof parsed === 'object' && !Array.isArray(parsed)
      ? parsed as Record<string, unknown>
      : {};
  } catch {
    return {};
  }
}

function stringField(value: unknown): string {
  return typeof value === 'string' ? value.trim() : '';
}

function shortId(value: string): string {
  if (!value) return '未知';
  if (value.length <= 10) return value;
  return `${value.slice(0, 6)}...${value.slice(-4)}`;
}

function directTitle(uid: string): string {
  return `联系人 ${shortId(uid)}`;
}

function groupTitle(groupId: number): string {
  return `群聊 ${groupId}`;
}

function messageStatusLabel(status: number): string {
  switch (status) {
    case 0:
      return '已发送';
    case 1:
      return '已送达';
    case 2:
      return '已读';
    default:
      return '状态未知';
  }
}

function groupRoleLabel(role: number): string {
  switch (role) {
    case 1:
      return '群主';
    case 2:
      return '管理员';
    default:
      return '成员';
  }
}

function genderLabel(gender?: number): string {
  switch (gender) {
    case 1:
      return '男';
    case 2:
      return '女';
    case 3:
      return '其他';
    default:
      return '保密';
  }
}

function wsStatusLabel(status: string): string {
  switch (status) {
    case 'connected':
      return '已连接';
    case 'connecting':
      return '连接中';
    case 'closed':
      return '已关闭';
    case 'closing':
      return '关闭中';
    case 'disconnect-requested':
      return '正在断开';
    case 'disconnected':
      return '未连接';
    default:
      return status || '未知';
  }
}

function wsDirectionLabel(direction: WsEvent['direction']): string {
  switch (direction) {
    case 'in':
      return '收到';
    case 'out':
      return '发出';
    case 'state':
      return '状态';
    case 'error':
      return '错误';
    default:
      return direction;
  }
}

function wsKindLabel(kind: string): string {
  switch (kind) {
    case 'connected':
    case 'connecting':
    case 'closed':
    case 'closing':
    case 'disconnect-requested':
    case 'disconnected':
      return wsStatusLabel(kind);
    case 'decode-failed':
      return '解码失败';
    case 'send-failed':
      return '发送失败';
    case 'text-probe':
      return '文本探测';
    case 'text':
      return '文本';
    case 'binary':
      return '二进制帧';
    case 'unknown':
      return '未知帧';
    case 'error':
      return '连接错误';
    default:
      return kind;
  }
}

function profileToForm(profile: Profile): ProfileFormState {
  return {
    nickname: profile.nickname || profile.account,
    avatar: profile.avatar || '',
    gender: profile.gender ?? 0,
    signature: profile.signature || '',
  };
}

function titleForView(view: ViewMode): string {
  switch (view) {
    case 'friends':
      return '联系人';
    case 'groups':
      return '群聊';
    case 'debug':
      return '开发';
    case 'profile':
      return '个人主页';
    case 'settings':
      return '设置';
    default:
      return '消息';
  }
}

function subtitleForView(view: ViewMode, conversation: Conversation | null, session: AuthSession): string {
  if (view === 'chats' && conversation) {
    return conversation.type === 'group' ? '群聊' : '私聊';
  }
  if (view === 'chats') return session.profile.nickname || session.profile.account;
  if (view === 'friends') return '管理联系人和好友申请';
  if (view === 'groups') return '管理群聊';
  if (view === 'profile') return '查看和管理个人资料';
  if (view === 'settings') return '账号与连接';
  return '网关请求与推送诊断';
}
