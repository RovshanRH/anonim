<script setup lang="ts">
// Главный компонент: состояние приложения, криптография и вызовы API
import { computed, onBeforeUnmount, onMounted, ref, watch } from "vue";
import AuthPanel from "./components/AuthPanel.vue";
import GuideSection from "./components/GuideSection.vue";
import ChatPanel from "./components/ChatPanel.vue";
import ProfilePanel from "./components/ProfilePanel.vue";
import { createTranslator, type TranslationKey, type Locale } from "./i18n";
import {
  type ApiMessage,
  type AuthMode,
  type PublicKeyResponse,
  STORAGE_KEYS,
} from "./appShared";

// Базовый адрес backend API (переопределяется через Vite env)
const API_BASE_URL =
  (import.meta.env.VITE_API_BASE_URL as string | undefined)?.trim() ||
  "http://127.0.0.1:8090";
// Локаль и режим аутентификации
const persistedLocale = localStorage.getItem(STORAGE_KEYS.locale);
const locale = ref<Locale>(persistedLocale === "ru" ? "ru" : "en");
const authMode = ref<AuthMode>("login");

// Поля для аутентификации
const username = ref("");
const password = ref("");

// Сохранённая сессия (token + username)
const token = ref(localStorage.getItem(STORAGE_KEYS.token) || "");
const currentUser = ref(localStorage.getItem(STORAGE_KEYS.username) || "");

// Состояние чата
const peerInput = ref("");
const peers = ref<string[]>([]);
const activePeer = ref("");

const draftMessage = ref("");
const messages = ref<ApiMessage[]>([]);
const decryptedMessageMap = ref<Record<number, string>>({});

// UI-статусы и сообщения
const isBusy = ref(false);
const infoMessage = ref("");
const errorMessage = ref("");

// Страница и профиль
const activePage = ref<"landing" | "chat" | "profile">("landing");
const profileAvatarDataUrl = ref("");
const avatarInputError = ref("");

// Криптографическое состояние (ключи, кеш публичных ключей)
const ownPrivateKey = ref<CryptoKey | null>(null);
const ownPublicKeyB64 = ref("");

const peerKeyCache = new Map<string, CryptoKey>();
let decryptGeneration = 0;

let pollTimer: number | undefined;

const isAuthenticated = computed(
  () => token.value.length > 0 && currentUser.value.length > 0,
);
const visibleName = computed(() => currentUser.value || "guest");
const visibleInitial = computed(() =>
  visibleName.value.charAt(0).toUpperCase(),
);

function t(key: TranslationKey): string {
  return createTranslator(locale.value)(key);
}

// Переводчик (обёртка вокруг i18n)

function setInfo(text: string): void {
  infoMessage.value = text;
  if (text) {
    errorMessage.value = "";
  }
}

function setError(text: string): void {
  errorMessage.value = text;
  if (text) {
    infoMessage.value = "";
  }
}

function clearMessages(): void {
  setInfo("");
  setError("");
}

// Управление flash-сообщениями

function persistSession(): void {
  localStorage.setItem(STORAGE_KEYS.token, token.value);
  localStorage.setItem(STORAGE_KEYS.username, currentUser.value);
}

function clearSession(): void {
  token.value = "";
  currentUser.value = "";
  activePeer.value = "";
  messages.value = [];
  decryptedMessageMap.value = {};
  ownPrivateKey.value = null;
  ownPublicKeyB64.value = "";
  peerKeyCache.clear();
  localStorage.removeItem(STORAGE_KEYS.token);
  localStorage.removeItem(STORAGE_KEYS.username);
}

// Сохранение/очистка сессии в localStorage

async function sha256Hex(input: string): Promise<string> {
  const data = new TextEncoder().encode(input);
  const digest = await crypto.subtle.digest("SHA-256", data);
  const bytes = new Uint8Array(digest);
  return Array.from(bytes)
    .map((v) => v.toString(16).padStart(2, "0"))
    .join("");
}

// Хелперы Web Crypto: sha256, nonce, base64 и т.п.

function randomNonce(size = 12): Uint8Array {
  const bytes = new Uint8Array(size);
  crypto.getRandomValues(bytes);
  return bytes;
}

function bytesToBase64(bytes: Uint8Array): string {
  let binary = "";
  for (const byte of bytes) {
    binary += String.fromCharCode(byte);
  }
  return btoa(binary);
}

function base64ToBytes(base64: string): Uint8Array {
  const binary = atob(base64);
  const bytes = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i);
  }
  return bytes;
}

function bytesToArrayBuffer(bytes: Uint8Array): ArrayBuffer {
  return bytes.buffer.slice(
    bytes.byteOffset,
    bytes.byteOffset + bytes.byteLength,
  ) as ArrayBuffer;
}

function base64ToArrayBuffer(base64: string): ArrayBuffer {
  return bytesToArrayBuffer(base64ToBytes(base64));
}

function getPrivateKeyStorageKey(name: string): string {
  return `${STORAGE_KEYS.privateKeyPrefix}${name}`;
}

function getPublicKeyStorageKey(name: string): string {
  return `${STORAGE_KEYS.publicKeyPrefix}${name}`;
}

function getAvatarStorageKey(name: string): string {
  return `${STORAGE_KEYS.avatarPrefix}${name}`;
}

function loadProfileCustomization(name: string): void {
  if (!name) {
    profileAvatarDataUrl.value = "";
    return;
  }

  const savedAvatar = localStorage.getItem(getAvatarStorageKey(name));

  profileAvatarDataUrl.value = savedAvatar || "";
}

// Работа с локальным профилем и аватаром

function persistProfileCustomization(name: string): void {
  if (profileAvatarDataUrl.value) {
    localStorage.setItem(getAvatarStorageKey(name), profileAvatarDataUrl.value);
  } else {
    localStorage.removeItem(getAvatarStorageKey(name));
  }
}

function readFileAsDataUrl(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(String(reader.result || ""));
    reader.onerror = () => reject(new Error(t("msgUnableToReadImage")));
    reader.readAsDataURL(file);
  });
}

async function handleAvatarInput(event: Event): Promise<void> {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];

  avatarInputError.value = "";

  if (!file) {
    return;
  }

  if (!file.type.startsWith("image/")) {
    avatarInputError.value = t("msgChooseImageFile");
    input.value = "";
    return;
  }

  const maxSizeBytes = 2 * 1024 * 1024;
  if (file.size > maxSizeBytes) {
    avatarInputError.value = t("msgImageTooLarge");
    input.value = "";
    return;
  }

  try {
    profileAvatarDataUrl.value = await readFileAsDataUrl(file);
  } catch (error) {
    avatarInputError.value =
      error instanceof Error ? error.message : t("msgUnableToLoadImage");
  }
}

function clearAvatar(): void {
  profileAvatarDataUrl.value = "";
  avatarInputError.value = "";
}

function saveProfileCustomization(): void {
  if (!isAuthenticated.value || !currentUser.value) {
    setError(t("msgProfileSignInRequired"));
    return;
  }

  persistProfileCustomization(currentUser.value);
  setInfo(t("msgProfileUpdated"));
}

function resetProfileCustomization(): void {
  if (!isAuthenticated.value || !currentUser.value) {
    return;
  }

  profileAvatarDataUrl.value = "";
  persistProfileCustomization(currentUser.value);
  setInfo(t("msgProfileReset"));
}

async function generateIdentityKeyPair(): Promise<CryptoKeyPair> {
  return crypto.subtle.generateKey(
    { name: "ECDH", namedCurve: "P-256" },
    true,
    ["deriveKey"],
  );
}

// Генерация/экспорт/импорт ключей ECDH для клиентской E2EE

async function exportPublicKeyBase64(key: CryptoKey): Promise<string> {
  const spki = await crypto.subtle.exportKey("spki", key);
  return bytesToBase64(new Uint8Array(spki));
}

async function exportPrivateKeyBase64(key: CryptoKey): Promise<string> {
  const pkcs8 = await crypto.subtle.exportKey("pkcs8", key);
  return bytesToBase64(new Uint8Array(pkcs8));
}

async function importPublicKeyBase64(raw: string): Promise<CryptoKey> {
  const data = base64ToArrayBuffer(raw);
  return crypto.subtle.importKey(
    "spki",
    data,
    { name: "ECDH", namedCurve: "P-256" },
    true,
    [],
  );
}

async function importPrivateKeyBase64(raw: string): Promise<CryptoKey> {
  const data = base64ToArrayBuffer(raw);
  return crypto.subtle.importKey(
    "pkcs8",
    data,
    { name: "ECDH", namedCurve: "P-256" },
    true,
    ["deriveKey"],
  );
}

async function deriveChatKey(peerPublicKey: CryptoKey): Promise<CryptoKey> {
  if (!ownPrivateKey.value) {
    throw new Error("Private key is not available on this device");
  }

  return crypto.subtle.deriveKey(
    {
      name: "ECDH",
      public: peerPublicKey,
    },
    ownPrivateKey.value,
    { name: "AES-GCM", length: 256 },
    false,
    ["encrypt", "decrypt"],
  );
}

async function getPeerPublicKey(usernameToCheck: string): Promise<CryptoKey> {
  const cached = peerKeyCache.get(usernameToCheck);
  if (cached) {
    return cached;
  }

  const payload = (await requestApi(
    `/api/users/${encodeURIComponent(usernameToCheck)}/public-key`,
  )) as PublicKeyResponse;

  const imported = await importPublicKeyBase64(payload.publicKey);
  peerKeyCache.set(usernameToCheck, imported);
  return imported;
}

async function encryptForPeer(
  plainText: string,
  peerName: string,
): Promise<{ ciphertext: string; nonce: string }> {
  const peerKey = await getPeerPublicKey(peerName);
  const sharedKey = await deriveChatKey(peerKey);
  const nonce = randomNonce(12);
  const nonceBuffer = bytesToArrayBuffer(nonce);
  const data = new TextEncoder().encode(plainText);

  const encrypted = await crypto.subtle.encrypt(
    {
      name: "AES-GCM",
      iv: nonceBuffer,
    },
    sharedKey,
    data,
  );

  return {
    ciphertext: bytesToBase64(new Uint8Array(encrypted)),
    nonce: bytesToBase64(nonce),
  };
}

// Шифрование и расшифровка сообщений на клиенте

async function decryptMessage(message: ApiMessage): Promise<string> {
  const peerName =
    message.from === currentUser.value ? message.to : message.from;
  const peerKey = await getPeerPublicKey(peerName);
  const sharedKey = await deriveChatKey(peerKey);

  const decrypted = await crypto.subtle.decrypt(
    {
      name: "AES-GCM",
      iv: base64ToArrayBuffer(message.nonce),
    },
    sharedKey,
    base64ToArrayBuffer(message.ciphertext),
  );

  return new TextDecoder().decode(decrypted);
}

async function decryptVisibleMessages(): Promise<void> {
  const generation = ++decryptGeneration;
  const nextMap: Record<number, string> = {};

  for (const message of messages.value) {
    try {
      nextMap[message.id] = await decryptMessage(message);
    } catch {
      nextMap[message.id] = t("msgDecryptFailed");
    }
  }

  if (generation === decryptGeneration) {
    decryptedMessageMap.value = nextMap;
  }
}

function getMessageText(message: ApiMessage): string {
  return decryptedMessageMap.value[message.id] || t("msgDecrypting");
}

async function loadOwnIdentity(name: string): Promise<void> {
  const privateRaw = localStorage.getItem(getPrivateKeyStorageKey(name));
  const publicRaw = localStorage.getItem(getPublicKeyStorageKey(name));

  if (!privateRaw || !publicRaw) {
    throw new Error(
      "Private key for this account is missing in current browser",
    );
  }

  ownPrivateKey.value = await importPrivateKeyBase64(privateRaw);
  ownPublicKeyB64.value = publicRaw;
}

function persistIdentity(
  name: string,
  privateKeyRaw: string,
  publicKeyRaw: string,
): void {
  localStorage.setItem(getPrivateKeyStorageKey(name), privateKeyRaw);
  localStorage.setItem(getPublicKeyStorageKey(name), publicKeyRaw);
}

function isE2EEReady(): boolean {
  return ownPrivateKey.value !== null && ownPublicKeyB64.value.length > 0;
}

async function requestApi(
  path: string,
  init?: RequestInit,
  withAuth = false,
): Promise<unknown> {
  const headers = new Headers(init?.headers || {});
  headers.set("Content-Type", "application/json");

  if (withAuth) {
    const authToken = token.value.trim();
    if (!authToken) {
      throw new Error(t("msgLoginFirst"));
    }

    headers.set("Authorization", `Bearer ${authToken}`);
    headers.set("X-Auth-Token", authToken);
  }

  const response = await fetch(`${API_BASE_URL}${path}`, {
    ...init,
    headers,
  });

  const text = await response.text();
  const payload = text ? JSON.parse(text) : {};

  if (!response.ok) {
    const errorText =
      typeof payload?.error === "string"
        ? payload.error
        : `HTTP ${response.status}`;
    throw new Error(errorText);
  }

  return payload;
}

// Обёртка для fetch: JSON, ошибки и добавление заголовков авторизации

function rememberPeer(name: string): void {
  const normalized = name.trim();
  if (!normalized || peers.value.includes(normalized)) {
    return;
  }
  peers.value = [normalized, ...peers.value];
}

async function register(): Promise<void> {
  if (!username.value || !password.value) {
    setError(t("msgFillUsernamePassword"));
    return;
  }

  isBusy.value = true;
  clearMessages();

  try {
    const keyPair = await generateIdentityKeyPair();
    const exportedPublic = await exportPublicKeyBase64(keyPair.publicKey);
    const exportedPrivate = await exportPrivateKeyBase64(keyPair.privateKey);

    await requestApi("/api/register", {
      method: "POST",
      body: JSON.stringify({
        username: username.value.trim(),
        passwordHash: await sha256Hex(password.value),
        publicKey: exportedPublic,
      }),
    });

    persistIdentity(username.value.trim(), exportedPrivate, exportedPublic);

    setInfo(t("msgRegistrationSuccess"));
    authMode.value = "login";
  } catch (error) {
    setError(
      error instanceof Error ? error.message : t("msgRegistrationFailed"),
    );
  } finally {
    isBusy.value = false;
  }
}

// Регистрация и вход: вызывают соответствующие backend-эндпоинты

async function login(): Promise<void> {
  if (!username.value || !password.value) {
    setError(t("msgFillUsernamePassword"));
    return;
  }

  isBusy.value = true;
  clearMessages();

  try {
    const result = (await requestApi("/api/login", {
      method: "POST",
      body: JSON.stringify({
        username: username.value.trim(),
        passwordHash: await sha256Hex(password.value),
      }),
    })) as { token: string; username: string };

    await loadOwnIdentity(result.username);

    token.value = result.token;
    currentUser.value = result.username;
    loadProfileCustomization(result.username);
    persistSession();
    peerKeyCache.clear();
    setInfo(`${t("msgWelcome")}, ${visibleName.value}`);
  } catch (error) {
    clearSession();
    setError(error instanceof Error ? error.message : t("msgLoginFailed"));
  } finally {
    isBusy.value = false;
  }
}

async function verifyPeer(usernameToCheck: string): Promise<void> {
  await getPeerPublicKey(usernameToCheck);
}

async function attachPeer(): Promise<void> {
  const peer = peerInput.value.trim();
  if (!peer) {
    setError(t("msgEnterPeer"));
    return;
  }
  if (!isAuthenticated.value) {
    setError(t("msgLoginFirst"));
    return;
  }

  isBusy.value = true;

  try {
    await verifyPeer(peer);
    rememberPeer(peer);
    activePeer.value = peer;
    peerInput.value = "";
    await loadMessages();
    setInfo(`${t("msgConnectedWith")} ${peer}`);
  } catch (error) {
    setError(error instanceof Error ? error.message : t("msgCannotOpenChat"));
  } finally {
    isBusy.value = false;
  }
}

// Подключение к собеседнику и загрузка сообщений

async function loadMessages(): Promise<void> {
  if (!activePeer.value || !isAuthenticated.value) {
    return;
  }

  try {
    const result = (await requestApi(
      `/api/messages?with=${encodeURIComponent(activePeer.value)}`,
      { method: "GET" },
      true,
    )) as { messages: ApiMessage[] };

    messages.value = Array.isArray(result.messages)
      ? [...result.messages].sort((a, b) => a.id - b.id)
      : [];

    void decryptVisibleMessages();
  } catch (error) {
    setError(
      error instanceof Error ? error.message : t("msgUnableToLoadMessages"),
    );
  }
}

// Загрузка переписки и обновление локального кеша

async function sendMessage(): Promise<void> {
  const text = draftMessage.value.trim();
  if (!text || !activePeer.value) {
    return;
  }
  if (!isE2EEReady()) {
    setError(t("msgE2eeUnavailable"));
    return;
  }

  isBusy.value = true;

  try {
    const encryptedPayload = await encryptForPeer(text, activePeer.value);

    await requestApi(
      "/api/messages",
      {
        method: "POST",
        body: JSON.stringify({
          to: activePeer.value,
          messageType: "text",
          ciphertext: encryptedPayload.ciphertext,
          nonce: encryptedPayload.nonce,
          encryption: "ecdh-p256/aes-gcm-256",
        }),
      },
      true,
    );

    draftMessage.value = "";
    await loadMessages();
  } catch (error) {
    setError(
      error instanceof Error ? error.message : t("msgUnableToSendMessage"),
    );
  } finally {
    isBusy.value = false;
  }
}

// Отправка шифрованного сообщения на сервер

function startPolling(): void {
  stopPolling();
  pollTimer = window.setInterval(() => {
    void loadMessages();
  }, 3000);
}

function stopPolling(): void {
  if (pollTimer !== undefined) {
    window.clearInterval(pollTimer);
    pollTimer = undefined;
  }
}

function logout(): void {
  stopPolling();
  clearSession();
  profileAvatarDataUrl.value = "";
  activePage.value = "landing";
  setInfo(t("msgSessionCleared"));
}

onMounted(() => {
  if (isAuthenticated.value) {
    loadProfileCustomization(currentUser.value);
    startPolling();
  }
});

onBeforeUnmount(() => {
  stopPolling();
});

watch(locale, (value) => {
  localStorage.setItem(STORAGE_KEYS.locale, value);
});

watch(isAuthenticated, (value) => {
  if (value) {
    startPolling();
    return;
  }
  stopPolling();
});

watch(currentUser, (value) => {
  if (value) {
    loadProfileCustomization(value);
  }
});
</script>

<template>
  <main class="layout">
    <AuthPanel
      :t="t"
      :locale="locale"
      :auth-mode="authMode"
      :username="username"
      :password="password"
      :is-busy="isBusy"
      :is-authenticated="isAuthenticated"
      :info-message="infoMessage"
      :error-message="errorMessage"
      @update:locale="locale = $event"
      @update:auth-mode="authMode = $event"
      @update:username="username = $event"
      @update:password="password = $event"
      @submit="authMode === 'login' ? login() : register()"
      @logout="logout"
    />

    <section class="panel panel--chat">
      <header class="chat-head">
        <h2>{{ t("conversations") }}</h2>
        <p v-if="isAuthenticated">
          {{ t("signedInAs") }} <strong>{{ visibleName }}</strong> (@{{
            currentUser
          }})
        </p>
        <p v-else>{{ t("authHint") }}</p>

        <div class="tabs tabs--page">
          <button
            :class="{ active: activePage === 'landing' }"
            @click="activePage = 'landing'"
          >
            {{ t("guideTab") }}
          </button>
          <button
            :class="{ active: activePage === 'chat' }"
            @click="activePage = 'chat'"
          >
            {{ t("chatTab") }}
          </button>
          <button
            :class="{ active: activePage === 'profile' }"
            @click="activePage = 'profile'"
          >
            {{ t("profileTab") }}
          </button>
        </div>
      </header>

      <GuideSection v-if="activePage === 'landing'" :t="t" />

      <ChatPanel
        v-else-if="activePage === 'chat'"
        :t="t"
        :is-authenticated="isAuthenticated"
        :is-busy="isBusy"
        :current-user="currentUser"
        :peer-input="peerInput"
        :peers="peers"
        :active-peer="activePeer"
        :draft-message="draftMessage"
        :messages="messages"
        :get-message-text="getMessageText"
        @update:peer-input="peerInput = $event"
        @update:active-peer="activePeer = $event"
        @update:draft-message="draftMessage = $event"
        @attach-peer="attachPeer"
        @load-messages="loadMessages"
        @send-message="sendMessage"
      />

      <ProfilePanel
        v-else
        :t="t"
        :is-authenticated="isAuthenticated"
        :profile-avatar-data-url="profileAvatarDataUrl"
        :visible-name="visibleName"
        :visible-initial="visibleInitial"
        :current-user="currentUser"
        :avatar-input-error="avatarInputError"
        @avatar-input="handleAvatarInput"
        @clear-avatar="clearAvatar"
        @save-profile="saveProfileCustomization"
        @reset-profile="resetProfileCustomization"
      />
    </section>
  </main>
</template>
