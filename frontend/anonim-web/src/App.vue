<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, ref, watch } from 'vue'

type AuthMode = 'login' | 'register'

type ApiMessage = {
  id: number
  from: string
  to: string
  messageType?: 'text'
  ciphertext: string
  nonce: string
  encryption: string
  timestamp: string
}

type PublicKeyResponse = {
  username: string
  publicKey: string
}

type Locale = 'en' | 'ru'

const STORAGE_KEYS = {
  token: 'anonim.token',
  username: 'anonim.username',
  privateKeyPrefix: 'anonim.privateKey.',
  publicKeyPrefix: 'anonim.publicKey.',
  avatarPrefix: 'anonim.avatar.',
  locale: 'anonim.locale',
} as const

const I18N = {
  en: {
    appTitle: 'Cipher Chat Client',
    language: 'Language',
    langEn: 'English',
    langRu: 'Russian',
    loginTab: 'Login',
    registerTab: 'Register',
    username: 'Username',
    password: 'Password',
    e2eeKeys: 'E2EE keys',
    e2eePlaceholder: 'On register this app generates ECDH P-256 keys and uploads only public key',
    pleaseWait: 'Please wait...',
    signIn: 'Sign in',
    createAccount: 'Create account',
    logout: 'Logout',
    conversations: 'Conversations',
    signedInAs: 'Signed in as',
    authHint: 'Please authenticate to open chat',
    guideTab: 'Guide',
    chatTab: 'Chat',
    profileTab: 'Profile',
    peerUsername: 'peer username',
    open: 'Open',
    choosePeer: 'Choose or add a peer to start chatting.',
    typeMessage: 'type message',
    send: 'Send',
    welcomeEyebrow: 'welcome',
    guideHeroTitle: 'Anonim is an encrypted messenger with browser-side key management',
    guideHeroText:
      'You can explore this guide without signing in. To start chatting, create an account, keep your private key on this device, and open a conversation with another user.',
    howToStart: 'How to start',
    howToStart1: 'Make sure backend is available at http://127.0.0.1:8090.',
    howToStart2: 'Register a new account to generate your ECDH key pair in browser.',
    howToStart3: 'Sign in and open chat with peer username.',
    howToStart4: 'Type message and send. Ciphertext is stored on server.',
    messengerFeatures: 'Messenger functionality',
    feature1: 'Registration and login with token authentication.',
    feature2: 'Peer-based dialogs with message polling every 3 seconds.',
    feature3: 'Automatic decrypt for messages visible in active conversation.',
    feature4: 'Local profile avatar customization in this browser.',
    encryptionBasics: 'Encryption basics in this app',
    encryptionText:
      'Anonim uses ECDH on curve P-256 to derive a shared secret between two users. The shared secret becomes an AES-GCM 256 key used for message encryption and authentication.',
    encryption1: 'Public key is uploaded to backend and shared for peer discovery.',
    encryption2: 'Private key remains in localStorage on your current device only.',
    encryption3: 'Each message uses a random nonce with AES-GCM.',
    encryption4: 'Server stores ciphertext, nonce, metadata, but not plaintext.',
    avatarImage: 'Avatar image (max 2 MB)',
    removeAvatar: 'Remove avatar',
    saveProfile: 'Save profile',
    reset: 'Reset',
    avatarNote: 'Avatar customization is local for this browser and does not change account login.',
    msgFillUsernamePassword: 'Fill username and password',
    msgRegistrationSuccess: 'Registration successful. Device key pair was generated.',
    msgRegistrationFailed: 'Registration failed',
    msgWelcome: 'Welcome',
    msgLoginFailed: 'Login failed',
    msgEnterPeer: 'Enter peer username',
    msgLoginFirst: 'Login first',
    msgConnectedWith: 'Connected with',
    msgCannotOpenChat: 'Cannot open chat with this user',
    msgUnableToLoadMessages: 'Unable to load messages',
    msgUnableToSendMessage: 'Unable to send message',
    msgSessionCleared: 'Session cleared',
    msgProfileSignInRequired: 'Sign in first to update your profile',
    msgProfileUpdated: 'Profile updated on this device',
    msgProfileReset: 'Profile customization reset',
    msgChooseImageFile: 'Choose an image file (png, jpg, webp, etc.)',
    msgImageTooLarge: 'Image is too large. Maximum size is 2 MB',
    msgUnableToLoadImage: 'Unable to load image',
    msgUnableToReadImage: 'Unable to read selected image',
    msgE2eeUnavailable: 'E2EE identity is unavailable in current browser',
    msgDecrypting: '[decrypting...]',
    msgDecryptFailed: '[unable to decrypt on this device]',
  },
  ru: {
    appTitle: 'Клиент шифрованного чата',
    language: 'Язык',
    langEn: 'Английский',
    langRu: 'Русский',
    loginTab: 'Вход',
    registerTab: 'Регистрация',
    username: 'Логин',
    password: 'Пароль',
    e2eeKeys: 'Ключи E2EE',
    e2eePlaceholder: 'При регистрации приложение генерирует ECDH P-256 ключи и отправляет на сервер только публичный ключ',
    pleaseWait: 'Подождите...',
    signIn: 'Войти',
    createAccount: 'Создать аккаунт',
    logout: 'Выйти',
    conversations: 'Диалоги',
    signedInAs: 'Вы вошли как',
    authHint: 'Авторизуйтесь, чтобы открыть чат',
    guideTab: 'Гайд',
    chatTab: 'Чат',
    profileTab: 'Профиль',
    peerUsername: 'Логин собеседника',
    open: 'Открыть',
    choosePeer: 'Выберите или добавьте собеседника, чтобы начать чат.',
    typeMessage: 'Введите сообщение',
    send: 'Отправить',
    welcomeEyebrow: 'Добро пожаловать',
    guideHeroTitle: 'Anonim — зашифрованный мессенджер с управлением ключами в браузере',
    guideHeroText:
      'Этот гайд доступен без входа. Чтобы начать общение, создайте аккаунт, сохраните приватный ключ на этом устройстве и откройте диалог с другим пользователем.',
    howToStart: 'Как начать',
    howToStart1: 'Убедитесь, что backend доступен по адресу http://127.0.0.1:8090.',
    howToStart2: 'Зарегистрируйте аккаунт, чтобы сгенерировать ECDH-пару ключей в браузере.',
    howToStart3: 'Войдите и откройте чат по логину собеседника.',
    howToStart4: 'Введите сообщение и отправьте. На сервере хранится только шифртекст.',
    messengerFeatures: 'Функционал мессенджера',
    feature1: 'Регистрация и вход с токен-аутентификацией.',
    feature2: 'Диалоги по логину пользователя и обновление сообщений каждые 3 секунды.',
    feature3: 'Автоматическая расшифровка сообщений в активном диалоге.',
    feature4: 'Локальная настройка аватара профиля в этом браузере.',
    encryptionBasics: 'Основы шифрования в приложении',
    encryptionText:
      'Anonim использует ECDH на кривой P-256 для вычисления общего секрета между двумя пользователями. Из него получается ключ AES-GCM 256 для шифрования и аутентификации сообщений.',
    encryption1: 'Публичный ключ загружается на backend и используется для поиска ключа собеседника.',
    encryption2: 'Приватный ключ хранится только в localStorage текущего устройства.',
    encryption3: 'Каждое сообщение шифруется с новым случайным nonce в AES-GCM.',
    encryption4: 'Сервер хранит шифртекст, nonce и метаданные, но не открытый текст.',
    avatarImage: 'Изображение аватара (до 2 МБ)',
    removeAvatar: 'Удалить аватар',
    saveProfile: 'Сохранить профиль',
    reset: 'Сброс',
    avatarNote: 'Настройка аватара локальная для этого браузера и не меняет логин аккаунта.',
    msgFillUsernamePassword: 'Заполните логин и пароль',
    msgRegistrationSuccess: 'Регистрация успешна. Пара ключей устройства создана.',
    msgRegistrationFailed: 'Ошибка регистрации',
    msgWelcome: 'Добро пожаловать',
    msgLoginFailed: 'Ошибка входа',
    msgEnterPeer: 'Введите логин собеседника',
    msgLoginFirst: 'Сначала выполните вход',
    msgConnectedWith: 'Подключено к',
    msgCannotOpenChat: 'Невозможно открыть чат с этим пользователем',
    msgUnableToLoadMessages: 'Не удалось загрузить сообщения',
    msgUnableToSendMessage: 'Не удалось отправить сообщение',
    msgSessionCleared: 'Сессия очищена',
    msgProfileSignInRequired: 'Сначала выполните вход, чтобы обновить профиль',
    msgProfileUpdated: 'Профиль обновлен на этом устройстве',
    msgProfileReset: 'Настройки профиля сброшены',
    msgChooseImageFile: 'Выберите файл изображения (png, jpg, webp и т.д.)',
    msgImageTooLarge: 'Изображение слишком большое. Максимальный размер 2 МБ',
    msgUnableToLoadImage: 'Не удалось загрузить изображение',
    msgUnableToReadImage: 'Не удалось прочитать выбранное изображение',
    msgE2eeUnavailable: 'E2EE-ключи недоступны в текущем браузере',
    msgDecrypting: '[Расшифровка...]',
    msgDecryptFailed: '[не удалось расшифровать на этом устройстве]',
  },
} as const

const API_BASE_URL =
  (import.meta.env.VITE_API_BASE_URL as string | undefined)?.trim() || 'http://127.0.0.1:8090'
const persistedLocale = localStorage.getItem(STORAGE_KEYS.locale)
const locale = ref<Locale>(persistedLocale === 'ru' ? 'ru' : 'en')
const authMode = ref<AuthMode>('login')

const username = ref('')
const password = ref('')

const token = ref(localStorage.getItem(STORAGE_KEYS.token) || '')
const currentUser = ref(localStorage.getItem(STORAGE_KEYS.username) || '')

const peerInput = ref('')
const peers = ref<string[]>([])
const activePeer = ref('')

const draftMessage = ref('')
const messages = ref<ApiMessage[]>([])
const decryptedMessageMap = ref<Record<number, string>>({})

const isBusy = ref(false)
const infoMessage = ref('')
const errorMessage = ref('')

const activePage = ref<'landing' | 'chat' | 'profile'>('landing')
const profileAvatarDataUrl = ref('')
const avatarInputError = ref('')

const ownPrivateKey = ref<CryptoKey | null>(null)
const ownPublicKeyB64 = ref('')

const peerKeyCache = new Map<string, CryptoKey>()
let decryptGeneration = 0

let pollTimer: number | undefined

const isAuthenticated = computed(() => token.value.length > 0 && currentUser.value.length > 0)
const visibleName = computed(() => currentUser.value || 'guest')
const visibleInitial = computed(() => visibleName.value.charAt(0).toUpperCase())

function t(key: keyof (typeof I18N)['en']): string {
  return I18N[locale.value][key]
}

function setInfo(text: string): void {
  infoMessage.value = text
  if (text) {
    errorMessage.value = ''
  }
}

function setError(text: string): void {
  errorMessage.value = text
  if (text) {
    infoMessage.value = ''
  }
}

function clearMessages(): void {
  setInfo('')
  setError('')
}

function persistSession(): void {
  localStorage.setItem(STORAGE_KEYS.token, token.value)
  localStorage.setItem(STORAGE_KEYS.username, currentUser.value)
}

function clearSession(): void {
  token.value = ''
  currentUser.value = ''
  activePeer.value = ''
  messages.value = []
  decryptedMessageMap.value = {}
  ownPrivateKey.value = null
  ownPublicKeyB64.value = ''
  peerKeyCache.clear()
  localStorage.removeItem(STORAGE_KEYS.token)
  localStorage.removeItem(STORAGE_KEYS.username)
}

async function sha256Hex(input: string): Promise<string> {
  const data = new TextEncoder().encode(input)
  const digest = await crypto.subtle.digest('SHA-256', data)
  const bytes = new Uint8Array(digest)
  return Array.from(bytes)
    .map((v) => v.toString(16).padStart(2, '0'))
    .join('')
}

function randomNonce(size = 12): Uint8Array {
  const bytes = new Uint8Array(size)
  crypto.getRandomValues(bytes)
  return bytes
}

function bytesToBase64(bytes: Uint8Array): string {
  let binary = ''
  for (const byte of bytes) {
    binary += String.fromCharCode(byte)
  }
  return btoa(binary)
}

function base64ToBytes(base64: string): Uint8Array {
  const binary = atob(base64)
  const bytes = new Uint8Array(binary.length)
  for (let i = 0; i < binary.length; i += 1) {
    bytes[i] = binary.charCodeAt(i)
  }
  return bytes
}

function bytesToArrayBuffer(bytes: Uint8Array): ArrayBuffer {
  return bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength) as ArrayBuffer
}

function base64ToArrayBuffer(base64: string): ArrayBuffer {
  return bytesToArrayBuffer(base64ToBytes(base64))
}

function getPrivateKeyStorageKey(name: string): string {
  return `${STORAGE_KEYS.privateKeyPrefix}${name}`
}

function getPublicKeyStorageKey(name: string): string {
  return `${STORAGE_KEYS.publicKeyPrefix}${name}`
}

function getAvatarStorageKey(name: string): string {
  return `${STORAGE_KEYS.avatarPrefix}${name}`
}

function loadProfileCustomization(name: string): void {
  if (!name) {
    profileAvatarDataUrl.value = ''
    return
  }

  const savedAvatar = localStorage.getItem(getAvatarStorageKey(name))

  profileAvatarDataUrl.value = savedAvatar || ''
}

function persistProfileCustomization(name: string): void {
  if (profileAvatarDataUrl.value) {
    localStorage.setItem(getAvatarStorageKey(name), profileAvatarDataUrl.value)
  } else {
    localStorage.removeItem(getAvatarStorageKey(name))
  }
}

function readFileAsDataUrl(file: File): Promise<string> {
  return new Promise((resolve, reject) => {
    const reader = new FileReader()
    reader.onload = () => resolve(String(reader.result || ''))
    reader.onerror = () => reject(new Error(t('msgUnableToReadImage')))
    reader.readAsDataURL(file)
  })
}

async function handleAvatarInput(event: Event): Promise<void> {
  const input = event.target as HTMLInputElement
  const file = input.files?.[0]

  avatarInputError.value = ''

  if (!file) {
    return
  }

  if (!file.type.startsWith('image/')) {
    avatarInputError.value = t('msgChooseImageFile')
    input.value = ''
    return
  }

  const maxSizeBytes = 2 * 1024 * 1024
  if (file.size > maxSizeBytes) {
    avatarInputError.value = t('msgImageTooLarge')
    input.value = ''
    return
  }

  try {
    profileAvatarDataUrl.value = await readFileAsDataUrl(file)
  } catch (error) {
    avatarInputError.value = error instanceof Error ? error.message : t('msgUnableToLoadImage')
  }
}

function clearAvatar(): void {
  profileAvatarDataUrl.value = ''
  avatarInputError.value = ''
}

function saveProfileCustomization(): void {
  if (!isAuthenticated.value || !currentUser.value) {
    setError(t('msgProfileSignInRequired'))
    return
  }

  persistProfileCustomization(currentUser.value)
  setInfo(t('msgProfileUpdated'))
}

function resetProfileCustomization(): void {
  if (!isAuthenticated.value || !currentUser.value) {
    return
  }

  profileAvatarDataUrl.value = ''
  persistProfileCustomization(currentUser.value)
  setInfo(t('msgProfileReset'))
}

async function generateIdentityKeyPair(): Promise<CryptoKeyPair> {
  return crypto.subtle.generateKey({ name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveKey'])
}

async function exportPublicKeyBase64(key: CryptoKey): Promise<string> {
  const spki = await crypto.subtle.exportKey('spki', key)
  return bytesToBase64(new Uint8Array(spki))
}

async function exportPrivateKeyBase64(key: CryptoKey): Promise<string> {
  const pkcs8 = await crypto.subtle.exportKey('pkcs8', key)
  return bytesToBase64(new Uint8Array(pkcs8))
}

async function importPublicKeyBase64(raw: string): Promise<CryptoKey> {
  const data = base64ToArrayBuffer(raw)
  return crypto.subtle.importKey('spki', data, { name: 'ECDH', namedCurve: 'P-256' }, true, [])
}

async function importPrivateKeyBase64(raw: string): Promise<CryptoKey> {
  const data = base64ToArrayBuffer(raw)
  return crypto.subtle.importKey('pkcs8', data, { name: 'ECDH', namedCurve: 'P-256' }, true, ['deriveKey'])
}

async function deriveChatKey(peerPublicKey: CryptoKey): Promise<CryptoKey> {
  if (!ownPrivateKey.value) {
    throw new Error('Private key is not available on this device')
  }

  return crypto.subtle.deriveKey(
    {
      name: 'ECDH',
      public: peerPublicKey,
    },
    ownPrivateKey.value,
    { name: 'AES-GCM', length: 256 },
    false,
    ['encrypt', 'decrypt'],
  )
}

async function getPeerPublicKey(usernameToCheck: string): Promise<CryptoKey> {
  const cached = peerKeyCache.get(usernameToCheck)
  if (cached) {
    return cached
  }

  const payload = (await requestApi(
    `/api/users/${encodeURIComponent(usernameToCheck)}/public-key`,
  )) as PublicKeyResponse

  const imported = await importPublicKeyBase64(payload.publicKey)
  peerKeyCache.set(usernameToCheck, imported)
  return imported
}

async function encryptForPeer(plainText: string, peerName: string): Promise<{ ciphertext: string; nonce: string }> {
  const peerKey = await getPeerPublicKey(peerName)
  const sharedKey = await deriveChatKey(peerKey)
  const nonce = randomNonce(12)
  const nonceBuffer = bytesToArrayBuffer(nonce)
  const data = new TextEncoder().encode(plainText)

  const encrypted = await crypto.subtle.encrypt(
    {
      name: 'AES-GCM',
      iv: nonceBuffer,
    },
    sharedKey,
    data,
  )

  return {
    ciphertext: bytesToBase64(new Uint8Array(encrypted)),
    nonce: bytesToBase64(nonce),
  }
}

async function decryptMessage(message: ApiMessage): Promise<string> {
  const peerName = message.from === currentUser.value ? message.to : message.from
  const peerKey = await getPeerPublicKey(peerName)
  const sharedKey = await deriveChatKey(peerKey)

  const decrypted = await crypto.subtle.decrypt(
    {
      name: 'AES-GCM',
      iv: base64ToArrayBuffer(message.nonce),
    },
    sharedKey,
    base64ToArrayBuffer(message.ciphertext),
  )

  return new TextDecoder().decode(decrypted)
}

async function decryptVisibleMessages(): Promise<void> {
  const generation = ++decryptGeneration
  const nextMap: Record<number, string> = {}

  for (const message of messages.value) {
    try {
      nextMap[message.id] = await decryptMessage(message)
    } catch {
      nextMap[message.id] = t('msgDecryptFailed')
    }
  }

  if (generation === decryptGeneration) {
    decryptedMessageMap.value = nextMap
  }
}

function getMessageText(message: ApiMessage): string {
  return decryptedMessageMap.value[message.id] || t('msgDecrypting')
}

async function loadOwnIdentity(name: string): Promise<void> {
  const privateRaw = localStorage.getItem(getPrivateKeyStorageKey(name))
  const publicRaw = localStorage.getItem(getPublicKeyStorageKey(name))

  if (!privateRaw || !publicRaw) {
    throw new Error('Private key for this account is missing in current browser')
  }

  ownPrivateKey.value = await importPrivateKeyBase64(privateRaw)
  ownPublicKeyB64.value = publicRaw
}

function persistIdentity(name: string, privateKeyRaw: string, publicKeyRaw: string): void {
  localStorage.setItem(getPrivateKeyStorageKey(name), privateKeyRaw)
  localStorage.setItem(getPublicKeyStorageKey(name), publicKeyRaw)
}

function isE2EEReady(): boolean {
  return ownPrivateKey.value !== null && ownPublicKeyB64.value.length > 0
}

async function requestApi(path: string, init?: RequestInit, withAuth = false): Promise<unknown> {
  const headers = new Headers(init?.headers || {})
  headers.set('Content-Type', 'application/json')

  if (withAuth) {
    const authToken = token.value.trim()
    if (!authToken) {
      throw new Error(t('msgLoginFirst'))
    }

    headers.set('Authorization', `Bearer ${authToken}`)
    headers.set('X-Auth-Token', authToken)
  }

  const response = await fetch(`${API_BASE_URL}${path}`, {
    ...init,
    headers,
  })

  const text = await response.text()
  const payload = text ? JSON.parse(text) : {}

  if (!response.ok) {
    const errorText = typeof payload?.error === 'string' ? payload.error : `HTTP ${response.status}`
    throw new Error(errorText)
  }

  return payload
}

function rememberPeer(name: string): void {
  const normalized = name.trim()
  if (!normalized || peers.value.includes(normalized)) {
    return
  }
  peers.value = [normalized, ...peers.value]
}

async function register(): Promise<void> {
  if (!username.value || !password.value) {
    setError(t('msgFillUsernamePassword'))
    return
  }

  isBusy.value = true
  clearMessages()

  try {
    const keyPair = await generateIdentityKeyPair()
    const exportedPublic = await exportPublicKeyBase64(keyPair.publicKey)
    const exportedPrivate = await exportPrivateKeyBase64(keyPair.privateKey)

    await requestApi('/api/register', {
      method: 'POST',
      body: JSON.stringify({
        username: username.value.trim(),
        passwordHash: await sha256Hex(password.value),
        publicKey: exportedPublic,
      }),
    })

    persistIdentity(username.value.trim(), exportedPrivate, exportedPublic)

    setInfo(t('msgRegistrationSuccess'))
    authMode.value = 'login'
  } catch (error) {
    setError(error instanceof Error ? error.message : t('msgRegistrationFailed'))
  } finally {
    isBusy.value = false
  }
}

async function login(): Promise<void> {
  if (!username.value || !password.value) {
    setError(t('msgFillUsernamePassword'))
    return
  }

  isBusy.value = true
  clearMessages()

  try {
    const result = (await requestApi('/api/login', {
      method: 'POST',
      body: JSON.stringify({
        username: username.value.trim(),
        passwordHash: await sha256Hex(password.value),
      }),
    })) as { token: string; username: string }

    await loadOwnIdentity(result.username)

    token.value = result.token
    currentUser.value = result.username
    loadProfileCustomization(result.username)
    persistSession()
    peerKeyCache.clear()
    setInfo(`${t('msgWelcome')}, ${visibleName.value}`)
  } catch (error) {
    clearSession()
    setError(error instanceof Error ? error.message : t('msgLoginFailed'))
  } finally {
    isBusy.value = false
  }
}

async function verifyPeer(usernameToCheck: string): Promise<void> {
  await getPeerPublicKey(usernameToCheck)
}

async function attachPeer(): Promise<void> {
  const peer = peerInput.value.trim()
  if (!peer) {
    setError(t('msgEnterPeer'))
    return
  }
  if (!isAuthenticated.value) {
    setError(t('msgLoginFirst'))
    return
  }

  isBusy.value = true

  try {
    await verifyPeer(peer)
    rememberPeer(peer)
    activePeer.value = peer
    peerInput.value = ''
    await loadMessages()
    setInfo(`${t('msgConnectedWith')} ${peer}`)
  } catch (error) {
    setError(error instanceof Error ? error.message : t('msgCannotOpenChat'))
  } finally {
    isBusy.value = false
  }
}

async function loadMessages(): Promise<void> {
  if (!activePeer.value || !isAuthenticated.value) {
    return
  }

  try {
    const result = (await requestApi(
      `/api/messages?with=${encodeURIComponent(activePeer.value)}`,
      { method: 'GET' },
      true,
    )) as { messages: ApiMessage[] }

    messages.value = Array.isArray(result.messages)
      ? [...result.messages].sort((a, b) => a.id - b.id)
      : []

    void decryptVisibleMessages()
  } catch (error) {
    setError(error instanceof Error ? error.message : t('msgUnableToLoadMessages'))
  }
}

async function sendMessage(): Promise<void> {
  const text = draftMessage.value.trim()
  if (!text || !activePeer.value) {
    return
  }
  if (!isE2EEReady()) {
    setError(t('msgE2eeUnavailable'))
    return
  }

  isBusy.value = true

  try {
    const encryptedPayload = await encryptForPeer(text, activePeer.value)

    await requestApi(
      '/api/messages',
      {
        method: 'POST',
        body: JSON.stringify({
          to: activePeer.value,
          messageType: 'text',
          ciphertext: encryptedPayload.ciphertext,
          nonce: encryptedPayload.nonce,
          encryption: 'ecdh-p256/aes-gcm-256',
        }),
      },
      true,
    )

    draftMessage.value = ''
    await loadMessages()
  } catch (error) {
    setError(error instanceof Error ? error.message : t('msgUnableToSendMessage'))
  } finally {
    isBusy.value = false
  }
}

function startPolling(): void {
  stopPolling()
  pollTimer = window.setInterval(() => {
    void loadMessages()
  }, 3000)
}

function stopPolling(): void {
  if (pollTimer !== undefined) {
    window.clearInterval(pollTimer)
    pollTimer = undefined
  }
}

function logout(): void {
  stopPolling()
  clearSession()
  profileAvatarDataUrl.value = ''
  activePage.value = 'landing'
  setInfo(t('msgSessionCleared'))
}

onMounted(() => {
  if (isAuthenticated.value) {
    loadProfileCustomization(currentUser.value)
    startPolling()
  }
})

onBeforeUnmount(() => {
  stopPolling()
})

watch(locale, (value) => {
  localStorage.setItem(STORAGE_KEYS.locale, value)
})

watch(isAuthenticated, (value) => {
  if (value) {
    startPolling()
    return
  }
  stopPolling()
})

watch(currentUser, (value) => {
  if (value) {
    loadProfileCustomization(value)
  }
})
</script>

<template>
  <main class="layout">
    <section class="panel panel--auth">
      <header class="panel__header">
        <p class="eyebrow">anonim</p>
        <h1>{{ t('appTitle') }}</h1>
      </header>

      <label class="field">
        <span>{{ t('language') }}</span>
        <select v-model="locale">
          <option value="en">{{ t('langEn') }}</option>
          <option value="ru">{{ t('langRu') }}</option>
        </select>
      </label>

      <div class="tabs">
        <button :class="{ active: authMode === 'login' }" @click="authMode = 'login'">{{ t('loginTab') }}</button>
        <button :class="{ active: authMode === 'register' }" @click="authMode = 'register'">{{ t('registerTab') }}</button>
      </div>

      <label class="field">
        <span>{{ t('username') }}</span>
        <input v-model="username" autocomplete="username" placeholder="alice" />
      </label>

      <label class="field">
        <span>{{ t('password') }}</span>
        <input v-model="password" type="password" autocomplete="current-password" placeholder="********" />
      </label>

      <label v-if="authMode === 'register'" class="field">
        <span>{{ t('e2eeKeys') }}</span>
        <textarea rows="4" disabled :placeholder="t('e2eePlaceholder')"></textarea>
      </label>

      <button class="btn btn--primary" :disabled="isBusy" @click="authMode === 'login' ? login() : register()">
        {{ isBusy ? t('pleaseWait') : authMode === 'login' ? t('signIn') : t('createAccount') }}
      </button>

      <button v-if="isAuthenticated" class="btn btn--ghost" @click="logout">{{ t('logout') }}</button>

      <p v-if="infoMessage" class="flash flash--info">{{ infoMessage }}</p>
      <p v-if="errorMessage" class="flash flash--error">{{ errorMessage }}</p>
    </section>

    <section class="panel panel--chat">
      <header class="chat-head">
        <h2>{{ t('conversations') }}</h2>
        <p v-if="isAuthenticated">{{ t('signedInAs') }} <strong>{{ visibleName }}</strong> (@{{ currentUser }})</p>
        <p v-else>{{ t('authHint') }}</p>

        <div class="tabs tabs--page">
          <button :class="{ active: activePage === 'landing' }" @click="activePage = 'landing'">{{ t('guideTab') }}</button>
          <button :class="{ active: activePage === 'chat' }" @click="activePage = 'chat'">{{ t('chatTab') }}</button>
          <button :class="{ active: activePage === 'profile' }" @click="activePage = 'profile'">{{ t('profileTab') }}</button>
        </div>
      </header>

      <section v-if="activePage === 'landing'" class="landing-panel">
        <div class="landing-hero">
          <p class="eyebrow">{{ t('welcomeEyebrow') }}</p>
          <h3>{{ t('guideHeroTitle') }}</h3>
          <p>{{ t('guideHeroText') }}</p>
        </div>

        <div class="landing-grid">
          <article class="landing-card">
            <h4>{{ t('howToStart') }}</h4>
            <ol>
              <!-- <li>{{ t('howToStart1') }}</li> -->
              <li>{{ t('howToStart2') }}</li>
              <li>{{ t('howToStart3') }}</li>
              <li>{{ t('howToStart4') }}</li>
            </ol>
          </article>

          <article class="landing-card">
            <h4>{{ t('messengerFeatures') }}</h4>
            <ul>
              <li>{{ t('feature1') }}</li>
              <li>{{ t('feature2') }}</li>
              <li>{{ t('feature3') }}</li>
              <li>{{ t('feature4') }}</li>
            </ul>
          </article>

          <article class="landing-card landing-card--wide">
            <h4>{{ t('encryptionBasics') }}</h4>
            <p>{{ t('encryptionText') }}</p>
            <ul>
              <li>{{ t('encryption1') }}</li>
              <li>{{ t('encryption2') }}</li>
              <li>{{ t('encryption3') }}</li>
              <li>{{ t('encryption4') }}</li>
            </ul>
          </article>
        </div>
      </section>

      <template v-else-if="activePage === 'chat'">
        <div class="peer-input">
          <input
            v-model="peerInput"
            :disabled="!isAuthenticated"
            :placeholder="t('peerUsername')"
            @keydown.enter.prevent="attachPeer"
          />
          <button class="btn btn--accent" :disabled="!isAuthenticated || isBusy" @click="attachPeer">{{ t('open') }}</button>
        </div>

        <div class="peers">
          <button
            v-for="peer in peers"
            :key="peer"
            class="peer-chip"
            :class="{ active: peer === activePeer }"
            @click="activePeer = peer; loadMessages()"
          >
            {{ peer }}
          </button>
        </div>

        <div class="messages" v-if="activePeer">
          <article
            v-for="message in messages"
            :key="message.id"
            class="bubble"
            :class="{ 'bubble--mine': message.from === currentUser }"
          >
            <header>
              <span>{{ message.from }}</span>
              <time>{{ message.timestamp }}</time>
            </header>
            <p>{{ getMessageText(message) }}</p>
            <small>{{ message.encryption }} • nonce {{ message.nonce }}</small>
          </article>
        </div>

        <p v-else class="empty">{{ t('choosePeer') }}</p>

        <form class="composer" @submit.prevent="sendMessage">
          <textarea
            v-model="draftMessage"
            :disabled="!activePeer || !isAuthenticated"
            rows="3"
            :placeholder="t('typeMessage')"
          ></textarea>
          <div class="composer-actions">
            <button class="btn btn--primary" :disabled="!draftMessage.trim() || !activePeer || isBusy">{{ t('send') }}</button>
          </div>
        </form>
      </template>

      <section v-else class="profile-panel">
        <div class="profile-preview">
          <img v-if="profileAvatarDataUrl" :src="profileAvatarDataUrl" class="profile-avatar" alt="profile avatar" />
          <div v-else class="profile-avatar profile-avatar--placeholder">{{ visibleInitial }}</div>
          <div>
            <h3>{{ visibleName }}</h3>
            <p>@{{ currentUser || 'guest' }}</p>
          </div>
        </div>

        <label class="field">
          <span>{{ t('avatarImage') }}</span>
          <input type="file" accept="image/*" :disabled="!isAuthenticated" @change="handleAvatarInput" />
        </label>

        <div class="profile-actions">
          <button class="btn btn--ghost" :disabled="!profileAvatarDataUrl || !isAuthenticated" @click="clearAvatar">{{ t('removeAvatar') }}</button>
          <button class="btn btn--primary" :disabled="!isAuthenticated" @click="saveProfileCustomization">{{ t('saveProfile') }}</button>
          <button class="btn btn--ghost" :disabled="!isAuthenticated" @click="resetProfileCustomization">{{ t('reset') }}</button>
        </div>

        <!-- <p class="profile-note">{{ t('avatarNote') }}</p>   -->
        <p v-if="avatarInputError" class="flash flash--error">{{ avatarInputError }}</p>
      </section>
    </section>
  </main>
</template>
